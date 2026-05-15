import { Component } from 'react';
import { ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { connect } from 'react-redux';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import {
  colorAcento,
  colorAcentoClaro,
  colorAlerta,
  colorBlancoFondo,
  colorPrimario,
  colorPrimarioMedio,
  colorTextoSecundario,
} from '../comun/comun';

const mapStateToProps = (state, ownProps) => ({
  incutwin: state.incutwins.incutwins.find(
    (i) => i.docId === ownProps.route.params.incutwinId
  ),
});

class DetallePantalla extends Component {
  render() {
    const { navigation, incutwin } = this.props;

    if (!incutwin) {
      return (
        <View style={styles.centrado}>
          <Text style={styles.errorTexto}>Incutwin no encontrada</Text>
        </View>
      );
    }

    return (
      <ScrollView style={styles.fondo} contentContainerStyle={styles.contenedor}>

        {/* Botón volver */}
        <TouchableOpacity style={styles.volver} onPress={() => navigation.goBack()}>
          <MaterialCommunityIcons name="arrow-left" size={22} color="white" />
          <Text style={styles.volverTexto}>Volver</Text>
        </TouchableOpacity>

        {/* Nombre + badge estado */}
        <View style={styles.tituloFila}>
          <Text style={styles.nombre}>{incutwin.nombre}</Text>
          <View style={[styles.badge, incutwin.enLinea ? styles.badgeVerde : styles.badgeGris]}>
            <Text style={styles.badgeTexto}>
              {incutwin.enLinea ? 'En línea' : 'Sin conexión'}
            </Text>
          </View>
        </View>

        <Text style={styles.incubadoraId}>{incutwin.incubadoraId?.toUpperCase()}</Text>

        {/* Tarjeta info general */}
        <View style={styles.tarjeta}>
          <Text style={styles.seccionTitulo}>Información general</Text>

          <FilaInfo
            icono="wifi"
            iconoColor={incutwin.enLinea ? colorAcentoClaro : colorTextoSecundario}
            label="Estado"
            valor={incutwin.enLinea ? 'Conectada' : 'Desconectada'}
          />
          <FilaInfo icono="hospital-building" label="Hospital" valor={incutwin.hospital} />
          <FilaInfo icono="city" label="Ciudad" valor={incutwin.ciudad} />
          <FilaInfo icono="earth" label="País" valor={incutwin.pais} />

          <View style={styles.filaInfo}>
            <MaterialCommunityIcons name="led-on" size={18} color={colorTextoSecundario} style={styles.filaIcono} />
            <Text style={styles.filaLabel}>Color LED</Text>
            <View style={styles.filaValorFila}>
              <View style={[styles.colorCirculo, { backgroundColor: incutwin.colorLED ?? '#888' }]} />
              <Text style={styles.filaValor}>{incutwin.colorLED ?? '—'}</Text>
            </View>
          </View>
        </View>

        {/* Tarjeta telemetría en tiempo real */}
        <View style={styles.tarjeta}>
          <Text style={styles.seccionTitulo}>Telemetría en tiempo real</Text>

          <View style={styles.filaInfo}>
            <MaterialCommunityIcons
              name="heart-pulse"
              size={18}
              color={incutwin.bpm > 0 ? colorAlerta : colorTextoSecundario}
              style={styles.filaIcono}
            />
            <Text style={styles.filaLabel}>Frecuencia cardíaca</Text>
            <Text style={[styles.filaValor, incutwin.bpm > 0 && { color: colorAlerta }]}>
              {incutwin.bpm > 0 ? `${incutwin.bpm} bpm` : '—'}
            </Text>
          </View>

          <FilaInfo
            icono="thermometer"
            iconoColor={colorAcento}
            label="Temperatura"
            valor={incutwin.temperatura > 0 ? `${incutwin.temperatura} °C` : '—'}
          />

          <View style={styles.filaInfo}>
            <MaterialCommunityIcons
              name="hand-wave"
              size={18}
              color={incutwin.holdDetected ? colorAcentoClaro : colorTextoSecundario}
              style={styles.filaIcono}
            />
            <Text style={styles.filaLabel}>Detección de mano</Text>
            <Text style={[styles.filaValor, incutwin.holdDetected && { color: colorAcentoClaro }]}>
              {incutwin.holdDetected ? 'Detectada' : 'No detectada'}
            </Text>
          </View>
        </View>

        {/* Tarjeta bebé */}
        <View style={styles.tarjeta}>
          <Text style={styles.seccionTitulo}>Bebé</Text>

          {incutwin.conBebe ? (
            <>
              <FilaInfo icono="baby-face-outline" iconoColor={colorAlerta} label="Nombre" valor={incutwin.bebe?.nombre} />
              <FilaInfo icono="timer-sand" label="Semanas" valor={`${incutwin.bebe?.semanas ?? '?'} semanas`} />
              <FilaInfo icono="calendar" label="Fecha ingreso" valor={incutwin.bebe?.fechaIngreso} />
            </>
          ) : (
            <View style={styles.sinBebeContenedor}>
              <MaterialCommunityIcons name="baby-buggy-off" size={36} color={colorTextoSecundario} />
              <Text style={styles.sinBebeTexto}>Sin bebé asignado</Text>
            </View>
          )}
        </View>

      </ScrollView>
    );
  }
}

function FilaInfo({ icono, iconoColor = colorTextoSecundario, label, valor }) {
  return (
    <View style={styles.filaInfo}>
      <MaterialCommunityIcons name={icono} size={18} color={iconoColor} style={styles.filaIcono} />
      <Text style={styles.filaLabel}>{label}</Text>
      <Text style={styles.filaValor}>{valor ?? '—'}</Text>
    </View>
  );
}

export default connect(mapStateToProps)(DetallePantalla);

const styles = StyleSheet.create({
  fondo: {
    flex: 1,
    backgroundColor: colorPrimario,
  },
  contenedor: {
    paddingHorizontal: 20,
    paddingTop: 60,
    paddingBottom: 40,
  },
  centrado: {
    flex: 1,
    backgroundColor: colorPrimario,
    justifyContent: 'center',
    alignItems: 'center',
  },
  errorTexto: {
    color: colorTextoSecundario,
    fontSize: 15,
  },

  // Volver
  volver: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    marginBottom: 28,
  },
  volverTexto: {
    color: 'white',
    fontSize: 16,
  },

  // Título
  tituloFila: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 10,
    flexWrap: 'wrap',
    marginBottom: 4,
  },
  nombre: {
    fontSize: 24,
    fontWeight: 'bold',
    color: 'white',
    flexShrink: 1,
  },
  incubadoraId: {
    fontSize: 13,
    color: colorAcento,
    marginBottom: 24,
    letterSpacing: 0.5,
  },

  // Badge
  badge: {
    borderRadius: 20,
    paddingHorizontal: 10,
    paddingVertical: 4,
  },
  badgeVerde: { backgroundColor: '#064E3B' },
  badgeGris: { backgroundColor: '#1E293B' },
  badgeTexto: {
    fontSize: 11,
    color: colorBlancoFondo,
    fontWeight: '600',
  },

  // Tarjeta
  tarjeta: {
    backgroundColor: colorPrimarioMedio,
    borderRadius: 14,
    padding: 16,
    marginBottom: 16,
    gap: 14,
  },
  seccionTitulo: {
    fontSize: 13,
    fontWeight: '600',
    color: colorTextoSecundario,
    textTransform: 'uppercase',
    letterSpacing: 0.8,
    marginBottom: 2,
  },

  // Fila info
  filaInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 10,
  },
  filaIcono: {
    width: 22,
  },
  filaLabel: {
    flex: 1,
    color: colorTextoSecundario,
    fontSize: 14,
  },
  filaValor: {
    color: 'white',
    fontSize: 14,
    fontWeight: '500',
    textAlign: 'right',
    flexShrink: 1,
  },
  filaValorFila: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  colorCirculo: {
    width: 16,
    height: 16,
    borderRadius: 8,
  },

  // Sin bebé
  sinBebeContenedor: {
    alignItems: 'center',
    paddingVertical: 16,
    gap: 8,
  },
  sinBebeTexto: {
    color: colorTextoSecundario,
    fontSize: 14,
    fontStyle: 'italic',
  },
});
