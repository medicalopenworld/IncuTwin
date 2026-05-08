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
import {
  colorAcento,
  colorAcentoClaro,
  colorAlerta,
  colorPrimario,
  colorPrimarioMedio,
  colorTextoSecundario,
} from '../comun/comun';

export default function LoginPantalla() {
  const insets = useSafeAreaInsets();
  const [email, setEmail] = useState('');
  const [contrasena, setContrasena] = useState('');
  const [mostrarContrasena, setMostrarContrasena] = useState(false);

  const handleLogin = () => {
    Alert.alert('Login pendiente de Firebase');
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

          <TouchableOpacity style={styles.boton} onPress={handleLogin} activeOpacity={0.85}>
            <Text style={styles.botonTexto}>Iniciar sesión</Text>
          </TouchableOpacity>

          <View style={styles.registroFila}>
            <Text style={styles.registroTexto}>¿No tienes cuenta? </Text>
            <TouchableOpacity onPress={() => {}}>
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
  botonTexto: {
    color: 'white',
    fontSize: 16,
    fontWeight: '600',
  },
  registroFila: {
    flexDirection: 'row',
    justifyContent: 'center',
    marginTop: 4,
  },
  registroTexto: {
    color: colorTextoSecundario,
    fontSize: 14,
  },
  registroEnlace: {
    color: colorAcentoClaro,
    fontSize: 14,
  },
});
