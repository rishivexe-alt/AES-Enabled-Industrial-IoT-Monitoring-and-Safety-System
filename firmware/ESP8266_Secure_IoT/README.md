# ESP8266 Secure IoT Firmware

This directory contains the public, credential-sanitized ESP8266 firmware for the project.

## Platform

- ESP8266 NodeMCU
- Arduino IDE
- NodeMCU 1.0 (ESP-12E Module)
- Serial Monitor: 115200 baud

## Libraries Used

The source includes:

- `ESP8266WiFi.h`
- `PubSubClient.h`
- `DHT.h`
- `Crypto.h`
- `AES.h`

## Local Configuration

The committed firmware contains placeholders for:

- Wi-Fi SSID
- Wi-Fi password
- MQTT broker address
- AES-128 key

Replace these only in your private working copy. Do not commit real credentials or a reusable production key.

## Encryption Format

The firmware creates a JSON telemetry string, applies PKCS#7 padding, encrypts it block-by-block with AES-128-CBC, prepends the fresh 16-byte IV, and Base64-encodes the combined bytes.

```text
[16-byte IV][ciphertext] → Base64 → MQTT
```

## Safety Thresholds

- Temperature: `> 35.0 °C`
- Humidity: `> 80.0 %`
- Gas sensor ADC: `> 1000`
- Distance: `0 < distance < 6.0 cm`

Invalid distance `-1.0 cm` does not trigger intrusion.
