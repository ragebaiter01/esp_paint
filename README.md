# esp_paint

Einrichten: Installiere im Arduino IDE Bibliotheksverwalter Adafruit SSD1306 und Adafruit GFX Library. Wähle außerdem das Board LOLIN(WEMOS) D1 R2 & mini aus dem ESP8266-Boardpaket. Die ESP8266-WLAN-, Webserver- und Wire-Bibliotheken sind darin enthalten. Adafruit Installationshinweise

Standardmäßig startet der ESP den Access Point ESP-Paint mit dem Passwort password123. Verbinde dich damit und öffne http://192.168.4.1. Für Router-WLAN trägst du SSID und Passwort im Sketch ein und stellst NETWORK_MODE auf WIFI_ONLY oder AP_AND_WIFI; die IP-Adresse wird im seriellen Monitor mit 115200 Baud angezeigt.
