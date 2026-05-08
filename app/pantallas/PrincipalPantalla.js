import { Component } from 'react';
import {
  ActivityIndicator,
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
import { fetchIncutwins } from '../redux/ActionCreators';
import { auth } from '../comun/firebase';
import {
  colorAcento,
  colorAcentoClaro,
  colorAlerta,
  colorBlancoFondo,
  colorPrimario,
  colorPrimarioMedio,
  colorTextoSecundario,
} from '../comun/comun';

const mapStateToProps = (state) => ({
  incutwins: state.incutwins,
});

const mapDispatchToProps = {
  fetchIncutwins,
};

class PrincipalPantalla extends Component {
  state = { refreshing: false };

  componentDidMount() {
    const uid = auth.currentUser?.uid;
    if (uid) {
      this.props.fetchIncutwins(uid);
    }
  }

  handleRefresh = async () => {
    const uid = auth.currentUser?.uid;
    if (!uid) return;
    this.setState({ refreshing: true });
    await this.props.fetchIncutwins(uid);
    this.setState({ refreshing: false });
  };

  handleLogout = async () => {
    await signOut(auth);
    this.props.navigation.replace('Login');
  };

  render() {
    const { navigation } = this.props;
    const { isLoading, errMess, incutwins } = this.props.incutwins;
    const email = auth.currentUser?.email ?? '';
    const nombre = email.split('@')[0];

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
    const conBebe = incutwins.filter((i) => i.conBebe).length;
    const enLinea = incutwins.filter((i) => i.enLinea).length;

    return (
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

          <View style={[styles.resumenTarjeta, { backgroundColor: '#3D1A0A' }]}>
            <Text style={[styles.resumenNumero, { color: colorAlerta }]}>{conBebe}</Text>
            <Text style={styles.resumenLabel}>Con bebé</Text>
          </View>

          <View style={[styles.resumenTarjeta, { backgroundColor: '#1E293B' }]}>
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
          incutwins.map((item) => (
            <TouchableOpacity
              key={item.docId}
              style={styles.tarjeta}
              onPress={() => navigation.navigate('Detalle', { incutwinId: item.docId, incubadoraId: item.incubadoraId })}
              activeOpacity={0.8}
            >
              {/* Fila superior: icono + ID + badge */}
              <View style={styles.tarjetaFila}>
                <MaterialCommunityIcons
                  name="wifi"
                  size={22}
                  color={item.enLinea ? colorAcentoClaro : colorTextoSecundario}
                  style={styles.iconoWifi}
                />
                <Text style={styles.tarjetaId}>
                  {item.incubadoraId?.toUpperCase() ?? item.docId.toUpperCase()}
                </Text>
                <View style={[styles.badge, item.enLinea ? styles.badgeVerde : styles.badgeGris]}>
                  <Text style={styles.badgeTexto}>
                    {item.enLinea ? 'En línea' : 'Sin conexión'}
                  </Text>
                </View>
              </View>

              {/* Hospital */}
              <View style={styles.tarjetaFila}>
                <MaterialCommunityIcons name="map-marker" size={16} color={colorTextoSecundario} />
                <Text style={styles.tarjetaHospital}>{item.hospital ?? '—'}</Text>
              </View>

              {/* Bebé */}
              {item.conBebe ? (
                <View style={styles.bebeInfo}>
                  <MaterialCommunityIcons name="baby-face-outline" size={16} color={colorAlerta} />
                  <Text style={styles.bebeTexto}>
                    {item.bebe?.nombre ?? 'Sin nombre'} · {item.bebe?.semanas ?? '?'} sem
                  </Text>
                </View>
              ) : (
                <Text style={styles.sinBebe}>Sin bebé</Text>
              )}
            </TouchableOpacity>
          ))
        )}
      </ScrollView>
    );
  }
}

export default connect(mapStateToProps, mapDispatchToProps)(PrincipalPantalla);

const styles = StyleSheet.create({
  fondo: {
    flex: 1,
    backgroundColor: colorPrimario,
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
  },
  emailTexto: {
    fontSize: 22,
    fontWeight: 'bold',
    color: 'white',
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
    marginTop: 4,
  },

  // Tarjeta incutwin
  tarjeta: {
    backgroundColor: colorPrimarioMedio,
    borderRadius: 14,
    padding: 16,
    marginBottom: 12,
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
  tarjetaId: {
    flex: 1,
    color: 'white',
    fontWeight: 'bold',
    fontSize: 14,
  },
  tarjetaHospital: {
    color: colorTextoSecundario,
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
    fontSize: 15,
  },
});
