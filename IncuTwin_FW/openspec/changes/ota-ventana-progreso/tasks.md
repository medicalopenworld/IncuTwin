# ota-ventana-progreso — Tareas

## 1. Firmware

- [x] 1.1 `ota_update`: cola hasta `ota_update_go()`, `TWIN_EVT_OTA_PENDING`, progreso 0..100 /
      fallo, reinicio ~1,5 s tras instalar; sin aceptar otra OTA mientras espera o descarga
- [x] 1.2 `ui_ota.c`: cuándo sale (recién arrancado / 2 min sin tocar / 6 h), ventana modal con
      barra y porcentaje, «Reiniciando...» y fallo con cierre a los 4 s; textos ES/EN
- [x] 1.3 Build `dev` sin comprobar el certificado (servidor HTTPS local para probar)
- [x] 1.4 Reintento tras un corte: OTA guardada en NVS (`ota`), hasta 3 descargas, borrada al
      instalarse o si el hash no cuadra

- [x] 1.5 Firmware objetivo retenido `cmd/fw`: `ota_version_cmp`, `ota_validate_target`,
      `ota_target_wanted` (tests), panel sobre flota, memoria del binario fallido (también rollback),
      `release.py --set-target / --clear-target`, BROKER-MQTT-CONTEXT §3 y §10

## 2. Verificación

- [x] 2.1 `idf.py build` sin avisos nuevos
- [x] 2.2 OTA de prueba en COM12 con la build `dev` (26-nov): `cmd/ota` por el broker, ventana en
      seguida (recién arrancado), 3,1 MB en ~51 s, sha256, reinicio, `"fw":"2.0.1"` y validada
- [x] 2.3 Corte de luz al 50 %: arranca con 2.0.0, al conectar reintenta sola (intento 2 de 3) y
      queda en 2.0.1; después, el firmware normal de nuevo en COM12
- [x] 2.4 Firmware objetivo en COM12 (build `dev`, 26-nov): objetivo del panel 2.0.1 retenido con
      la placa en 2.0.0 → «OTA ya», ventana, ~70 s (rssi −80), reinicio y validada; al reconectar con
      2.0.1, «nada que hacer»; objetivo borrado (retenido vacío) y firmware normal de nuevo
