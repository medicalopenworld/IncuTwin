import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { Provider as PaperProvider } from 'react-native-paper';
import LoginPantalla from './pantallas/LoginPantalla';

export default function App() {
  return (
    <SafeAreaProvider>
      <PaperProvider>
        <LoginPantalla />
        <StatusBar style="light" />
      </PaperProvider>
    </SafeAreaProvider>
  );
}
