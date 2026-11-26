# hmi-corazones — Tareas

Verificación en la IncuTwin de COM12: `test_apps` on-target y después el firmware.

## 1. Lógica (TDD en `test_apps`)

- [x] 1.1 Tests Unity de `rewards_logic` con los escenarios de la spec `rewards`: saldo y tope
      diario, sin hora, racha larga, abrazos guardados, días sin bebés, niveles, sellos, pasaporte
      y estrella, desbloquear y sin saldo, guirnalda de diciembre, demo aparte
- [x] 1.2 `components/rewards/rewards_logic.c` que los pasa
- [x] 1.3 Siembra de la racha larga desde la máscara de 7 días de `accompany` (test + código)

## 2. Pegamento

- [x] 2.1 `storage`: `STORAGE_NS_REWARDS` (+ `_SIM`) y borrado en el reset de fábrica
- [x] 2.2 `rewards.c`: blob versionado, mutex, `rewards_copy()`, suscripción a `ACC_INPUT` y
      `STATE_CHANGED`, identidad de la estancia con `stay_log`, `TWIN_EVT_REWARDS_CHANGED`
- [x] 2.3 `app_events`: `TWIN_EVT_REWARDS_CHANGED` con bits de novedades; `app_main`: `rewards_init`
- [x] 2.4 Verificar en COM12 por la consola (log de `rewards`) que un gesto suma 1 y otro el
      mismo día no, y que sobrevive a un reinicio (26-nov: gesto real → saldo 1, racha 3, «Primera
      mano» y TG; sigue tras reiniciar). Comando `rewards` de solo lectura en la consola

## 3. Pantalla

- [x] 3.1 Textos ES/EN: niveles, sellos y sus burbujas, pasaporte vacío, franjas, Personalizar,
      «Te faltan N corazones», confirmar desbloqueo
- [x] 3.2 Página de la racha: saldo, nivel y abrazos guardados
- [x] 3.3 Página de Logros (cuarta página): sellos con burbuja y pasaporte con banderas y estrellas
- [x] 3.4 `ui_streak_fx`: «+1» en el corazón que vuela y «+2» al súper corazón
- [x] 3.5 Franja con tipo: avisos de sello, nivel y guirnalda; tocarla lleva a Logros
- [x] 3.6 Temas: halo en reposo y fondo de Inicio desde lo elegido
- [x] 3.7 Ajustes con scroll y fila «Personalizar»; subpantalla Personalizar con desbloqueo
- [x] 3.8 Guirnalda: asset en `tools/gen_extra.py`, en Inicio del 1-dic al 6-ene si está puesta
- [x] 3.9 Demo: saldo, sellos y pasaporte simulados en la UI

## 5. Corazones, canje y métricas (26-nov)

- [x] 5.1 Canje «Una IncuNest con tu nombre» en `rewards_logic` (test) y aviso `redeem` con reintento
- [x] 5.2 `metrics_payload` (tests) y publicación en `cloud_status` (conexión, cada hora, 30 s tras cambios)
- [x] 5.3 Pestaña Corazones (`ui_hearts.c`) con la tienda; fuera Personalizar de Ajustes y Logros de Inicio
- [x] 5.4 Barra de cinco botones: sobre para Notificaciones, pollito para Historial
- [x] 5.5 Latido visible al ritmo real en el icono del corazón; burbuja sin número
- [x] 5.6 Rol `gateway` en `broker_bootstrap.py` y mapeos del IoT Gateway en BROKER-MQTT-CONTEXT §9
- [ ] 5.7 Desplegar: `broker_bootstrap.py`, las dos entradas del gateway y el aviso de canje en ThingsBoard

## 4. Verificación

- [x] 4.1 `idf.py -C test_apps` en COM12: todos los tests en verde (146, 26-nov)
- [x] 4.2 `idf.py build` sin avisos nuevos y flash en COM12 (revisión visual OK de Pablo, 26-nov): gesto → «+1» y saldo; Logros con
      «Primera mano» y la bandera de la IncuNest; desbloquear Lavanda y verla en el halo; la demo
      no toca el saldo de verdad
