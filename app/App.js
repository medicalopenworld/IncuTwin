import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Provider as PaperProvider } from 'react-native-paper';
import { NavigationContainer } from '@react-navigation/native';
import { createNativeStackNavigator } from '@react-navigation/native-stack';
import { Provider } from 'react-redux';
import { ConfigureStore } from './redux/configureStore';
import LoginPantalla from './pantallas/LoginPantalla';
import RegistroPantalla from './pantallas/RegistroPantalla';
import PrincipalPantalla from './pantallas/PrincipalPantalla';
import DetallePantalla from './pantallas/DetallePantalla';
import PerfilPantalla from './pantallas/PerfilPantalla';

const Stack = createNativeStackNavigator();
const store = ConfigureStore();

export default function App() {
  return (
    <Provider store={store}>
      <SafeAreaProvider>
        <PaperProvider>
          <NavigationContainer>
            <Stack.Navigator screenOptions={{ headerShown: false }}>
              <Stack.Screen name="Login" component={LoginPantalla} />
              <Stack.Screen name="Registro" component={RegistroPantalla} />
              <Stack.Screen name="Principal" component={PrincipalPantalla} />
              <Stack.Screen name="Detalle" component={DetallePantalla} />
              <Stack.Screen name="Perfil" component={PerfilPantalla} />
            </Stack.Navigator>
          </NavigationContainer>
          <StatusBar style="light" />
        </PaperProvider>
      </SafeAreaProvider>
    </Provider>
  );
}
