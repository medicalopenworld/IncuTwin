import { Component, useRef, useEffect } from 'react';
import {
  ActivityIndicator,
  Animated,
  RefreshControl,
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { connect } from 'react-redux';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import { signOut } from 'firebase/auth';
import { fetchIncutwins, subscribeRealtime, unsubscribeRealtime } from '../redux/ActionCreators';
import { auth } from '../comun/firebase';
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

const mapStateToProps = (state) => ({
  incutwins: state.incutwins,
});

const mapDispatchToProps = {
  fetchIncutwins,
  subscribeRealtime,
  unsubscribeRealtime,
};

class PrincipalPantalla extends Component {
  state = { refreshing: false };

  async componentDidMount() {
    const uid = auth.currentUser?.uid;
    if (uid) {
      const incutwins = await this.props.fetchIncutwins(uid);
      if (incutwins?.length) {
        this.props.subscribeRealtime(incutwins);
      }
    }
    this._focusUnsub = this.props.navigation.addListener('focus', () => {
      this.forceUpdate();
    });
  }

  componentDidUpdate(prevProps) {
    const prevRefresh = prevProps.route?.params?.refreshAt;
    const currRefresh = this.props.route?.params?.refreshAt;
    if (currRefresh && currRefresh !== prevRefresh) {
      this.handleRefresh();
    }
  }

  componentWillUnmount() {
    this.props.unsubscribeRealtime();
    this._focusUnsub?.();
  }

  handleRefresh = async () => {
    const uid = auth.currentUser?.uid;
    if (!uid) return;
    this.setState({ refreshing: true });
    const incutwins = await this.props.fetchIncutwins(uid);
    if (incutwins?.length) {
      this.props.subscribeRealtime(incutwins);
    }
    this.setState({ refreshing: false });
  };

  handleLogout = async () => {
    await signOut(auth);
    this.props.navigation.replace('Login');
  };

  render() {
    const { navigation } = this.props;
    const { isLoading, errMess, incutwins } = this.props.incutwins;
    const usuario = auth.currentUser;
    const nombre = usuario?.displayName?.trim() || usuario?.email?.split('@')[0] || '';

    if (isLoading) {
      return (
        <View style={styles.centrado}>
          <ActivityIndicator size="large" color={colorAcento} />
        </View>
      );
    }

    if (errMess) {
      return (
        <View style={styles.centrado}>
          <Text style={styles.error}>{errMess}</Text>
        </View>
      );
    }

    const totalIncutwins = incutwins.length;
    const conBebe = incutwins.filter((i) => i.enLinea && i.conBebe).length;
    const enLinea = incutwins.filter((i) => i.enLinea).length;

    return (
      <View style={styles.flex}>
      <ScrollView
        style={styles.fondo}
        contentContainerStyle={styles.contenedor}
        refreshControl={
          <RefreshControl
            refreshing={this.state.refreshing}
            onRefresh={this.handleRefresh}
            tintColor={colorAcento}
            colors={[colorAcento]}
          />
        }
      >

        {/* Header */}
        <View style={styles.header}>
          <View style={styles.headerFila}>
            <View>
              <Text style={styles.hola}>Hola,</Text>
              <Text style={styles.emailTexto}>{nombre}</Text>
            </View>
            <View style={styles.headerIconos}>
              <TouchableOpacity onPress={() => this.props.navigation.navigate('Perfil')} style={styles.iconoBoton}>
                <MaterialCommunityIcons name="account-circle-outline" size={24} color={colorTextoSecundario} />
              </TouchableOpacity>
              <TouchableOpacity onPress={this.handleLogout} style={styles.iconoBoton}>
                <MaterialCommunityIcons name="logout" size={22} color={colorTextoSecundario} />
              </TouchableOpacity>
            </View>
          </View>
        </View>

        {/* Tarjetas resumen */}
        <View style={styles.resumenFila}>
          <View style={[styles.resumenTarjeta, { backgroundColor: colorPrimarioMedio }]}>
            <Text style={[styles.resumenNumero, { color: colorAcentoClaro }]}>{totalIncutwins}</Text>
            <Text style={styles.resumenLabel}>Total</Text>
          </View>

          <View style={[styles.resumenTarjeta, { backgroundColor: '#FEF3C7' }]}>
            <Text style={[styles.resumenNumero, { color: colorAlerta }]}>{conBebe}</Text>
            <Text style={styles.resumenLabel}>Con bebé</Text>
          </View>

          <View style={[styles.resumenTarjeta, { backgroundColor: '#E0F2FE' }]}>
            <Text style={[styles.resumenNumero, { color: colorTextoSecundario }]}>{enLinea}</Text>
            <Text style={styles.resumenLabel}>En línea</Text>
          </View>
        </View>

        {/* Lista de incutwins */}
        {incutwins.length === 0 ? (
          <View style={styles.vacio}>
            <MaterialCommunityIcons name="wifi-off" size={48} color={colorTextoSecundario} />
            <Text style={styles.vacioTexto}>No tienes incutwins asignadas</Text>
          </View>
        ) : (
          [...incutwins].sort((a, b) => (b.enLinea ? 1 : 0) - (a.enLinea ? 1 : 0)).map((item) => (
            <IncutwinCard
              key={item.docId}
              item={item}
              onPress={() => navigation.navigate('Detalle', { incutwinId: item.docId, incubadoraId: item.incubadoraId })}
            />
          ))
        )}
      </ScrollView>

      <TouchableOpacity
        style={styles.fab}
        onPress={() => navigation.navigate('Escaneo')}
        activeOpacity={0.85}
      >
        <MaterialCommunityIcons name="qrcode-scan" size={26} color="white" />
      </TouchableOpacity>
      </View>
    );
  }
}

function IncutwinCard({ item, onPress }) {
  const pulse = useRef(new Animated.Value(0)).current;

  useEffect(() => {
    let anim;
    if (item.holdDetected) {
      // Siempre brillante: pulsa entre azul cielo y blanco puro, sin volver al oscuro
      anim = Animated.loop(
        Animated.sequence([
          Animated.timing(pulse, { toValue: 1, duration: 450, useNativeDriver: false }),
          Animated.timing(pulse, { toValue: 0, duration: 450, useNativeDriver: false }),
        ])
      );
      anim.start();
    } else {
      pulse.setValue(0);
    }
    return () => anim?.stop();
  }, [item.holdDetected]);

  // Azul brillante ↔ blanco puro. Nunca oscuro.
  const cardBg = item.holdDetected
    ? pulse.interpolate({
        inputRange: [0, 1],
        outputRange: ['#38BDF8', '#FFFFFF'],
      })
    : colorPrimarioMedio;

  return (
    <TouchableOpacity onPress={onPress} activeOpacity={0.8} style={styles.tarjetaWrapper}>
      <Animated.View style={[styles.tarjeta, { backgroundColor: cardBg }]}>
        <View style={styles.tarjetaFila}>
          <MaterialCommunityIcons
            name="wifi"
            size={22}
            color={item.enLinea ? colorAcentoClaro : colorTextoSecundario}
            style={styles.iconoWifi}
          />
          <View style={styles.tarjetaTextos}>
            <Text style={styles.tarjetaNombre}>{item.nombre ?? '—'}</Text>
            <Text style={styles.tarjetaId}>
              {item.incubadoraId?.toUpperCase() ?? item.docId.toUpperCase()}
            </Text>
          </View>
          <View style={[styles.badge, item.enLinea ? styles.badgeVerde : styles.badgeGris]}>
            <Text style={styles.badgeTexto}>
              {item.enLinea ? 'En línea' : 'Sin conexión'}
            </Text>
          </View>
        </View>

        <View style={styles.tarjetaFila}>
          <MaterialCommunityIcons name="map-marker" size={16} color={colorTextoSecundario} />
          <Text style={styles.tarjetaHospital}>{item.hospital ?? '—'}</Text>
        </View>

        {item.enLinea && (
          item.conBebe ? (
            <View style={styles.bebeInfo}>
              <MaterialCommunityIcons name="baby-face-outline" size={16} color={colorAlerta} />
              <Text style={styles.bebeTexto}>
                {item.bebe?.nombre ?? 'Sin nombre'} · {item.bebe?.semanas ?? '?'} sem
              </Text>
            </View>
          ) : (
            <Text style={styles.sinBebe}>Sin bebé</Text>
          )
        )}
      </Animated.View>
    </TouchableOpacity>
  );
}

export default connect(mapStateToProps, mapDispatchToProps)(PrincipalPantalla);

const styles = StyleSheet.create({
  flex: {
    flex: 1,
    backgroundColor: colorPrimario,
  },
  fondo: {
    flex: 1,
    backgroundColor: colorPrimario,
  },
  fab: {
    position: 'absolute',
    bottom: 32,
    right: 24,
    backgroundColor: colorAcento,
    width: 56,
    height: 56,
    borderRadius: 28,
    justifyContent: 'center',
    alignItems: 'center',
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.25,
    shadowRadius: 4,
    elevation: 5,
  },
  contenedor: {
    paddingHorizontal: 20,
    paddingTop: 60,
    paddingBottom: 32,
  },
  centrado: {
    flex: 1,
    backgroundColor: colorPrimario,
    justifyContent: 'center',
    alignItems: 'center',
  },
  error: {
    color: '#EF4444',
    fontSize: 15,
    textAlign: 'center',
    paddingHorizontal: 24,
  },

  // Header
  header: {
    marginBottom: 24,
  },
  headerFila: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  headerIconos: {
    flexDirection: 'row',
    gap: 4,
  },
  iconoBoton: {
    padding: 8,
  },
  hola: {
    fontSize: 16,
    color: colorTextoSecundario,
  colorTexto,
  },
  emailTexto: {
    fontSize: 22,
    fontWeight: 'bold',
    color: colorTexto,
    marginTop: 2,
  },

  // Resumen
  resumenFila: {
    flexDirection: 'row',
    gap: 10,
    marginBottom: 28,
  },
  resumenTarjeta: {
    flex: 1,
    borderRadius: 12,
    paddingVertical: 14,
    alignItems: 'center',
  },
  resumenNumero: {
    fontSize: 32,
    fontWeight: 'bold',
  },
  resumenLabel: {
    fontSize: 12,
    color: colorTextoSecundario,
  colorTexto,
    marginTop: 4,
  },

  // Tarjeta incutwin
  tarjetaWrapper: {
    marginBottom: 12,
  },
  tarjeta: {
    backgroundColor: colorPrimarioMedio,
    borderRadius: 14,
    padding: 16,
    gap: 8,
  },
  tarjetaFila: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  iconoWifi: {
    marginRight: 2,
  },
  tarjetaTextos: {
    flex: 1,
    gap: 2,
  },
  tarjetaNombre: {
    color: colorTexto,
    fontWeight: 'bold',
    fontSize: 15,
  },
  tarjetaId: {
    color: colorTextoSecundario,
    fontSize: 12,
  },
  tarjetaHospital: {
    color: colorTextoSecundario,
  colorTexto,
    fontSize: 13,
  },

  // Badge
  badge: {
    borderRadius: 20,
    paddingHorizontal: 10,
    paddingVertical: 3,
  },
  badgeVerde: {
    backgroundColor: '#064E3B',
  },
  badgeGris: {
    backgroundColor: '#1E293B',
  },
  badgeTexto: {
    fontSize: 11,
    color: colorBlancoFondo,
    fontWeight: '600',
  },

  // Bebé
  bebeInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
  },
  bebeTexto: {
    color: colorAlerta,
    fontSize: 13,
  },
  sinBebe: {
    color: colorTextoSecundario,
  colorTexto,
    fontSize: 13,
    fontStyle: 'italic',
  },

  // Vacío
  vacio: {
    alignItems: 'center',
    marginTop: 60,
    gap: 12,
  },
  vacioTexto: {
    color: colorTextoSecundario,
  colorTexto,
    fontSize: 15,
  },
});
