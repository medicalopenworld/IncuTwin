import { initializeApp, getApps, getApp } from 'firebase/app';
import { initializeAuth, getAuth, getReactNativePersistence } from 'firebase/auth';
import ReactNativeAsyncStorage from '@react-native-async-storage/async-storage';

const firebaseConfig = {
  apiKey: 'AIzaSyANwzDWwczsHJgOa3EDcAoLpTVrv7YvHYI',
  authDomain: 'incutwinapp.firebaseapp.com',
  databaseURL: 'https://incutwinapp-default-rtdb.europe-west1.firebasedatabase.app',
  projectId: 'incutwinapp',
  storageBucket: 'incutwinapp.firebasestorage.app',
  messagingSenderId: '937391136242',
  appId: '1:937391136242:web:f4357d0fe6d630d483df6b',
};

export const app = getApps().length === 0 ? initializeApp(firebaseConfig) : getApp();

export const auth = (() => {
  try {
    return initializeAuth(app, {
      persistence: getReactNativePersistence(ReactNativeAsyncStorage),
    });
  } catch {
    return getAuth(app);
  }
})();
