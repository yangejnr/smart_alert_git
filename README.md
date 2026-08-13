# Smart Alert — ESP32 IoT Alarm & Telemetry Receiver

Smart Alert is an embedded IoT prototype built around the ESP32. It receives sensor messages over MQTT, presents live status information on an LCD, and provides local visual and audible alerts using LEDs and a buzzer.

## Vision

The project explores a simple but reusable edge-alert architecture: remote sensor nodes publish events, while an ESP32 receiver subscribes to those messages and converts them into immediate physical feedback.

A typical use case is an environmental or security monitoring system where a remote device publishes data such as temperature and motion state and the receiver provides an on-site indication without requiring a computer screen.

## System Flow

```text
Remote Sensor Node
       │
       ▼
   MQTT Broker
       │ TLS
       ▼
      ESP32
       │
 ┌─────┼───────────┐
 ▼     ▼           ▼
LCD   LEDs       Buzzer
```

## Current Capabilities

The current firmware implements:

- ESP32 Wi-Fi connectivity
- MQTT subscription using `PubSubClient`
- TLS-capable network client
- JSON message parsing with ArduinoJson
- 16×2 I2C LCD output
- red power/status LED
- green idle/ready LED
- white activity indicator
- audible buzzer feedback
- display of incoming temperature and motion values
- automatic MQTT reconnection logic

## Message Model

The receiver expects JSON-style sensor messages containing fields such as:

```json
{
  "temperature": 28.4,
  "motion": 1,
  "mac": "sensor-id"
}
```

The exact payload can evolve as additional sensor types are introduced.

## Security

Wi-Fi credentials are intentionally **not stored in the tracked source code**.

Copy:

```text
include/secrets.example.h
```

to:

```text
include/secrets.h
```

and set the network credentials locally. `include/secrets.h` is excluded by `.gitignore`.

> Never commit real passwords, API keys or private certificates to a public repository.

## Technology

- ESP32
- C++ / Arduino framework
- PlatformIO
- MQTT
- TLS
- ArduinoJson
- I2C
- LCD1602
- Wi-Fi

## Repository Structure

```text
smart_alert_git/
├── include/
│   └── secrets.example.h
├── src/
│   └── main.cpp
├── lib/
├── test/
├── platformio.ini
└── README.md
```

## Development Status

**Embedded prototype.**

The project currently demonstrates the receiver/alert side of a larger sensor-to-alert architecture. Production deployment would require hardened credential management, validated certificates, robust broker authentication, non-blocking alert timing and fault recovery testing.

## Author

**Yange Henry Terzugwe**  
Software Developer • AI & Robotics Practitioner
