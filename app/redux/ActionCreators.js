import { getFirestore, collection, query, where, getDocs } from 'firebase/firestore';
import { ref, onValue } from 'firebase/database';
import * as Notifications from 'expo-notifications';
import { app, database } from '../comun/firebase';
import { INCUTWINS_LOADING, INCUTWINS_SUCCESS, INCUTWINS_FAILED, INCUTWIN_REALTIME_UPDATE } from './ActionTypes';

const BPM_MIN = 100;
const BPM_MAX = 180;

const notify = (title, body) => {
  Notifications.scheduleNotificationAsync({
    content: { title, body },
    trigger: null,
  }).catch(() => {});
};

let _unsubscribers = [];
const _initialized = new Set();

export const fetchIncutwins = (uid) => async (dispatch) => {
  dispatch({ type: INCUTWINS_LOADING });
  try {
    const db = getFirestore(app);
    const q = query(collection(db, 'incutwins'), where('propietarioUid', '==', uid));
    const snapshot = await getDocs(q);
    const data = snapshot.docs.map((doc) => ({ docId: doc.id, ...doc.data() }));
    dispatch({ type: INCUTWINS_SUCCESS, payload: data });
    return data;
  } catch (error) {
    dispatch({ type: INCUTWINS_FAILED, payload: error.message });
  }
};

export const subscribeRealtime = (incutwins) => (dispatch, getState) => {
  _unsubscribers.forEach((unsub) => unsub());
  _unsubscribers = [];
  _initialized.clear();

  incutwins.forEach((incutwin) => {
    const dbRef = ref(database, `incutwins/${incutwin.id}`);
    const unsub = onValue(dbRef, (snapshot) => {
      if (!snapshot.exists()) return;
      const newData = snapshot.val();

      // Primera llegada: inicializar sin notificar
      if (!_initialized.has(incutwin.id)) {
        _initialized.add(incutwin.id);
        dispatch({ type: INCUTWIN_REALTIME_UPDATE, payload: { incutwinId: incutwin.id, data: newData } });
        return;
      }

      const old = getState().incutwins.incutwins.find((i) => i.id === incutwin.id);
      dispatch({ type: INCUTWIN_REALTIME_UPDATE, payload: { incutwinId: incutwin.id, data: newData } });

      if (!old) return;
      const nombre = old.nombre ?? incutwin.id;

      // Conexión / desconexión
      if (!old.enLinea && newData.enLinea)
        notify('Incutwin conectada', `${nombre} está ahora en línea.`);
      else if (old.enLinea && !newData.enLinea)
        notify('Incutwin desconectada', `${nombre} ha perdido la conexión.`);

      // Bebé entra / sale
      if (!old.conBebe && newData.conBebe)
        notify('Bebé detectado', `Se ha asignado un bebé a ${nombre}.`);
      else if (old.conBebe && !newData.conBebe)
        notify('Bebé retirado', `El bebé ha sido retirado de ${nombre}.`);

      // Alarma BPM
      if (newData.enLinea && newData.conBebe && newData.bpm > 0) {
        if (newData.bpm < BPM_MIN && (old.bpm >= BPM_MIN || !old.enLinea || !old.conBebe))
          notify('⚠️ Latido bajo', `${nombre}: ${newData.bpm} bpm (umbral ${BPM_MIN}).`);
        else if (newData.bpm > BPM_MAX && (old.bpm <= BPM_MAX || !old.enLinea || !old.conBebe))
          notify('⚠️ Latido alto', `${nombre}: ${newData.bpm} bpm (umbral ${BPM_MAX}).`);
      }

      // Mano detectada → mensaje de cariño
      if (!old.holdDetected && newData.holdDetected && newData.conBebe) {
        const bebe = old.bebe?.nombre ?? 'tu bebé';
        const pais = old.pais ? ` en ${old.pais}` : '';
        notify('💛 Mensaje de cariño', `Acabas de mandar un mensaje de cariño a ${bebe}${pais}.`);
      }
    });
    _unsubscribers.push(unsub);
  });
};

export const unsubscribeRealtime = () => () => {
  _unsubscribers.forEach((unsub) => unsub());
  _unsubscribers = [];
  _initialized.clear();
};
