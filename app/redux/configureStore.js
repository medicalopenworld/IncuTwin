import { configureStore } from '@reduxjs/toolkit';
import { incutwinReducer } from './incutwinReducer';

export const ConfigureStore = () => {
  const store = configureStore({
    reducer: {
      incutwins: incutwinReducer,
    },
  });
  return store;
};
