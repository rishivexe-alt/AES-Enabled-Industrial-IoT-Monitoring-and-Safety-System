# ESP8266 Pin Configuration

| Device | Signal | ESP8266 pin | GPIO |
|---|---|---|---:|
| DHT11 | DATA | D2 | GPIO4 |
| HC-SR04 | TRIG | D1 | GPIO5 |
| HC-SR04 | ECHO | D5 | GPIO14 |
| MQ-135 | AO | A0 | Analog input |
| Touch sensor | OUT | D6 | GPIO12 |
| Green LED | Anode/control | D4 | GPIO2 |
| Red LED | Anode/control | D8 | GPIO15 |
| Yellow LED | Anode/control | D7 | GPIO13 |
| Buzzer | Control | D0 | GPIO16 |

All devices must share the appropriate electrical ground. Check the voltage compatibility of connected peripherals before powering the prototype.
