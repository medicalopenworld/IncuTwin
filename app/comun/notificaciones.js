import * as Notifications from 'expo-notifications';
import Constants from 'expo-constants';
import { Platform } from 'react-native';
import { getFirestore, doc, setDoc, serverTimestamp } from 'firebase/firestore';
import { app } from './firebase';

export async function registrarTokenPush(uid) {
  if (!uid) return;

  // Solo dispositivos físicos reciben push notifications reales
  if (!Constants.isDevice) return;

  const { status } = await Notifications.getPermissionsAsync();
  if (status !== 'granted') return;

  // Android necesita canal de notificación
  if (Platform.OS === 'android') {
    await Notifications.setNotificationChannelAsync('default', {
      name: 'default',
      importance: Notifications.AndroidImportance.MAX,
      vibrationPattern: [0, 250, 250, 250],
    });
  }

  try {
    const projectId = Constants.expoConfig?.extra?.eas?.projectId;
    const tokenData = await Notifications.getExpoPushTokenAsync(
      projectId ? { projectId } : undefined
    );
    const token = tokenData.data;

    const db = getFirestore(app);
    await setDoc(
      doc(db, 'usuarios', uid),
      { pushToken: token, tokenActualizadoEn: serverTimestamp() },
      { merge: true }
    );
  } catch (e) {
    console.warn('[Push] No se pudo registrar el token:', e.message);
  }
}
