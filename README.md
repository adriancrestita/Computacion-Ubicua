# Computacion Ubicua

Proyecto de estación meteorológica basada en ESP32.

## Organización actual

- `firmware/async-weather-station/`
  - `config/config.h`: credenciales y parámetros de red/MQTT utilizados por el sketch principal.
  - `include/*.hpp`: utilidades compartidas (WiFi, MQTT asíncrono, NTP/JSON).
  - `src/est-metereologica.ino`: sketch oficial de la estación (versión AsyncMqttClient).
- `json/`: ejemplos de carga útil en formato JSON.
- `Fichas técnicas/`: documentación de sensores.
- `librerias zip/`: dependencias externas conservadas tal y como se recibieron.

La versión anterior basada en PubSubClient se eliminó para simplificar el mantenimiento. Solo se mantiene el código probado de `codigo bueno/`, reorganizado en la carpeta `firmware/async-weather-station/` sin modificar la lógica del sketch.
