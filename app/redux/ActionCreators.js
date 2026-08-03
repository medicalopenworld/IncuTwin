import { getFirestore, collection, query, where, getDocs, doc, updateDoc, deleteField } from 'firebase/firestore';
import { ref, onValue } from 'firebase/database';
import { app, database } from '../comun/firebase';
import { INCUTWINS_LOADING, INCUTWINS_SUCCESS, INCUTWINS_FAILED, INCUTWIN_REALTIME_UPDATE, INCUTWIN_REMOVE } from './ActionTypes';

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

export const subscribeRealtime = (incutwins) => (dispatch) => {
  _unsubscribers.forEach((unsub) => unsub());
  _unsubscribers = [];
  _initialized.clear();

  incutwins.forEach((incutwin) => {
    const dbRef = ref(database, `incutwins/${incutwin.id}`);
    const unsub = onValue(dbRef, (snapshot) => {
      if (!snapshot.exists()) return;
      const newData = snapshot.val();

      if (!_initialized.has(incutwin.id)) {
        _initialized.add(incutwin.id);
      }

      dispatch({ type: INCUTWIN_REALTIME_UPDATE, payload: { incutwinId: incutwin.id, data: newData } });
    });
    _unsubscribers.push(unsub);
  });
};

export const unsubscribeRealtime = () => () => {
  _unsubscribers.forEach((unsub) => unsub());
  _unsubscribers = [];
  _initialized.clear();
};

export const removeIncutwin = (docId) => (dispatch) => {
  dispatch({ type: INCUTWIN_REMOVE, payload: docId });
  const db = getFirestore(app);
  updateDoc(doc(db, 'incutwins', docId), { propietarioUid: deleteField() }).catch(() => {});
};
