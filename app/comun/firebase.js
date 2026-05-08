import { initializeApp } from 'firebase/app';
import { getAuth } from 'firebase/auth';

const firebaseConfig = {
  apiKey: 'AIzaSyANwzDWwczsHJgOa3EDcAoLpTVrv7YvHYI',
  authDomain: 'incutwinapp.firebaseapp.com',
  databaseURL: 'https://incutwinapp-default-rtdb.europe-west1.firebasedatabase.app',
  projectId: 'incutwinapp',
  storageBucket: 'incutwinapp.firebasestorage.app',
  messagingSenderId: '937391136242',
  appId: '1:937391136242:web:f4357d0fe6d630d483df6b',
};

const app = initializeApp(firebaseConfig);
export const auth = getAuth(app);
