# KairosLab Bell Manager

Firmware and Android companion app for the KairosLab school bell controller.

## ESP32-C3 firmware

Open `oled_displayMenu.ino` in Arduino IDE and select **ESP32C3 Dev Module**. In **Tools → Partition Scheme**, select **Huge APP (3MB No OTA/1MB SPIFFS)** before compiling or uploading; the firmware image is larger than the default application partition. Keep **USB CDC On Boot** enabled when using the USB serial port.

The Android companion project is in `android-app/`. Its build and Bluetooth connection instructions are in `android-app/README.md`.
