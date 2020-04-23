# moisture-ESP-mcu
Uses an arduino moisture sensor with ESP (node mcu)
NodeMCU-Based IoT Project: Connecting YL-69 & YL-38 Moisture

Version:
ESP8266/ESP-NodeMCO Moisture-Sensor-Module to MQTT - V1.2 OTA Version 


What you have to adjust:

- sensor_pin: if you use an other chip or pin
- IOT_HOSTNAME: unique name of the ESP chip (normaly the last 4 elements from MAC address
WIFI:
- WLAN_SSID and WLAN_PASS
MQTT:
- AIO_SERVER: the IP address of the MQTT broker host
- AIO_USERNAME & AIO_KEY (if you use authorisation on the broker)
- all PROGMEM variables which holds the publish topic structure: adjust to your existing hyrarchi
OTA (Over The Air Updates)
- ArduinoOTA.setPassword .... : put your password here if you use authorisation for OTA

Flash with Arduino IDE (ESP / Nodem MCU libraries have to been installed 
