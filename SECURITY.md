# Security Notes

This repository is intended for public GitHub publication.

## Never commit

- Wi-Fi passwords
- Reusable AES secret keys
- MQTT usernames/passwords
- API keys or tokens
- Private certificates
- Sensitive runtime databases
- Private network configuration
- Confidential AI model files or training data

## Cryptographic scope

AES-128-CBC is used for application-layer confidentiality. The design does not add a MAC/authentication tag, so CBC alone must not be presented as authenticated encryption.

## Local safety scope

The primary safety decision is made on the ESP8266. The dashboard mirrors/visualizes telemetry and does not replace the embedded safety logic.
