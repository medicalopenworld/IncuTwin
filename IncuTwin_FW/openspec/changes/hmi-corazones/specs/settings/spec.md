## MODIFIED Requirements

### Requirement: Restablecer de fábrica [manual]
Mantener pulsado el botón Ajustes de la barra de navegación ≥ 1000 ms SHALL abrir un diálogo
"¿Restablecer de fábrica?" / "Factory reset?" con botones "Reset" y cerrar. Los diálogos del
panel SHALL usar las fuentes con glifos del español (sin cuadros de sustitución) y un ancho de
208 px centrado, sin llegar a los bordes de la pantalla. Confirmar SHALL
borrar la configuración de usuario (WiFi, onboarding completado, idioma, volumen, brillo, estado
del gemelo cacheado) y las recompensas (corazones, racha, sellos, pasaporte, desbloqueos) y
reiniciar al onboarding. NO SHALL borrar la identidad de fábrica (serie, credenciales MQTT) ni los
contadores de uso.

#### Scenario: Reset confirmado
- **WHEN** se mantiene el botón Ajustes 1 s, aparece el diálogo y se pulsa "Reset"
- **THEN** el panel reinicia en < 2 s, arranca en «Conecta tu móvil» del onboarding (sin pantalla
  de idioma), con 0 corazones, y tras completarlo vuelve a conectar al broker con las mismas
  credenciales de siempre

#### Scenario: Reset cancelado
- **WHEN** aparece el diálogo y se pulsa cerrar
- **THEN** el diálogo desaparece y nada cambia
