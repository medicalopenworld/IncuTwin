import { useRef, useState } from 'react';
import { Alert, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { CameraView, useCameraPermissions } from 'expo-camera';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import { collection, doc, getDocs, getFirestore, query, updateDoc, where } from 'firebase/firestore';
import * as Notifications from 'expo-notifications';
import { app, auth } from '../comun/firebase';
import {
  colorAcento,
  colorPrimario,
  colorTexto,
  colorTextoSecundario,
} from '../comun/comun';

Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowAlert: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});

export default function EscaneoPantalla({ navigation }) {
  const [permission, requestPermission] = useCameraPermissions();
  const [procesando, setProcesando] = useState(false);
  const yaEscaneado = useRef(false);

  const handleScan = async ({ data }) => {
    if (yaEscaneado.current) return;
    yaEscaneado.current = true;
    setProcesando(true);

    try {
      const db = getFirestore(app);
      const uid = auth.currentUser?.uid;

      const q = query(collection(db, 'incutwins'), where('id', '==', data));
      const snapshot = await getDocs(q);

      if (snapshot.empty) {
        Alert.alert('QR no válido', 'No se encontró ninguna incutwin con ese código.', [
          { text: 'Reintentar', onPress: () => { yaEscaneado.current = false; setProcesando(false); } },
        ]);
        return;
      }

      const incutwinDoc = snapshot.docs[0];
      const incutwinData = incutwinDoc.data();

      if (incutwinData.propietarioUid === uid) {
        Alert.alert('Ya la tienes', 'Esta incutwin ya está en tu lista.', [
          { text: 'OK', onPress: () => navigation.goBack() },
        ]);
        return;
      }

      await updateDoc(doc(db, 'incutwins', incutwinDoc.id), { propietarioUid: uid });

      await Notifications.scheduleNotificationAsync({
        content: {
          title: '¡Incutwin añadida!',
          body: `${incutwinData.nombre} ha sido añadida a tu lista.`,
        },
        trigger: null,
      });

      navigation.navigate('Principal', { refreshAt: Date.now() });
    } catch (e) {
      Alert.alert('Error', e.message ?? 'No se pudo añadir la incutwin.');
      yaEscaneado.current = false;
      setProcesando(false);
    }
  };

  if (!permission) return <View style={styles.centrado} />;

  if (!permission.granted) {
    return (
      <View style={styles.centrado}>
        <MaterialCommunityIcons name="camera-off" size={48} color={colorTextoSecundario} />
        <Text style={styles.permisosTexto}>Se necesita acceso a la cámara</Text>
        <TouchableOpacity style={styles.boton} onPress={requestPermission}>
          <Text style={styles.botonTexto}>Permitir cámara</Text>
        </TouchableOpacity>
        <TouchableOpacity onPress={() => navigation.goBack()}>
          <Text style={styles.cancelarTexto}>Cancelar</Text>
        </TouchableOpacity>
      </View>
    );
  }

  return (
    <View style={styles.flex}>
      <CameraView
        style={StyleSheet.absoluteFillObject}
        facing="back"
        barcodeScannerSettings={{ barcodeTypes: ['qr'] }}
        onBarcodeScanned={procesando ? undefined : handleScan}
      />

      <View style={styles.overlay}>
        <TouchableOpacity style={styles.cerrar} onPress={() => navigation.goBack()}>
          <MaterialCommunityIcons name="close" size={28} color="white" />
        </TouchableOpacity>

        <View style={styles.marco} />

        <Text style={styles.instruccion}>
          {procesando ? 'Procesando...' : 'Apunta al QR de la incutwin'}
        </Text>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  flex: { flex: 1 },

  centrado: {
    flex: 1,
    backgroundColor: colorPrimario,
    justifyContent: 'center',
    alignItems: 'center',
    gap: 16,
    paddingHorizontal: 32,
  },
  permisosTexto: {
    color: colorTexto,
    fontSize: 16,
    textAlign: 'center',
  },
  boton: {
    backgroundColor: colorAcento,
    borderRadius: 10,
    paddingVertical: 12,
    paddingHorizontal: 24,
  },
  botonTexto: {
    color: 'white',
    fontSize: 15,
    fontWeight: '600',
  },
  cancelarTexto: {
    color: colorTextoSecundario,
    fontSize: 14,
  },

  overlay: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'space-between',
    paddingTop: 60,
    paddingBottom: 80,
  },
  cerrar: {
    alignSelf: 'flex-end',
    marginRight: 20,
    backgroundColor: 'rgba(0,0,0,0.45)',
    borderRadius: 20,
    padding: 6,
  },
  marco: {
    width: 240,
    height: 240,
    borderWidth: 3,
    borderColor: colorAcento,
    borderRadius: 16,
    backgroundColor: 'transparent',
  },
  instruccion: {
    color: 'white',
    fontSize: 15,
    backgroundColor: 'rgba(0,0,0,0.5)',
    paddingHorizontal: 20,
    paddingVertical: 10,
    borderRadius: 20,
    overflow: 'hidden',
  },
});
