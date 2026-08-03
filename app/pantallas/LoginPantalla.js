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
import { signInWithEmailAndPassword } from 'firebase/auth';
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

export default function LoginPantalla({ navigation }) {
  const insets = useSafeAreaInsets();
  const [email, setEmail] = useState('');
  const [contrasena, setContrasena] = useState('');
  const [mostrarContrasena, setMostrarContrasena] = useState(false);
  const [cargando, setCargando] = useState(false);

  const handleLogin = async () => {
    if (!email.trim() || !contrasena) {
      Alert.alert('Campos requeridos', 'Introduce el correo y la contraseña.');
      return;
    }
    try {
      setCargando(true);
      await signInWithEmailAndPassword(auth, email.trim(), contrasena);
      navigation.replace('Principal');
    } catch (error) {
      const mensajes = {
        'auth/invalid-credential': 'Correo o contraseña incorrectos.',
        'auth/user-not-found': 'No existe una cuenta con ese correo.',
        'auth/wrong-password': 'Contraseña incorrecta.',
        'auth/too-many-requests': 'Demasiados intentos. Espera un momento.',
      };
      Alert.alert('Error', mensajes[error.code] ?? 'No se pudo iniciar sesión.');
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
          <Text style={styles.titulo}>IncuTwin</Text>
          <Text style={styles.subtitulo}>Conectando corazones</Text>
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
            textColor={colorTexto}
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
            textColor={colorTexto}
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

          <TouchableOpacity
            style={[styles.boton, cargando && styles.botonDesactivado]}
            onPress={handleLogin}
            activeOpacity={0.85}
            disabled={cargando}
          >
            <Text style={styles.botonTexto}>
              {cargando ? 'Entrando...' : 'Iniciar sesión'}
            </Text>
          </TouchableOpacity>

          <View style={styles.registroFila}>
            <Text style={styles.registroTexto}>¿No tienes cuenta? </Text>
            <TouchableOpacity onPress={() => navigation.navigate('Registro')}>
              <Text style={styles.registroEnlace}>Regístrate</Text>
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
    color: colorTexto,
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
  botonTexto: {
    color: colorTexto,
    fontSize: 16,
    fontWeight: '600',
  },
  botonDesactivado: {
    opacity: 0.6,
  },
  registroFila: {
    flexDirection: 'row',
    justifyContent: 'center',
    marginTop: 4,
  },
  registroTexto: {
    color: colorTextoSecundario,
  colorTexto,
    fontSize: 14,
  },
  registroEnlace: {
    color: colorAcentoClaro,
    fontSize: 14,
  },
});
