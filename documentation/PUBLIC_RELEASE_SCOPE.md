# Public Release Scope

This repository is a security-sanitized public release of the AES-Enabled Industrial IoT Monitoring and Safety System.

## Included

- Credential-sanitized ESP8266 firmware
- Exact hardware/pin documentation
- System and data-flow diagrams
- AES-CBC documentation
- Dedicated technical project report
- Representative dashboard evidence
- Preliminary Edge-AI security-module description
- Public-release security guidance

## Intentionally excluded

- Real Wi-Fi credentials
- Reusable AES keys
- MQTT credentials/tokens
- Live runtime SQLite database
- Private deployment configuration
- Confidential AI source, data, parameters, and weights
- Receiver/dashboard source that was not part of the verified public-release source bundle

This boundary is intentional. Missing implementation files are not replaced with invented code.
