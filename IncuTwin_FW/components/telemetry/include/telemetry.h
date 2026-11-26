/* IncuTwin — telemetria de uso: registro, cola persistente y envio por lotes (spec telemetry-events).
 *
 * Hilos: una tarea propia ("telemetry") es la unica que toca la cola de flash (tel_queue no es
 * reentrante y un push puede borrar un sector: 30-400 ms). telemetry_log() se puede llamar desde
 * cualquier tarea (UI/LVGL incluida): valida, anota la hora (`t`, `up`) y copia el evento a una
 * cola en RAM sin esperar; si esta llena el evento se pierde y suma a `lost`. La tarea lo numera
 * (`seq`), lo guarda en la particion `storage` y lo envia en lotes a incutwin/<id>/ev (QoS 1).
 *
 * Envio: al conectar con el broker, 60 s despues del primer evento sin enviar o con 20 pendientes,
 * lo que antes ocurra; un solo lote en vuelo. Un lote sale de la cola solo con su PUBACK (el del
 * msg_id en vuelo; cualquier otro se ignora). Sin PUBACK en 30 s o al caer la sesion, el lote se da
 * por no enviado y vuelve a salir desde la cola con los mismos `seq` (el recolector descarta
 * duplicados por (cliente, seq)).
 *
 * Identidad (NVS `tel`): `gen` (empieza en 1; storage_factory_reset() lo sube), `boot_n` (+1 en
 * cada arranque), `acked` (ultimo seq confirmado), `fw` (ultimo firmware visto), `demo_n`. Cada
 * evento guarda el gen y el arranque en que se registro; un lote solo junta eventos con los mismos.
 * Si la NVS se borra y la cola de flash no, al abrir la cola gen sube al mas alto de la cola y
 * boot_n a uno mas que el mas alto: nunca vuelven atras. boot_n y demo_n cuentan toda la vida de
 * la IncuTwin, a traves de duenos (no se ponen a 0 con el restablecimiento).
 *
 * Nunca bloquea la UI ni ensena errores: lo que falla va al log serie.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Tras storage_init(): lee y sube las identidades, registra `boot` (y `ota` si cambio el firmware)
 * y arranca la tarea, que abre la cola de flash fuera del camino critico del arranque. */
void telemetry_init(void);

/* Tras mqtt_link_start(): se suscribe a TWIN_EVT_MQTT y TWIN_EVT_MQTT_PUBACK. Sin llamarla los
 * eventos se guardan pero no se envian. */
void telemetry_start(void);

/* Registra un evento. kind: 1..11 caracteres [a-z_] (si no, se descarta con un aviso). d_json:
 * objeto JSON de < 160 B ("{...}", sin caracteres de control); NULL = "{}" (sin aviso); si no es valido se
 * sustituye por "{}" con un aviso. Seguro desde cualquier tarea; no bloquea. Con la demo activa
 * solo pasa kind "demo". Antes de telemetry_init() no hace nada. */
void telemetry_log(const char *kind, const char *d_json);

/* Con on, telemetry_log ignora todo salvo "demo". Al pasar de off a on suma uno a `demo_n`. */
void telemetry_set_demo(bool on);

uint32_t telemetry_gen(void);        /* gen de este arranque */
uint32_t telemetry_boot(void);       /* numero de arranque (boot_n) */
uint32_t telemetry_pending(void);    /* eventos en la cola de flash sin confirmar */
uint32_t telemetry_lost(void);       /* perdidos desde el arranque (cola llena, RAM llena, flash) */
uint32_t telemetry_demo_count(void); /* NVS tel/demo_n */

/* Para el restablecimiento de fabrica: guarda en flash lo que haya en RAM, envia un lote si hay
 * sesion y espera su PUBACK como mucho wait_ms (bloquea al que llama). */
void telemetry_flush_now(uint32_t wait_ms);

/* Comando de consola `tel`: gen, boot_n, next_seq, pending, lost, lote en vuelo y pila libre minima
 * de la tarea telemetry y de esp_timer. */
void telemetry_print_status(void);

#ifdef __cplusplus
}
#endif
