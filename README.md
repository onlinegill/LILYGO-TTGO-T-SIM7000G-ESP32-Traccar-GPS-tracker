# LILYGO T-SIM7000G ESP32 / Traccar GPS tracker

Arduino firmware for reporting SIM7000G GNSS positions to a Traccar server using the OsmAnd-style HTTP position endpoint (typically port 5055).

## Hardware
LILYGO T-SIM7000G ESP32, active GNSS antenna, compatible SIM with data plan, and stable power. Verify the exact hardware revision and pinout against LILYGO documentation.

## Configure
Edit `traccar.ino` before uploading:
- `apn`, `gprsUser`, `gprsPass`: SIM provider's packet-data settings.
- `server` and `port`: accessible Traccar server and OsmAnd protocol port (default 5055).
- `myid`: Traccar device identifier, matching the device configured in Traccar.
- `GSM_PIN`: SIM PIN if needed; leave blank otherwise.
- `REPORT_INTERVAL_MS`: interval between attempts (default 30 seconds).

**Privacy:** this firmware uses plain HTTP. It is **not** encrypted: network intermediaries can observe GPS coordinates and device identifiers. Use an appropriately protected server path/network before production deployment. Do not place personal credentials or private endpoints in public commits.

## Build / verification
The original Arduino IDE sketch is `traccar.ino`. Install ESP32 board support, TinyGSM, and ArduinoHttpClient and select an appropriate ESP32 board. For reproducible automated builds use PlatformIO:

```sh
python3 -m pip install platformio==6.1.18
mkdir -p src
printf '#include <Arduino.h>\n' > src/main.cpp
cat traccar.ino >> src/main.cpp
pio run -e sim7000g
```

The CI workflow performs the same build for pull requests. **A passing build does not validate a physical modem**, SIM registration, network APN, GPS antenna, satellite lock, battery calibration, or successful reporting to your Traccar installation.

## Behavior and limitations
- Enables GNSS active-antenna power with the SIM7000G module GPIO command, then enables GNSS.
- Checks cellular network and packet-data connections before attempting uploads.
- Rejects invalid GNSS latitude/longitude and reports a valid location on the configured interval.
- Attempts a fresh network connection on subsequent iterations when disconnected.
- Battery percentage is a rough estimate, not a calibrated fuel gauge.
- Vehicle ignition and charge detection **cannot be inferred accurately** from the battery pin. Their telemetry fields are sent as `unknown`. Don't use them for automation without adding a dedicated vehicle-voltage/ignition input.
- HDOP and VDOP are placeholders set to zero rather than measured values.
- Successful HTTP responses indicate acceptance by the web endpoint, but the Traccar device must also be configured correctly.

## Hardware acceptance checklist
1. Confirm a clean build, upload, and serial startup (115200 baud).
2. Verify modem network registration and APN connection.
3. Outdoors with GNSS antenna attached, wait for a valid satellite fix.
4. Confirm correct coordinates appear in the Traccar device, and examine HTTP responses.
5. Disconnect/reconnect cellular service and confirm recovery.
6. Check reported battery voltage against a meter before trusting percentage or battery-related alerts.

Original README dated January 16, 2024. This branch updates documentation and connection reliability without claiming hardware validation.
