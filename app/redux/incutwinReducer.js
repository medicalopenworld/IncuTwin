import { INCUTWINS_LOADING, INCUTWINS_SUCCESS, INCUTWINS_FAILED } from './ActionTypes';

const initialState = {
  isLoading: false,
  errMess: null,
  incutwins: [],
};

export const incutwinReducer = (state = initialState, action) => {
  switch (action.type) {
    case INCUTWINS_LOADING:
      return { ...state, isLoading: true, errMess: null };
    case INCUTWINS_SUCCESS:
      return { ...state, isLoading: false, errMess: null, incutwins: action.payload };
    case INCUTWINS_FAILED:
      return { ...state, isLoading: false, errMess: action.payload };
    default:
      return state;
  }
};
