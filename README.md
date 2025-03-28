# Firminia is your personal buddy for digital sign activities with AskMeSign

### Hardware Required

* ESP32 development board: [Seeed XIAO Esp32-S3 1.47 LCD](https://www.seeedstudio.com/Seeed-XIAO-ESP32C3-p-5431.html).)
* Display: Waveshare 240×240, [General 1.28inch Round LCD Display Module](https://www.waveshare.com/1.28inch-lcd-module.htm).), 65K RGB
* Switch connected to a GPIO for triggering the BLE advertising
* A USB cable for power supply and programming

### Software Required

* An app to control BLE 4.2 connection from smartphone: 

### JSON format to configure Firminia
Throught BLE connection you need to send a JSON structure to configure Firminia for WIFI and API access.

This is the dataset:

{
    "ssid": "",
    "password": "",
    "server": "askmesign.askmesuite.com",
    "port": "443",
    "url": "https://askmesign.askmesuite.com/api/v2/files/pending?page=0&size=1",
    "token": "",
    "user": ""
}

### External API Required

System works with AskMeSign API
https://askmesign.askmesuite.com/swagger-ui.html#/