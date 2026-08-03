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
import { MaterialCommunityIcons } from '@expo/vector-icons';
import { createUserWithEmailAndPassword } from 'firebase/auth';
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

function emailValido(email) {
  return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email);
}

function calcularFortaleza(contrasena) {
  if (contrasena.length === 0) return 0;
  let puntos = 0;
  if (contrasena.length >= 6) puntos++;
  if (contrasena.length >= 10) puntos++;
  if (/[A-Z]/.test(contrasena)) puntos++;
  if (/[0-9]/.test(contrasena)) puntos++;
  if (/[^A-Za-z0-9]/.test(contrasena)) puntos++;
  return Math.min(puntos, 3);
}

const FORTALEZA_CONFIG = [
  { label: 'Débil',  color: '#EF4444' },
  { label: 'Media',  color: colorAlerta },
  { label: 'Fuerte', color: '#22C55E' },
];

export default function RegistroPantalla({ navigation }) {
  const insets = useSafeAreaInsets();
  const [email, setEmail] = useState('');
  const [contrasena, setContrasena] = useState('');
  const [confirmar, setConfirmar] = useState('');
  const [mostrarContrasena, setMostrarContrasena] = useState(false);
  const [mostrarConfirmar, setMostrarConfirmar] = useState(false);
  const [cargando, setCargando] = useState(false);
  const [emailTocado, setEmailTocado] = useState(false);

  const fortaleza = calcularFortaleza(contrasena);
  const emailOk = emailValido(email);

  const handleRegistro = async () => {
    if (!email.trim() || !contrasena || !confirmar) {
      Alert.alert('Campos requeridos', 'Rellena todos los campos.');
      return;
    }
    if (!emailOk) {
      Alert.alert('Error', 'El correo no tiene un formato válido.');
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
          { paddingTop: insets.top + 16, paddingBottom: insets.bottom + 32 },
        ]}
        keyboardShouldPersistTaps="handled"
      >
        {/* Botón volver */}
        <TouchableOpacity style={styles.volver} onPress={() => navigation.goBack()}>
          <MaterialCommunityIcons name="arrow-left" size={22} color={colorTexto} />
          <Text style={styles.volverTexto}>Volver</Text>
        </TouchableOpacity>

        {/* Cabecera */}
        <View style={styles.cabecera}>
          <Text style={styles.emoji}>❤️</Text>
          <Text style={styles.titulo}>Crear cuenta</Text>
          <Text style={styles.subtitulo}>Únete a IncuTwin</Text>
        </View>

        {/* Formulario */}
        <View style={styles.formulario}>

          {/* Email con icono de validación */}
          <View>
            <TextInput
              label="Correo electrónico"
              value={email}
              onChangeText={(v) => { setEmail(v); setEmailTocado(true); }}
              onBlur={() => setEmailTocado(true)}
              mode="outlined"
              keyboardType="email-address"
              autoCapitalize="none"
              outlineColor={emailTocado && !emailOk ? '#EF4444' : colorAcento}
              activeOutlineColor={emailTocado && !emailOk ? '#EF4444' : colorAcentoClaro}
              textColor={colorTexto}
              style={styles.campo}
              theme={{ colors: { background: colorPrimarioMedio, onSurfaceVariant: colorTextoSecundario } }}
              right={
                emailTocado && email.length > 0 ? (
                  <TextInput.Icon
                    icon={emailOk ? 'check-circle' : 'alert-circle'}
                    color={emailOk ? '#22C55E' : '#EF4444'}
                  />
                ) : null
              }
            />
            {emailTocado && !emailOk && email.length > 0 && (
              <Text style={styles.emailError}>Formato de correo no válido</Text>
            )}
          </View>

          {/* Contraseña */}
          <View>
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
            {/* Barra de fortaleza */}
            {contrasena.length > 0 && (
              <View style={styles.fortalezaContenedor}>
                <View style={styles.barrasFila}>
                  {[1, 2, 3].map((nivel) => (
                    <View
                      key={nivel}
                      style={[
                        styles.barra,
                        {
                          backgroundColor:
                            fortaleza >= nivel
                              ? FORTALEZA_CONFIG[fortaleza - 1].color
                              : colorPrimarioMedio,
                        },
                      ]}
                    />
                  ))}
                </View>
                <Text style={[styles.fortalezaLabel, { color: FORTALEZA_CONFIG[fortaleza - 1]?.color ?? colorTextoSecundario }]}>
                  {FORTALEZA_CONFIG[fortaleza - 1]?.label ?? ''}
                </Text>
              </View>
            )}
          </View>

          {/* Confirmar contraseña */}
          <TextInput
            label="Confirmar contraseña"
            value={confirmar}
            onChangeText={setConfirmar}
            mode="outlined"
            secureTextEntry={!mostrarConfirmar}
            outlineColor={confirmar.length > 0 && confirmar !== contrasena ? '#EF4444' : colorAcento}
            activeOutlineColor={confirmar.length > 0 && confirmar !== contrasena ? '#EF4444' : colorAcentoClaro}
            textColor={colorTexto}
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

  // Volver
  volver: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 6,
    marginBottom: 24,
  },
  volverTexto: {
    color: colorTexto,
    fontSize: 16,
  },

  cabecera: {
    alignItems: 'center',
    marginBottom: 40,
  },
  emoji: {
    fontSize: 56,
    color: colorAlerta,
  },
  titulo: {
    fontSize: 32,
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

  // Email error
  emailError: {
    color: '#EF4444',
    fontSize: 12,
    marginTop: 4,
    marginLeft: 4,
  },

  // Fortaleza
  fortalezaContenedor: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 10,
    marginTop: 8,
  },
  barrasFila: {
    flex: 1,
    flexDirection: 'row',
    gap: 6,
  },
  barra: {
    flex: 1,
    height: 4,
    borderRadius: 2,
  },
  fortalezaLabel: {
    fontSize: 12,
    fontWeight: '600',
    width: 48,
    textAlign: 'right',
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
    color: colorTexto,
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
  colorTexto,
    fontSize: 14,
  },
  loginEnlace: {
    color: colorAcentoClaro,
    fontSize: 14,
  },
});
