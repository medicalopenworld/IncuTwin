import { StyleSheet, Text, View } from 'react-native';
import { colorAcento, colorPrimario, colorTextoSecundario } from '../comun/comun';

export default function DetallePantalla({ route }) {
  const { incutwinId, incubadoraId } = route.params;

  return (
    <View style={styles.contenedor}>
      <Text style={styles.id}>{(incubadoraId ?? incutwinId).toUpperCase()}</Text>
      <Text style={styles.proximamente}>Detalle próximamente</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  contenedor: {
    flex: 1,
    backgroundColor: colorPrimario,
    justifyContent: 'center',
    alignItems: 'center',
    gap: 12,
  },
  id: {
    fontSize: 20,
    fontWeight: 'bold',
    color: colorAcento,
  },
  proximamente: {
    fontSize: 15,
    color: colorTextoSecundario,
  },
});
