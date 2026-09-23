# Preliminary Edge-AI Security Detection Module

The project includes a separate **Preliminary Edge-AI Security Detection Module** for security-oriented analysis of IoT telemetry/communication observations.

## Intended Security Classes

- Legitimate
- Spoofed
- Replayed
- Malformed

## Reported Evaluation

| Metric | Reported project result |
|---|---:|
| Weighted F1-score | 95.0% |
| Inference latency | 8.5 ms |

These values are documented as reported project results. They are not independently reproducible from this public repository.

## Confidentiality Boundary

The following are intentionally not disclosed:

- Model architecture
- Training code
- Training dataset
- Proprietary feature engineering
- Model parameters
- Model weights
- Internal inference implementation

The AI module is an additional security-analysis layer. It does not replace the ESP8266 local safety logic or AES-128-CBC telemetry protection.
