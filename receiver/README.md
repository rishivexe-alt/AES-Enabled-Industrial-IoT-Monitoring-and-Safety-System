# Python Receiver

The working system uses a Python MQTT receiver to reconstruct telemetry after transport.

Verified processing sequence:

```text
MQTT payload
→ Base64 decode
→ extract 16-byte IV
→ AES-128-CBC decrypt
→ PKCS#7 unpad
→ JSON parse
→ SQLite insertion
```

## Public-release status

The private `receiver.py` implementation was not included in the verified public-release source bundle used to create this ZIP. It is therefore intentionally **not recreated or replaced with a synthetic implementation**.

When the source is cleared for publication, place it here and generate `receiver/requirements.txt` directly from its actual imports.
