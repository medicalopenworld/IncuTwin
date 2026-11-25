# Nombre del bebé puesto por el usuario de la IncuTwin

Fecha: 24-11-2026 · Estado: borrador para aprobar · Base: rama `test/baby-sex-acompanar`
(`feat/acompanar` + sexo del bebé de la sesión de la nube)

## Para qué

Cuando a la incubadora que sigue tu IncuTwin llega un bebé sin nombre, el panel te pregunta si
quieres ponerle uno. Ese nombre es solo tuyo: se queda en tu IncuTwin, no lo ve nadie más y no
sale del panel. Sirve para que el vínculo con "tu" bebé sea personal.

## Objetivos

- Aviso al llegar un bebé sin nombre: «Ha llegado un bebé. ¿Quieres ponerle nombre?»
  [SÍ] [AHORA NO].
- Teclado en pantalla para nombres cortos, con la disposición de un teclado español de ordenador
  (Q-P, A-Ñ, Z-M) y sin vocales con tilde (Pablo, 24-nov-2026).
- El nombre sale donde hoy sale el nombre (la etiqueta sobre el bebé).
- Se puede poner más tarde o cambiar tocando la etiqueta.

## No objetivos

- Compartir el nombre con otros paneles, con la app o con ThingsBoard.
- Conservarlo tras un reset de fábrica o un cambio de placa (eso sería la opción B).
- Poner nombre en modo demo.

## Lo que sería un fracaso aunque funcione

- Que el aviso salga en mitad de "Coge mi mano" o tape una alarma.
- Que vuelva a preguntar por el mismo bebé después de decir AHORA NO.
- Que el nombre de un bebé se le quede puesto al siguiente.
- Que el nombre salga del panel, por MQTT, por los logs, por `info` o por las estadísticas de uso.

## Opciones (elegida: A)

- **A. Todo en el panel (elegida).** El aviso, el teclado y el guardado en la NVS de la IncuTwin.
  Es la más pequeña y encaja con «solo para ti».
- B. Aviso en el panel y guardado en ThingsBoard. Sobrevive a resets, pero el nombre sale del
  panel y hay que tocar el broker y el historial.
- C. Aviso en el panel y nombre escrito en el móvil por QR. Mucha pieza para escribir una palabra.

## Diseño

**Cuándo pregunta.** Con el bebé dentro (`baby = in`) en la incubadora que sigue el panel
(emparejada o elegida por el modo Acompañar), si no llega el nombre real (`name` vacío) y no hay
nombre local para esa estancia. Pregunta una sola vez por estancia. Si el panel está en mitad de
"Coge mi mano", o con alarma de temperatura, espera a que termine.

**Prioridad del nombre.** El nombre real, que solo llega con consentimiento de la familia
(`itw_show_name`), va primero. Si no llega, el nombre local. Si tampoco hay, la etiqueta muestra
«Ponle nombre» y al tocarla se abre el teclado.

**Teclado.** `lv_keyboard` de LVGL con un mapa propio en español: letras, Ñ, Á É Í Ó Ú y espacio.
Máximo 12 caracteres. Se recortan los espacios de los extremos. Vacío equivale a cancelar.
Las fuentes ya tienen esos glifos (`gen_fonts.py`, `ES_SYMBOLS`).

**Identificar la estancia.** *Corrección a lo dicho al elegir A:* hoy el estado del broker no
trae un identificador de estancia. Sin él, el panel no distingue "el mismo bebé tras reiniciar"
de "otro bebé" si estuvo apagado durante un cambio, sobre todo en modo Acompañar, que salta de
incubadora. Mi propuesta es que ThingsBoard añada `stay_id` al payload de
`incubators/<id>/state` (ya existe en la máquina de estados, p. ej. `"353:17"`, y no es un dato
personal). Mientras no esté desplegado, el panel usa `incubator_id` + un contador que sube en
cada transición `TWIN_TR_BABY_IN`. Funciona salvo si el panel se pierde la salida y la entrada.

**Guardado.** En la NVS, un anillo de los últimos 8 pares {estancia, nombre}, en el namespace
propio del componente. Se borra con `factory-reset`. Nunca se publica ni se escribe en el log
(como mucho «nombre local: sí»).

**ThingsBoard (lo que se despliega de la rama de la nube).** Se mantiene el sexo (`sex`, para el
dibujo de niña). El apodo automático deja de ir en `name` del broker: si no, nunca llegaría un
bebé «sin nombre». El apodo puede quedarse en los eventos del webhook y en los server attributes.
Esto también resuelve el fallo de `tb/test_sm_anon.js`.

## Lo que decide quien lo construya

- Dónde vive el código: un componente nuevo (`baby_name`) o dentro de `ui`.
- Aspecto exacto del aviso y del teclado en 240×320, siguiendo el estilo de las tarjetas
  existentes.
- Textos en todos los idiomas de `ui_i18n`.
- Si el aviso suena (por defecto, sin sonido nuevo).
- Tests on-target de la lógica: cuándo preguntar, el anillo NVS y la prioridad del nombre.
