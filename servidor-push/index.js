const admin = require('firebase-admin');
const { Expo } = require('expo-server-sdk');
const http = require('http');

// =============================================================================
// Inicialización Firebase Admin
// En Render: FIREBASE_SERVICE_ACCOUNT contiene el JSON de la service account
// =============================================================================
const serviceAccount = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT);

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: 'https://incutwinapp-default-rtdb.europe-west1.firebasedatabase.app',
});

const rtdb      = admin.database();
const firestore = admin.firestore();
const expo      = new Expo();

// =============================================================================
// Umbrales de alarma
// =============================================================================
const BPM_MIN  = 110;
const BPM_MAX  = 160;
const TEMP_MIN = 36.0;
const TEMP_MAX = 37.5;

// Estado anterior de cada incubadora (en memoria)
const estadoAnterior = {};

// =============================================================================
// Obtiene el docId de Firestore y el propietarioUid de una incutwin por su id
// =============================================================================
async function getIncutwinFirestore(incubadoraId) {
  const snap = await firestore
    .collection('incutwins')
    .where('id', '==', incubadoraId)
    .limit(1)
    .get();
  if (snap.empty) return null;
  const docSnap = snap.docs[0];
  return { docId: docSnap.id, ...docSnap.data() };
}

// =============================================================================
// Envía una notificación push a un usuario concreto
// =============================================================================
async function enviarNotificacion(uid, titulo, cuerpo, data) {
  const usuarioSnap = await firestore.collection('usuarios').doc(uid).get();
  if (!usuarioSnap.exists) return;

  const token = usuarioSnap.data()?.pushToken;
  if (!token || !Expo.isExpoPushToken(token)) {
    console.warn(`[Push] Token inválido para uid=${uid}`);
    return;
  }

  const mensaje = {
    to: token,
    title: titulo,
    body: cuerpo,
    data,
    sound: 'default',
    priority: 'high',
  };

  try {
    const [ticket] = await expo.sendPushNotificationsAsync([mensaje]);
    if (ticket.status === 'error') {
      console.error(`[Push] Error ticket: ${ticket.message}`);
      // Token inválido (app desinstalada) → borrar de Firestore
      if (ticket.details?.error === 'DeviceNotRegistered') {
        await firestore.collection('usuarios').doc(uid).update({ pushToken: admin.firestore.FieldValue.delete() });
        console.log(`[Push] Token inválido eliminado para uid=${uid}`);
      }
    } else {
      console.log(`[Push] OK → ${titulo} para uid=${uid}`);
    }
  } catch (e) {
    console.error('[Push] Error enviando:', e.message);
  }
}

// =============================================================================
// Evalúa qué notificaciones lanzar al detectar un cambio en una incubadora
// =============================================================================
async function evaluarCambio(incubadoraId, nuevo) {
  const anterior = estadoAnterior[incubadoraId];
  if (!anterior) return; // primera carga, sin estado previo

  const incutwin = await getIncutwinFirestore(incubadoraId);
  if (!incutwin?.propietarioUid) return;

  const uid     = incutwin.propietarioUid;
  const nombre  = incutwin.nombre ?? incubadoraId;
  const navData = { incutwinId: incutwin.docId, incubadoraId };

  // --- Desconexión ---
  if (anterior.enLinea && !nuevo.enLinea) {
    await enviarNotificacion(uid, 'Incutwin desconectada', `${nombre} ha perdido la conexión.`, navData);
    return;
  }

  if (!nuevo.enLinea || !nuevo.conBebe) return;

  // --- BPM ---
  if (nuevo.bpm > 0) {
    const bpmAntes = anterior.bpm ?? 0;
    if (nuevo.bpm < BPM_MIN && bpmAntes >= BPM_MIN) {
      await enviarNotificacion(uid, '⚠️ Latido bajo', `${nombre}: ${nuevo.bpm} bpm (mín ${BPM_MIN})`, navData);
    } else if (nuevo.bpm > BPM_MAX && bpmAntes <= BPM_MAX) {
      await enviarNotificacion(uid, '⚠️ Latido alto', `${nombre}: ${nuevo.bpm} bpm (máx ${BPM_MAX})`, navData);
    }
  }

  // --- Temperatura ---
  if (nuevo.temperatura > 0) {
    const tempAntes = anterior.temperatura ?? 0;
    if (nuevo.temperatura < TEMP_MIN && tempAntes >= TEMP_MIN) {
      await enviarNotificacion(uid, '⚠️ Temperatura baja', `${nombre}: ${nuevo.temperatura}°C (mín ${TEMP_MIN})`, navData);
    } else if (nuevo.temperatura > TEMP_MAX && tempAntes <= TEMP_MAX) {
      await enviarNotificacion(uid, '⚠️ Temperatura alta', `${nombre}: ${nuevo.temperatura}°C (máx ${TEMP_MAX})`, navData);
    }
  }
}

// =============================================================================
// Listener principal — escucha cambios en /incutwins en tiempo real
// =============================================================================
rtdb.ref('incutwins').on('child_changed', async (snapshot) => {
  const id    = snapshot.key;
  const nuevo = snapshot.val();

  try {
    await evaluarCambio(id, nuevo);
  } catch (e) {
    console.error(`[RTDB] Error procesando ${id}:`, e.message);
  } finally {
    estadoAnterior[id] = nuevo;
  }
});

// Carga el estado inicial para tener referencia en el primer cambio
rtdb.ref('incutwins').once('value', (snapshot) => {
  const data = snapshot.val() ?? {};
  Object.entries(data).forEach(([key, val]) => {
    estadoAnterior[key] = val;
  });
  console.log(`[BOOT] Estado inicial cargado: ${Object.keys(data).length} incutwins`);
});

// =============================================================================
// Servidor HTTP mínimo — Render necesita un puerto abierto para health checks
// =============================================================================
const PORT = process.env.PORT || 3000;
http.createServer((req, res) => res.end('IncuTwin Push Server OK')).listen(PORT, () => {
  console.log(`[HTTP] Escuchando en puerto ${PORT}`);
});
