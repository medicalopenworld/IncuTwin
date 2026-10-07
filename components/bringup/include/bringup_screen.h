/* IncuTwin — pantalla de bring-up (spec board-bringup).
 *
 * Solo se compila con CONFIG_INCUTWIN_BRINGUP_SCREEN. Sustituye la UI de
 * producto por una pantalla de verificacion de hardware:
 *   - rectangulos de orientacion (rojo arriba-izq, verde abajo-der) y "USB ^"
 *   - cinco dianas tactiles; cada toque se registra en el log con su error
 *   - un circulo en movimiento continuo para detectar tearing
 *   - rampa de brillo 10/50/100 % cada segundo
 *   - BOOT corto: siguiente combinacion swap/mirror del tactil (se muestra en
 *     pantalla y en el log). BOOT largo (>= 1 s): escala en el zumbador.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void bringup_screen_start(void);

#ifdef __cplusplus
}
#endif
