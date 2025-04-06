# Firminia – Your Personal Assistant for Digital Signature Activities with AskMeSign

Firminia is a powerful, yet easy-to-use embedded solution designed to streamline your digital signing workflow using the AskMeSign platform. Leveraging BLE and Wi-Fi connectivity, Firminia displays pending documents and their status directly on a compact and efficient LCD display.

## Project Overview
Firminia periodically checks the AskMeSign API for pending documents requiring signatures, displaying the count clearly on an intuitive user interface. Configuration and initial setup are handled conveniently via BLE communication from your smartphone.

## Hardware Requirements

- **ESP32 Development Board:** [Seeed XIAO Esp32-S3](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html)
- **Display:** Waveshare [1.28-inch Round LCD Module](https://www.waveshare.com/1.28inch-lcd-module.htm), 240×240 pixels, 65K RGB
- **Push Button:** Normally-open switch for initiating BLE advertising mode
- **USB Cable:** For powering and programming the ESP32

## Software Requirements

- **BLE Scanner App** (Android recommended) to handle BLE 4.2 connections for initial configuration.
- **ESP-IDF Framework:** Version 5.4 recommended
- **LVGL Library:** For display management and animations

## Project Files Overview

- `main_flow.c`: Central logic controlling device states, Wi-Fi connectivity, BLE handling, and periodic API calls.
- `api_manager.c`: Manages HTTPS requests to the AskMeSign API, JSON response parsing, and error handling.
- `ble_manager.c`: Implements BLE GATT services allowing JSON-based configuration through a smartphone.
- `device_config.c`: Stores and retrieves device configuration (Wi-Fi credentials, API endpoints, user tokens) using NVS.
- `display_manager.c`: Controls LVGL-based user interface, handles animations, status indicators, and pending document count display.
- `wifi_manager.c`: Handles Wi-Fi initialization, connection logic, and reconnection events.

## JSON Configuration via BLE

Configure Firminia easily using the following JSON structure sent through a BLE app:

```json
{
    "ssid": "your_wifi_ssid",
    "password": "your_wifi_password",
    "server": "askmesign.askmesuite.com",
    "port": "443",
    "url": "https://askmesign.askmesuite.com/api/v2/files/pending?page=0&size=1",
    "token": "your_api_token",
    "user": "your_user_identifier"
}
```

## API Integration

Firminia integrates seamlessly with the AskMeSign REST API:

- **API Documentation:** [AskMeSign Swagger UI](https://askmesign.askmesuite.com/swagger-ui.html#/)
- **Supported Actions:** Fetching the count of pending documents to sign.

## Building and Flashing

To build and flash Firminia, ensure you have ESP-IDF set up:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/YOUR_SERIAL_PORT flash monitor
```

## Dependencies

Refer to `idf_component.yml` for component dependencies:

- `lvgl/lvgl: 9.2.0`
- `espressif/esp_lcd_gc9a01`: Driver for GC9A01 LCD Display

## Usage

1. **Initial Setup:** Press the configured button to enable BLE advertising.
2. **BLE Configuration:** Connect with your BLE scanner app and send the configuration JSON.
3. **Operation:** After successful configuration, Firminia connects to Wi-Fi, fetches document data periodically, and updates the display accordingly.

## Troubleshooting

- Ensure correct JSON configuration.
- Verify Wi-Fi signal and credentials.
- Monitor serial logs for diagnostic messages.

## Contributing

Feel free to fork, enhance, and open pull requests.

## License

This project is licensed under the MIT License.

