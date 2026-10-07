# fleet-tools

Scripts de `tools/` (Python 3.12, Windows y Linux) para fabricar, emparejar y publicar
firmware. Ningún secreto en el repo: la clave de admin del broker va en `MQTT_ADMIN_PASS`, la del
rol publisher en `MQTT_PUB_PASS`.

## ADDED Requirements

### Requirement: Fabricación [manual]
`factory_provision.py --port <COM> [--hwrev 1.2] [--sn <SN>] [--no-broker]` SHALL, por panel:
leer la MAC; generar el serie `ITW-YYWW-NNNN` (o usar `--sn`); derivar el client id MQTT
`incutwin-<YYWW-NNNN>`; generar una contraseña aleatoria de 24 caracteres; dar de alta el
cliente en el broker con `mosquitto_ctrl ... dynsec createClient` + `addClientRole incutwin`
(salvo `--no-broker`); generar y flashear en 0x9000 una imagen NVS con los namespaces `factory`
(`sn`, `hwrev`, `batch`) y `mqtt` (`user`, `pass`); crear la etiqueta PNG (QR = serie, 50×25 mm,
300 dpi); y apuntar el panel en `tools/factory/devices.csv` (serie, MAC, hwrev, lote, fecha,
client id; **sin contraseña**). La contraseña SHALL guardarse en `tools/factory/secrets/<SN>.json`,
directorio ignorado por git.

#### Scenario: Alta completa
- **WHEN** se ejecuta con `MQTT_ADMIN_PASS` definido y el panel en COM12
- **THEN** `mosquitto_ctrl listClients` incluye el nuevo client id, el panel arranca con ese serie
  y conecta al broker, y la etiqueta y el manifiesto existen

#### Scenario: Reprovisionar un panel existente
- **WHEN** se ejecuta con `--sn` de un panel ya dado de alta
- **THEN** el script reutiliza la contraseña de `secrets/<SN>.json` y no crea un cliente nuevo

### Requirement: Emparejar y desemparejar [manual]
`pair.py <SN|client_id> <incubator_id>` SHALL crear el rol `incubator-<id>` si no existe (con
`subscribePattern incubators/<id>/state`), añadirlo al cliente y publicar retenido con QoS 1
`{"incubator_id":"<id>"}` en `incutwin/<client_id>/cmd/pair` usando las credenciales
`publisher`. `unpair.py <SN|client_id>` SHALL quitar el rol y publicar un retenido vacío.

#### Scenario: Emparejar la unidad de pruebas
- **WHEN** se ejecuta `pair.py ITW-2640-0007 353`
- **THEN** en < 5 s el panel muestra "Esperando a la IncuNest..." y, si hay estado retenido de
  353, el estado real

### Requirement: Release [manual]
`release.py [--upload <destino>]` SHALL compilar con `idf.py build`, copiar el binario a
`dist/incutwin-<versión>.bin`, calcular su sha256, escribir `dist/incutwin-<versión>.json` con
`{"url": "https://fw.medicalopenworld.org/incutwin/incutwin-<versión>.bin", "sha256": "..."}`,
subirlo si se indica destino, e imprimir el `mosquitto_pub` listo para lanzar la OTA a una
unidad o a `all`.

#### Scenario: Release local
- **WHEN** se ejecuta sin `--upload`
- **THEN** existen el `.bin` y el `.json`, y el sha256 impreso coincide con `sha256sum` del binario

### Requirement: Generación de assets [manual]
`gen_assets.py` y `gen_extra.py` SHALL producir los ficheros C de imágenes en formato LVGL 9
(`lv_image_dsc_t`, formatos I8 con paleta para el bebé y ARGB8888/RGB565 para el resto) a partir
de `Images/*.png`, y las 6 paletas de tono de piel, dejando previsualizaciones en
`tools/preview/`.

#### Scenario: Regenerar
- **WHEN** se ejecutan ambos scripts
- **THEN** el firmware compila con los ficheros generados y el bebé se ve igual que en la
  previsualización
