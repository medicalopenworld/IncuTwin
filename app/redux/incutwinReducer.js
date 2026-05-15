import { INCUTWINS_LOADING, INCUTWINS_SUCCESS, INCUTWINS_FAILED, INCUTWIN_REALTIME_UPDATE } from './ActionTypes';

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
    case INCUTWIN_REALTIME_UPDATE:
      return {
        ...state,
        incutwins: state.incutwins.map((item) =>
          item.id === action.payload.incutwinId
            ? { ...item, ...action.payload.data }
            : item
        ),
      };
    default:
      return state;
  }
};
