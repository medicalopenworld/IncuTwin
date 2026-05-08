import { getFirestore, collection, query, where, getDocs } from 'firebase/firestore';
import { app } from '../comun/firebase';
import { INCUTWINS_LOADING, INCUTWINS_SUCCESS, INCUTWINS_FAILED } from './ActionTypes';

export const fetchIncutwins = (uid) => async (dispatch) => {
  dispatch({ type: INCUTWINS_LOADING });
  try {
    const db = getFirestore(app);
    const q = query(collection(db, 'incutwins'), where('propietarioUid', '==', uid));
    const snapshot = await getDocs(q);
    const data = snapshot.docs.map(doc => ({ docId: doc.id, ...doc.data() }));
    dispatch({ type: INCUTWINS_SUCCESS, payload: data });
  } catch (error) {
    dispatch({ type: INCUTWINS_FAILED, payload: error.message });
  }
};
