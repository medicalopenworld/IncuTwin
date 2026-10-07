/* IncuTwin — consola serie de soporte (design.md D12), 115200 por el USB.
 *   info          serie, client id, incubadora, slot, fw, ip, errores del tactil
 *   usage         contadores de uso
 *   unpair        borra el incubator_id local (el cmd/pair retenido lo repondra)
 *   reboot        reinicia
 *   factory-reset borra prov/settings/twin y reinicia al onboarding
 * Nunca imprime contrasenas. */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void console_start(void);

#ifdef __cplusplus
}
#endif
