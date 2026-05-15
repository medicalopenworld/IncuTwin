import { useState } from 'react';
import { Alert, ScrollView, StyleSheet, Text, TouchableOpacity, View } from 'react-native';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { MaterialCommunityIcons } from '@expo/vector-icons';
import { sendPasswordResetEmail, signOut } from 'firebase/auth';
import { auth } from '../comun/firebase';
import {
  colorAcento,
  colorAcentoClaro,
  colorAlerta,
  colorPrimario,
  colorPrimarioMedio,
  colorTextoSecundario,
  colorTexto,
} from '../comun/comun';

function formatearFecha(isoString) {
  if (!isoString) return '—';
  const fecha = new Date(isoString);
  return fecha.toLocaleDateString('es-ES', { day: '2-digit', month: 'long', year: 'numeric' });
}

function obtenerIniciales(email) {
  if (!email) return '?';
  const nombre = email.split('@')[0];
  return nombre.slice(0, 2).toUpperCase();
}

export default function PerfilPantalla({ navigation }) {
  const insets = useSafeAreaInsets();
  const [enviandoReset, setEnviandoReset] = useState(false);

  const usuario = auth.currentUser;
  const email = usuario?.email ?? '';
  const fechaRegistro = formatearFecha(usuario?.metadata?.creationTime);
  const iniciales = obtenerIniciales(email);

  const handleCambiarContrasena = async () => {
    Alert.alert(
      'Cambiar contraseña',
      `Se enviará un enlace de restablecimiento a ${email}. ¿Continuar?`,
      [
        { text: 'Cancelar', style: 'cancel' },
        {
          text: 'Enviar',
          onPress: async () => {
            try {
              setEnviandoReset(true);
              await sendPasswordResetEmail(auth, email);
              Alert.alert('Correo enviado', 'Revisa tu bandeja de entrada para restablecer la contraseña.');
            } catch (error) {
              Alert.alert('Error', 'No se pudo enviar el correo. Inténtalo de nuevo.');
            } finally {
              setEnviandoReset(false);
            }
          },
        },
      ]
    );
  };

  const handleLogout = async () => {
    Alert.alert(
      'Cerrar sesión',
      '¿Seguro que quieres salir?',
      [
        { text: 'Cancelar', style: 'cancel' },
        {
          text: 'Salir',
          style: 'destructive',
          onPress: async () => {
            await signOut(auth);
            navigation.replace('Login');
          },
        },
      ]
    );
  };

  return (
    <ScrollView
      style={styles.fondo}
      contentContainerStyle={[styles.contenedor, { paddingTop: insets.top + 16, paddingBottom: insets.bottom + 32 }]}
    >
      {/* Botón volver */}
      <TouchableOpacity style={styles.volver} onPress={() => navigation.goBack()}>
        <MaterialCommunityIcons name="arrow-left" size={22} color={colorTexto} />
        <Text style={styles.volverTexto}>Volver</Text>
      </TouchableOpacity>

      {/* Avatar */}
      <View style={styles.avatarContenedor}>
        <View style={styles.avatarCirculo}>
          <Text style={styles.avatarIniciales}>{iniciales}</Text>
        </View>
        <Text style={styles.emailTexto}>{email}</Text>
        <Text style={styles.fechaTexto}>Miembro desde {fechaRegistro}</Text>
      </View>

      {/* Sección cuenta */}
      <View style={styles.tarjeta}>
        <Text style={styles.seccionTitulo}>Cuenta</Text>

        <View style={styles.filaInfo}>
          <MaterialCommunityIcons name="email-outline" size={20} color={colorTextoSecundario} />
          <View style={styles.filaTextos}>
            <Text style={styles.filaLabel}>Correo electrónico</Text>
            <Text style={styles.filaValor}>{email}</Text>
          </View>
        </View>

        <View style={styles.separador} />

        <View style={styles.filaInfo}>
          <MaterialCommunityIcons name="calendar-outline" size={20} color={colorTextoSecundario} />
          <View style={styles.filaTextos}>
            <Text style={styles.filaLabel}>Fecha de registro</Text>
            <Text style={styles.filaValor}>{fechaRegistro}</Text>
          </View>
        </View>
      </View>

      {/* Sección seguridad */}
      <View style={styles.tarjeta}>
        <Text style={styles.seccionTitulo}>Seguridad</Text>

        <TouchableOpacity
          style={styles.filaBoton}
          onPress={handleCambiarContrasena}
          disabled={enviandoReset}
          activeOpacity={0.7}
        >
          <MaterialCommunityIcons name="lock-reset" size={20} color={colorAcentoClaro} />
          <Text style={styles.filaBotonTexto}>
            {enviandoReset ? 'Enviando correo...' : 'Cambiar contraseña'}
          </Text>
          <MaterialCommunityIcons name="chevron-right" size={20} color={colorTextoSecundario} />
        </TouchableOpacity>
      </View>

      {/* Botón cerrar sesión */}
      <TouchableOpacity style={styles.logoutBoton} onPress={handleLogout} activeOpacity={0.8}>
        <MaterialCommunityIcons name="logout" size={20} color="#EF4444" />
        <Text style={styles.logoutTexto}>Cerrar sesión</Text>
      </TouchableOpacity>
    </ScrollView>
  );
}

const styles = StyleSheet.create({
  fondo: {
    flex: 1,
    backgroundColor: colorPrimario,
  },
  contenedor: {
    paddingHorizontal: 20,
  },

  // Volver
  volver: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    marginBottom: 32,
  },
  volverTexto: {
    color: colorTexto,
    fontSize: 16,
  },

  // Avatar
  avatarContenedor: {
    alignItems: 'center',
    marginBottom: 32,
    gap: 8,
  },
  avatarCirculo: {
    width: 84,
    height: 84,
    borderRadius: 42,
    backgroundColor: colorAcento,
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 4,
  },
  avatarIniciales: {
    fontSize: 32,
    fontWeight: 'bold',
    color: colorTexto,
  },
  emailTexto: {
    fontSize: 18,
    fontWeight: '600',
    color: colorTexto,
  },
  fechaTexto: {
    fontSize: 13,
    color: colorTextoSecundario,
  colorTexto,
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
    fontSize: 12,
    fontWeight: '600',
    color: colorTextoSecundario,
  colorTexto,
    textTransform: 'uppercase',
    letterSpacing: 0.8,
  },

  // Fila info
  filaInfo: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 14,
  },
  filaTextos: {
    flex: 1,
    gap: 2,
  },
  filaLabel: {
    fontSize: 12,
    color: colorTextoSecundario,
  colorTexto,
  },
  filaValor: {
    fontSize: 14,
    color: colorTexto,
    fontWeight: '500',
  },
  separador: {
    height: 1,
    backgroundColor: '#1E293B',
  },

  // Fila botón
  filaBoton: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 14,
  },
  filaBotonTexto: {
    flex: 1,
    fontSize: 15,
    color: colorAcentoClaro,
  },

  // Logout
  logoutBoton: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'center',
    gap: 10,
    backgroundColor: '#2D1515',
    borderRadius: 12,
    paddingVertical: 14,
    marginTop: 8,
  },
  logoutTexto: {
    color: '#EF4444',
    fontSize: 15,
    fontWeight: '600',
  },
});
