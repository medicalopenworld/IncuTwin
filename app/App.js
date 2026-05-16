import { useEffect, useState } from 'react';
import { ActivityIndicator, View } from 'react-native';
import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Provider as PaperProvider } from 'react-native-paper';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { Provider } from 'react-redux';
import { onAuthStateChanged } from 'firebase/auth';
import * as Notifications from 'expo-notifications';
import { ConfigureStore } from './redux/configureStore';
import { auth } from './comun/firebase';
import { colorAcento, colorPrimario } from './comun/comun';

Notifications.setNotificationHandler({
  handleNotification: async () => ({
    shouldShowAlert: true,
    shouldPlaySound: true,
    shouldSetBadge: false,
  }),
});
import LoginPantalla from './pantallas/LoginPantalla';
import RegistroPantalla from './pantallas/RegistroPantalla';
import PrincipalPantalla from './pantallas/PrincipalPantalla';
import DetallePantalla from './pantallas/DetallePantalla';
import PerfilPantalla from './pantallas/PerfilPantalla';
import EscaneoPantalla from './pantallas/EscaneoPantalla';

const Stack = createNativeStackNavigator();
const store = ConfigureStore();

export default function App() {
  const [usuario, setUsuario] = useState(undefined);

  useEffect(() => {
    Notifications.requestPermissionsAsync();
    const unsuscribir = onAuthStateChanged(auth, (user) => {
      setUsuario(user ?? null);
    });
    return unsuscribir;
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
          <NavigationContainer>
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
