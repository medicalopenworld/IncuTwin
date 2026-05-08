import { StyleSheet, Text, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import {
  colorAcentoClaro,
  colorAlerta,
  colorPrimario,
  colorTextoSecundario,
} from '../comun/comun';

export default function PrincipalPantalla() {
  const insets = useSafeAreaInsets();

  return (
    <View style={[styles.contenedor, { paddingTop: insets.top }]}>
      <Text style={styles.emoji}>❤️</Text>
      <Text style={styles.titulo}>IncuTwin</Text>
      <Text style={styles.subtitulo}>Bienvenido/a</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  contenedor: {
    flex: 1,
    backgroundColor: colorPrimario,
    alignItems: 'center',
    justifyContent: 'center',
    gap: 12,
  },
  emoji: {
    fontSize: 64,
    color: colorAlerta,
  },
  titulo: {
    fontSize: 32,
    fontWeight: 'bold',
    color: 'white',
  },
  subtitulo: {
    fontSize: 16,
    color: colorAcentoClaro,
  },
});
