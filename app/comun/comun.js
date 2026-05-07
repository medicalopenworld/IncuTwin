// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
import { getAnalytics } from "firebase/analytics";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
// For Firebase JS SDK v7.20.0 and later, measurementId is optional
const firebaseConfig = {
    apiKey: "AIzaSyANwzDWwczsHJgOa3EDcAoLpTVrv7YvHYI",
    authDomain: "incutwinapp.firebaseapp.com",
    databaseURL: "https://incutwinapp-default-rtdb.europe-west1.firebasedatabase.app",
    projectId: "incutwinapp",
    storageBucket: "incutwinapp.firebasestorage.app",
    messagingSenderId: "937391136242",
    appId: "1:937391136242:web:f4357d0fe6d630d483df6b",
    measurementId: "G-9SGMB6JHWJ"
};

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const analytics = getAnalytics(app);