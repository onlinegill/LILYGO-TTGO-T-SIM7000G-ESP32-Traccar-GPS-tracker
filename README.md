# LILYGO T-SIM7000G ESP32 GPS Tracker for Traccar

ESP32 + SIM7000G cellular GPS tracker firmware that sends GNSS positions to a [Traccar](https://www.traccar.org/) server through its OsmAnd-compatible HTTP endpoint.

> **Status:** The firmware has passed a PlatformIO ESP32 compilation check. Testing with a physical board, SIM card, GPS antenna, and live Traccar server is still required.

## Features

- GPS coordinates, speed, altitude, satellite counts, and cellular signal data
- Cellular connection checks with retry behavior
- Rejection of invalid GPS coordinates
- Configurable position reporting interval (default: 30 seconds)
- Additional battery and device uptime telemetry
- Automated firmware compilation using GitHub Actions

## Requirements

| Item | Requirement |
| --- | --- |
| Controller | LILYGO TTGO T-SIM7000G ESP32 |
| Cellular | Activated SIM and compatible LTE-M / NB-IoT service |
| GPS | GNSS antenna with outdoor reception for initial testing |
| Server | Traccar server with an OsmAnd protocol listener, commonly TCP port `5055` |
| Software | Arduino IDE with ESP32 support, or PlatformIO |

**Check your particular LILYGO board revision and your mobile carrier's SIM7000G band support before use.** Not every SIM7000G module works on every carrier.

## Quick start

1. Install your SIM card and connect the cellular and GNSS antennas. Provide stable power.
2. In Traccar, create a device and note its unique identifier.
3. Open [`traccar.ino`](traccar.ino) and edit the settings shown below.
4. Compile and upload the sketch for your ESP32 board.
5. Open the serial monitor at **115200 baud**.
6. Test outdoors, allow time for a GPS fix, and verify the location in Traccar.

### Configuration

At the top of [`traccar.ino`](traccar.ino), update:

```cpp
#define GSM_PIN ""                     // SIM PIN if your SIM requires one

const char apn[] = "YOUR-APN";
const char gprsUser[] = "";
const char gprsPass[] = "";

const char server[] = "YOUR_TRACCAR_HOST";
const int port = 5055;
String myid = "YOUR_TRACCAR_DEVICE_ID";

const unsigned long REPORT_INTERVAL_MS = 30000UL;
```

Use the hostname or public IP address of your Traccar service, **without** an `http://` prefix. If using a private LAN address, the cellular modem cannot reach it unless the cellular network has a suitable route or VPN. Confirm your Traccar OsmAnd listener is accessible through the relevant firewall.

### Build with PlatformIO

The repository includes [`platformio.ini`](platformio.ini). The root Arduino sketch is converted to a PlatformIO source file for CI:

```bash
python3 -m pip install platformio==6.1.18
mkdir -p src
printf '#include <Arduino.h>\n' > src/main.cpp
cat traccar.ino >> src/main.cpp
pio run -e sim7000g
```

To upload using PlatformIO, connect the board over USB, then run:

```bash
pio run -e sim7000g -t upload
pio device monitor -b 115200
```

You can also use the Arduino IDE with TinyGSM, ArduinoHttpClient, and ESP32 board support. Library compatibility may depend on the versions installed.

## Files

| File | Purpose |
| --- | --- |
| [`traccar.ino`](traccar.ino) | Current tracker firmware |
| [`old_backup.ino`](old_backup.ino) | Unmodified backup of the previous firmware |
| [`platformio.ini`](platformio.ini) | PlatformIO dependency and board configuration |
| [`.github/workflows/firmware.yml`](.github/workflows/firmware.yml) | Automatic compilation on pull requests and pushes |

## Troubleshooting

| Problem | What to check |
| --- | --- |
| No cellular network | SIM activation, carrier bands, antenna, coverage, and SIM PIN |
| Packet-data connection fails | APN, data plan, and roaming permissions |
| No GPS location | GNSS antenna connection, clear outdoor sky, and time for first fix |
| HTTP request fails | Server hostname, port `5055`, firewall, and external reachability |
| Traccar shows no position | Exact device ID, protocol port, HTTP response, and valid GPS fix |
| Battery reading looks wrong | Board revision and voltage-divider / ADC calibration |

## Known limitations and security

- **HTTP is unencrypted.** Coordinates, device identifiers, and metadata may be visible to network intermediaries. Do not assume this is appropriate for sensitive deployments.
- Battery percentage is only an estimate. Validate against a meter before using it for alerts.
- Ignition and charging status are reported as `unknown`; they require a dedicated vehicle signal.
- HDOP and VDOP are currently reported as zero placeholders rather than measured precision values.
- The firmware uses fixed-period updates; it does not persist unsent points during connectivity loss.
- This project is community firmware, not a certified emergency or anti-theft tracking system.

## Testing and contributions

GitHub Actions compiles the firmware on pull requests. **A successful build does not prove hardware compatibility or end-to-end tracking.** Please [open an issue](https://github.com/onlinegill/LILYGO-TTGO-T-SIM7000G-ESP32-Traccar-GPS-tracker/issues) with your board revision, carrier/country, observed serial output (remove personal data), and reproduction steps if something fails.

The previous working sketch is kept in [`old_backup.ino`](old_backup.ino) for comparison and rollback.
