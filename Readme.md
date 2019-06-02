Arduino RC RX receiver code for ESP-NOW RC protocol implemented in OpenTX for ESP32 (https://github.com/tmrttmrt/opentx/tree/esp32).

It supports ESP32 and ESP8266 chips. The current version supports PPM output only. Extending it to support servo PWM outputs is straightforward.

Only ESP8266 has been tested for now. (Use it on your own risk!)

## Compilation

ESP8266: use Generic ESP8266 module with nonos-sdk 2.2.2. Broadcast reception from ESP32 does not work with other versions. 

## Operation 

On boot the receiver listens for BIND_TIMEOUT (1 s, LED is on continously) for broadcasted BIND packets on a WiFi channel BIND_CH (1). If a BIND packet is received the receiver stores the transmitter MAC address and the channel defined in the packet and stops listening for further BIND packets. The channel values in the bind packet are stored as the failsafe values and normal operation is resumed where only DATA packets from the stored transmitter MAC address are accepted.

If no BIND is recived within BIND_TIMEOUT (1 s) the normal operation is resumed with the settings stored during the previous binding. LED blinks fast if the DATA packets from the transmitter are recived or slow if there is no DATA from the transmitter. 

PPM output is activated only after the first DATA packet is recived form the stored transmitter MAC address. 

If the signal from the transmitter is lost the PPM output is held on the last received values for FS_TIMEOUT (1 s). After this the stored failsafe PPM output is set (LED blinks slowly) until the reception is recovered.

## To do
- telemetry
- long range WiFi for the case of ESP32 to possibly increase range
- schematics of a receiver with level shifters and power management for 5V operation
