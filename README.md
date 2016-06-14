# Tetris Table

Insipred by [LED side table](https://sites.google.com/site/klaasdc/led-table)
I build my own Tetris Table based un EPS8266 and WS2812B LED-Stripes.
The construction and additional informations can be found on my page at:
https://www.basti79.de/mediawiki/index.php/LED_Tetris-Tisch

It's based on the Arduino [Arduino core for ESP8266](https://github.com/esp8266/Arduino) and needs these libraries:
* [WiFiManager](https://github.com/tzapu/WiFiManager)
* [Arduino Client for MQTT](https://github.com/knolleary/pubsubclient)
* [NeoPixelBus](https://github.com/Makuna/NeoPixelBus)
* [SimpleTimer Library](http://playground.arduino.cc/Code/SimpleTimer) (already included here)

Please use "git clone --recursive" to get the LCD-Fonts.

Use "html2h.sh" to convert the index.html into a C-header.
