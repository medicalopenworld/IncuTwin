import { getFirestore, collection, query, where, getDocs } from 'firebase/firestore';
import { ref, onValue } from 'firebase/database';
import { app, database } from '../comun/firebase';
import { INCUTWINS_LOADING, INCUTWINS_SUCCESS, INCUTWINS_FAILED, INCUTWIN_REALTIME_UPDATE } from './ActionTypes';

let _unsubscribers = [];

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

  incutwins.forEach((incutwin) => {
    const dbRef = ref(database, `incutwins/${incutwin.id}`);
    const unsub = onValue(dbRef, (snapshot) => {
      if (snapshot.exists()) {
        dispatch({
          type: INCUTWIN_REALTIME_UPDATE,
          payload: { incutwinId: incutwin.id, data: snapshot.val() },
        });
      }
    });
    _unsubscribers.push(unsub);
  });
};

export const unsubscribeRealtime = () => () => {
  _unsubscribers.forEach((unsub) => unsub());
  _unsubscribers = [];
};
