import { useEffect, useRef, useState } from 'react';
import { ActivityIndicator, View } from 'react-native';
import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Provider as PaperProvider } from 'react-native-paper';
import { NavigationContainer, createNavigationContainerRef } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { Provider } from 'react-redux';
import { onAuthStateChanged } from 'firebase/auth';
import * as Notifications from 'expo-notifications';
import { ConfigureStore } from './redux/configureStore';
import { auth } from './comun/firebase';
import { colorAcento, colorPrimario } from './comun/comun';
import { registrarTokenPush } from './comun/notificaciones';

import LoginPantalla from './pantallas/LoginPantalla';
import RegistroPantalla from './pantallas/RegistroPantalla';
import PrincipalPantalla from './pantallas/PrincipalPantalla';
import DetallePantalla from './pantallas/DetallePantalla';
import PerfilPantalla from './pantallas/PerfilPantalla';
import EscaneoPantalla from './pantallas/EscaneoPantalla';

Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowBanner: true,
    shouldShowList: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});

const Stack = createNativeStackNavigator();
const store = ConfigureStore();
export const navigationRef = createNavigationContainerRef();

export default function App() {
  const [usuario, setUsuario] = useState(undefined);
  const notifListener = useRef();
  const responseListener = useRef();

  useEffect(() => {
    Notifications.requestPermissionsAsync();

    const unsuscribir = onAuthStateChanged(auth, (user) => {
      setUsuario(user ?? null);
      if (user) registrarTokenPush(user.uid);
    });

    // Tap en notificación → navegar a la incutwin correspondiente
    responseListener.current = Notifications.addNotificationResponseReceivedListener((response) => {
      const data = response.notification.request.content.data;
      if (data?.incutwinId && navigationRef.isReady()) {
        navigationRef.navigate('Detalle', {
          incutwinId: data.incutwinId,
          incubadoraId: data.incubadoraId,
        });
      }
    });

    return () => {
      unsuscribir();
      responseListener.current?.remove();
    };
  }, []);

  if (usuario === undefined) {
    return (
      <View style={{ flex: 1, backgroundColor: colorPrimario, justifyContent: 'center', alignItems: 'center' }}>
        <ActivityIndicator size="large" color={colorAcento} />
        <StatusBar style="dark" />
      </View>
    );
  }

  return (
    <Provider store={store}>
      <SafeAreaProvider>
        <PaperProvider>
          <NavigationContainer ref={navigationRef}>
            <Stack.Navigator
              screenOptions={{ headerShown: false }}
              initialRouteName={usuario ? 'Principal' : 'Login'}
            >
              <Stack.Screen name="Login" component={LoginPantalla} />
              <Stack.Screen name="Registro" component={RegistroPantalla} />
              <Stack.Screen name="Principal" component={PrincipalPantalla} />
              <Stack.Screen name="Detalle" component={DetallePantalla} />
              <Stack.Screen name="Perfil" component={PerfilPantalla} />
              <Stack.Screen name="Escaneo" component={EscaneoPantalla} />
            </Stack.Navigator>
          </NavigationContainer>
          <StatusBar style="dark" />
        </PaperProvider>
      </SafeAreaProvider>
    </Provider>
  );
}
