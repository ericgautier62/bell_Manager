# KairosLab Android companion

This Android Studio project wraps the existing standalone dashboard in a WebView and sends its API requests to the ESP32-C3 over BLE GATT.

## Pair and connect

1. On the ESP32, open **Connexion → Bluetooth**. This opens BLE advertising for five minutes.
2. In the app, tap **Bluetooth** and grant the requested Bluetooth permissions.
3. The app connects to the nearby KairosLab unit and loads its live state.

The app uses BLE service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`; control, input and output characteristics end in `0002`, `0004` and `0003` respectively. `REQ|id|method|path|length` opens a request; the app then sends ordered `0xD1, offset-low, offset-high, payload` chunks and `COMMIT|id`. Responses begin with `RESULT|id|length`, followed by `0xD2` offset-framed chunks. All API bodies are UTF-8 form data and use the same routes and validation as the web dashboard.

The five-minute physical activation window limits when a nearby phone can connect and modify settings. The school should still treat the device as physically accessible during that interval.

## Build

Open this `android-app` directory in Android Studio and build the `app` configuration. The project uses only Android platform APIs and has no third-party runtime dependencies.
