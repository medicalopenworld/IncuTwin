import { useState } from 'react';
import {
  Alert,
  KeyboardAvoidingView,
  Platform,
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { TextInput } from 'react-native-paper';
import { useSafeAreaInsets } from 'react-native-safe-area-context';
import { createUserWithEmailAndPassword } from 'firebase/auth';
import { auth } from '../comun/firebase';
import {
  colorAcento,
  colorAcentoClaro,
  colorAlerta,
  colorPrimario,
  colorPrimarioMedio,
  colorTextoSecundario,
} from '../comun/comun';

export default function RegistroPantalla({ navigation }) {
  const insets = useSafeAreaInsets();
  const [email, setEmail] = useState('');
  const [contrasena, setContrasena] = useState('');
  const [confirmar, setConfirmar] = useState('');
  const [mostrarContrasena, setMostrarContrasena] = useState(false);
  const [mostrarConfirmar, setMostrarConfirmar] = useState(false);
  const [cargando, setCargando] = useState(false);

  const handleRegistro = async () => {
    if (!email.trim() || !contrasena || !confirmar) {
      Alert.alert('Campos requeridos', 'Rellena todos los campos.');
      return;
    }
    if (contrasena !== confirmar) {
      Alert.alert('Error', 'Las contraseñas no coinciden.');
      return;
    }
    if (contrasena.length < 6) {
      Alert.alert('Error', 'La contraseña debe tener al menos 6 caracteres.');
      return;
    }
    try {
      setCargando(true);
      await createUserWithEmailAndPassword(auth, email.trim(), contrasena);
      navigation.replace('Principal');
    } catch (error) {
      const mensajes = {
        'auth/email-already-in-use': 'Ya existe una cuenta con ese correo.',
        'auth/invalid-email': 'El correo no tiene un formato válido.',
        'auth/weak-password': 'La contraseña es demasiado débil.',
      };
      Alert.alert('Error', mensajes[error.code] ?? 'No se pudo crear la cuenta.');
    } finally {
      setCargando(false);
    }
  };

  return (
    <KeyboardAvoidingView
      style={styles.flex}
      behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
    >
      <ScrollView
        contentContainerStyle={[
          styles.contenedor,
          { paddingTop: insets.top + 48, paddingBottom: insets.bottom + 32 },
        ]}
        keyboardShouldPersistTaps="handled"
      >
        {/* Cabecera */}
        <View style={styles.cabecera}>
          <Text style={styles.emoji}>❤️</Text>
          <Text style={styles.titulo}>Crear cuenta</Text>
          <Text style={styles.subtitulo}>Únete a IncuTwin</Text>
        </View>

        {/* Formulario */}
        <View style={styles.formulario}>
          <TextInput
            label="Correo electrónico"
            value={email}
            onChangeText={setEmail}
            mode="outlined"
            keyboardType="email-address"
            autoCapitalize="none"
            outlineColor={colorAcento}
            activeOutlineColor={colorAcentoClaro}
            textColor="white"
            style={styles.campo}
            theme={{ colors: { background: colorPrimarioMedio, onSurfaceVariant: colorTextoSecundario } }}
          />

          <TextInput
            label="Contraseña"
            value={contrasena}
            onChangeText={setContrasena}
            mode="outlined"
            secureTextEntry={!mostrarContrasena}
            outlineColor={colorAcento}
            activeOutlineColor={colorAcentoClaro}
            textColor="white"
            style={styles.campo}
            theme={{ colors: { background: colorPrimarioMedio, onSurfaceVariant: colorTextoSecundario } }}
            right={
              <TextInput.Icon
                icon={mostrarContrasena ? 'eye-off' : 'eye'}
                color={colorTextoSecundario}
                onPress={() => setMostrarContrasena((prev) => !prev)}
              />
            }
          />

          <TextInput
            label="Confirmar contraseña"
            value={confirmar}
            onChangeText={setConfirmar}
            mode="outlined"
            secureTextEntry={!mostrarConfirmar}
            outlineColor={colorAcento}
            activeOutlineColor={colorAcentoClaro}
            textColor="white"
            style={styles.campo}
            theme={{ colors: { background: colorPrimarioMedio, onSurfaceVariant: colorTextoSecundario } }}
            right={
              <TextInput.Icon
                icon={mostrarConfirmar ? 'eye-off' : 'eye'}
                color={colorTextoSecundario}
                onPress={() => setMostrarConfirmar((prev) => !prev)}
              />
            }
          />

          <TouchableOpacity
            style={[styles.boton, cargando && styles.botonDesactivado]}
            onPress={handleRegistro}
            activeOpacity={0.85}
            disabled={cargando}
          >
            <Text style={styles.botonTexto}>
              {cargando ? 'Creando cuenta...' : 'Registrarse'}
            </Text>
          </TouchableOpacity>

          <View style={styles.loginFila}>
            <Text style={styles.loginTexto}>¿Ya tienes cuenta? </Text>
            <TouchableOpacity onPress={() => navigation.goBack()}>
              <Text style={styles.loginEnlace}>Inicia sesión</Text>
            </TouchableOpacity>
          </View>
        </View>
      </ScrollView>
    </KeyboardAvoidingView>
  );
}

const styles = StyleSheet.create({
  flex: {
    flex: 1,
    backgroundColor: colorPrimario,
  },
  contenedor: {
    flexGrow: 1,
    paddingHorizontal: 28,
    backgroundColor: colorPrimario,
  },
  cabecera: {
    alignItems: 'center',
    marginBottom: 48,
  },
  emoji: {
    fontSize: 64,
    color: colorAlerta,
  },
  titulo: {
    fontSize: 36,
    fontWeight: 'bold',
    color: 'white',
    marginTop: 8,
  },
  subtitulo: {
    fontSize: 16,
    fontStyle: 'italic',
    color: colorAcentoClaro,
    marginTop: 6,
  },
  formulario: {
    gap: 16,
  },
  campo: {
    backgroundColor: colorPrimarioMedio,
  },
  boton: {
    backgroundColor: colorAcento,
    borderRadius: 10,
    paddingVertical: 14,
    alignItems: 'center',
    marginTop: 8,
  },
  botonDesactivado: {
    opacity: 0.6,
  },
  botonTexto: {
    color: 'white',
    fontSize: 16,
    fontWeight: '600',
  },
  loginFila: {
    flexDirection: 'row',
    justifyContent: 'center',
    marginTop: 4,
  },
  loginTexto: {
    color: colorTextoSecundario,
    fontSize: 14,
  },
  loginEnlace: {
    color: colorAcentoClaro,
    fontSize: 14,
  },
});
