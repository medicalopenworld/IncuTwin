import { Component } from 'react';
import { Animated, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
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
  colorTexto,
} from '../comun/comun';

const mapStateToProps = (state, ownProps) => ({
  incutwin: state.incutwins.incutwins.find(
    (i) => i.docId === ownProps.route.params.incutwinId
  ),
});

class DetallePantalla extends Component {
  _escala = new Animated.Value(1);
  _animacion = null;

  componentDidMount() {
    this._iniciarLatido(this.props.incutwin?.bpm);
  }

  componentDidUpdate(prevProps) {
    const bpmAnterior = prevProps.incutwin?.bpm;
    const bpmActual = this.props.incutwin?.bpm;
    const conBebeAnterior = prevProps.incutwin?.conBebe;
    const conBebeActual = this.props.incutwin?.conBebe;

    const enLineaActual = this.props.incutwin?.enLinea;
    const enLineaAnterior = prevProps.incutwin?.enLinea;

    const bpmCambio = bpmActual !== bpmAnterior;
    const bebeApareció = conBebeActual && !conBebeAnterior;
    const seDesconectó = !enLineaActual && enLineaAnterior;
    const seConectó = enLineaActual && !enLineaAnterior;

    if (bpmCambio || bebeApareció || seConectó) {
      const activo = enLineaActual && conBebeActual;
      this._iniciarLatido(activo ? bpmActual : 0);
    } else if (seDesconectó) {
      this._iniciarLatido(0);
    }
  }

  componentWillUnmount() {
    this._animacion?.stop();
  }

  _iniciarLatido(bpm) {
    this._animacion?.stop();
    this._escala.setValue(1);
    if (!bpm || bpm <= 0) return;

    const intervalo = 60000 / bpm;
    const subida = 100;
    const bajada = 100;
    const espera = Math.max(intervalo - subida - bajada, 50);

    this._animacion = Animated.loop(
      Animated.sequence([
        Animated.timing(this._escala, { toValue: 1.25, duration: subida, useNativeDriver: true }),
        Animated.timing(this._escala, { toValue: 1, duration: bajada, useNativeDriver: true }),
        Animated.delay(espera),
      ])
    );
    this._animacion.start();
  }

  render() {
    const { navigation, incutwin } = this.props;

    if (!incutwin) {
      return (
        <View style={styles.centrado}>
          <Text style={styles.errorTexto}>Incutwin no encontrada</Text>
        </View>
      );
    }

    const tieneBpm = incutwin.bpm > 0;
    const tieneTemp = incutwin.temperatura > 0;

    return (
      <ScrollView style={styles.fondo} contentContainerStyle={styles.contenedor}>

        {/* Botón volver */}
        <TouchableOpacity style={styles.volver} onPress={() => navigation.goBack()}>
          <MaterialCommunityIcons name="arrow-left" size={22} color={colorTexto} />
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

        {/* Tarjeta bebé — solo si está en línea */}
        {incutwin.enLinea && <View style={styles.tarjeta}>
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
        </View>}

        {/* Telemetría — solo si hay bebé */}
        {incutwin.enLinea && incutwin.conBebe && (
          <View style={styles.tarjeta}>
            <Text style={styles.seccionTitulo}>Latido en tiempo real</Text>
            <View style={styles.bpmContenedor}>
              <Animated.View style={{ transform: [{ scale: this._escala }] }}>
                <MaterialCommunityIcons
                  name="heart"
                  size={64}
                  color={tieneBpm ? '#DC2626' : colorTextoSecundario}
                />
              </Animated.View>
              <View style={styles.bpmFila}>
                <Text style={[styles.bpmNumero, { color: tieneBpm ? '#DC2626' : colorTextoSecundario }]}>
                  {tieneBpm ? incutwin.bpm : '—'}
                </Text>
                {tieneBpm && <Text style={styles.bpmUnidad}>bpm</Text>}
              </View>
            </View>
          </View>
        )}

        {incutwin.enLinea && incutwin.conBebe && (
          <View style={styles.miniFilas}>
            <View style={[styles.tarjeta, styles.miniTarjeta]}>
              <MaterialCommunityIcons
                name="thermometer"
                size={28}
                color={tieneTemp ? colorAlerta : colorTextoSecundario}
              />
              <Text style={[styles.miniValor, { color: tieneTemp ? colorTexto : colorTextoSecundario }]}>
                {tieneTemp ? `${incutwin.temperatura}°C` : '—'}
              </Text>
              <Text style={styles.miniLabel}>Temperatura</Text>
            </View>

            <View style={[styles.tarjeta, styles.miniTarjeta]}>
              <MaterialCommunityIcons
                name="hand-wave"
                size={28}
                color={incutwin.holdDetected ? colorAcentoClaro : colorTextoSecundario}
              />
              <Text style={[styles.miniValor, { color: incutwin.holdDetected ? colorAcentoClaro : colorTextoSecundario }]}>
                {incutwin.holdDetected ? 'Sí' : 'No'}
              </Text>
              <Text style={styles.miniLabel}>Mano detectada</Text>
            </View>
          </View>
        )}

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
  colorTexto,
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
    color: colorTexto,
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
    color: colorTexto,
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
  colorTexto,
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
  colorTexto,
    fontSize: 14,
  },
  filaValor: {
    color: colorTexto,
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
  colorTexto,
    fontSize: 14,
    fontStyle: 'italic',
  },

  // BPM
  bpmContenedor: {
    alignItems: 'center',
    paddingVertical: 12,
    gap: 8,
  },
  bpmFila: {
    flexDirection: 'row',
    alignItems: 'flex-end',
    gap: 6,
  },
  bpmNumero: {
    fontSize: 64,
    fontWeight: 'bold',
    lineHeight: 72,
  },
  bpmUnidad: {
    fontSize: 18,
    color: colorTextoSecundario,
  colorTexto,
    marginBottom: 10,
  },

  // Mini tarjetas
  miniFilas: {
    flexDirection: 'row',
    gap: 12,
    marginBottom: 0,
  },
  miniTarjeta: {
    flex: 1,
    alignItems: 'center',
    paddingVertical: 20,
    gap: 6,
    marginBottom: 16,
  },
  miniValor: {
    fontSize: 22,
    fontWeight: 'bold',
  },
  miniLabel: {
    fontSize: 12,
    color: colorTextoSecundario,
  colorTexto,
  },
});
