#define SerialMon Serial

// Set serial for AT commands (to the module)
#define SerialAT Serial1

#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024  // Set RX buffer to 1Kb

// Global Telemetry Data Strings
String FINALLATI = "0", FINALLOGI = "0", FINALSPEED = "0", FINALALT = "0";
String ignition = "false";
float battery = 0.0;

// Set GSM PIN, if any
#define GSM_PIN ""

// Your GPRS credentials, if any
const char apn[] = "YOUR-APN";
const char gprsUser[] = "";
const char gprsPass[] = "";

const char server[] = "Your Traccar Server IP";
const int port = 5055;
String myid = "Traccar ID";
const String FIRMWARE_VERSION = "v2.2.0-reliability";
const unsigned long REPORT_INTERVAL_MS = 30000UL;
const unsigned long NETWORK_RETRY_MS = 15000UL;

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

// DEBUG MODE: uncomment to print modem AT exchanges (can disclose network metadata)
// #define DUMP_AT_COMMANDS

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);
HttpClient http(client, server, port);

#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 27
#define PIN_RX 26
#define PWR_PIN 4
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13
#define LED_PIN 12
#define BAT_ADC 35

float ReadBattery() {
  float vref = 1.100;
  uint16_t volt = analogRead(BAT_ADC);
  float battery_voltage = ((float)volt / 4095.0) * 2.0 * 3.3 * (vref);
  return battery_voltage;
}

void modemPowerOn() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1000);
  digitalWrite(PWR_PIN, HIGH);
}

void modemPowerOff() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, LOW);
  delay(1500);
  digitalWrite(PWR_PIN, HIGH);
}

void enableGPS(void) {
  Serial.println("Starting GPS module...");
  modem.sendAT("+CGPIO=0,48,1,1");
  if (modem.waitResponse(5000L) != 1) {
    SerialMon.println("Warning: GPS antenna power command failed");
  }
  if (!modem.enableGPS()) {
    SerialMon.println("Warning: GNSS activation failed");
  }
}

void send_data(float lat, float lon, float speed, float alt, float accuracy, float currentBattery, 
                  int vsat, int usat, int rssi, String operatorName, float hdop, float vdop) {
  
  FINALLATI = String(lat, 8);
  FINALLOGI = String(lon, 8);
  FINALSPEED = String(speed, 2);
  FINALALT = String(alt, 0);
  
  String FINALBAT = "";
  String FINALBATLEVEL = "";
  // Battery voltage alone cannot reliably indicate vehicle ignition/charging.
  String FINALIGNITION = "unknown";
  String FINALCHARGE = "unknown";

  // Engine Status Logic via Battery Detection
  if (currentBattery <= 0.1) {
    FINALBATLEVEL = "";
    FINALBAT = "0.0";
    FINALIGNITION = "unknown";
    FINALCHARGE = "unknown";
  } else {
    float batterylevel = ((currentBattery - 3.0) / 1.2) * 100.0;
    if (batterylevel > 100.0) batterylevel = 100.0;
    if (batterylevel < 0.0) batterylevel = 0.0;
    
    FINALBAT = String(currentBattery, 2);
    FINALBATLEVEL = String(batterylevel, 0);
    FINALIGNITION = "unknown";
    FINALCHARGE = "unknown";
  }

  unsigned long uptimeSeconds = millis() / 1000;

  // Build Max-Telemetry URL Query String for Traccar API
  String urlParams = "/?id=" + myid + 
                     "&lat=" + FINALLATI + 
                     "&lon=" + FINALLOGI + 
                     "&altitude=" + FINALALT + 
                     "&speed=" + FINALSPEED + 
                     "&accuracy=" + String(accuracy, 2) + 
                     "&batt=" + FINALBAT + 
                     "&batteryLevel=" + FINALBATLEVEL +
                     "&ignition=" + FINALIGNITION + 
                     "&charge=" + FINALCHARGE +
                     "&rssi=" + String(rssi) +
                     "&operator=" + operatorName +
                     "&sat=" + String(usat) +          // Satellites Used
                     "&satVisible=" + String(vsat) +   // Satellites in View
                     "&hdop=" + String(hdop, 2) +      // Horizontal precision
                     "&vdop=" + String(vdop, 2) +      // Vertical precision
                     "&uptime=" + String(uptimeSeconds) +
                     "&version=" + FIRMWARE_VERSION;

  // URL parameters must not contain unescaped delimiters from the carrier name.
  // Preserve legitimate numeric values and encode the operator separately.
  // The operator was appended above; replace its raw substring with its encoded form.
  String encodedOperator;
  const char hex[] = "0123456789ABCDEF";
  for (size_t i = 0; i < operatorName.length(); ++i) {
    const uint8_t c = static_cast<uint8_t>(operatorName[i]);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encodedOperator += static_cast<char>(c);
    } else {
      encodedOperator += '%';
      encodedOperator += hex[(c >> 4) & 0x0F];
      encodedOperator += hex[c & 0x0F];
    }
  }
  urlParams.replace("&operator=" + operatorName + "&sat=", "&operator=" + encodedOperator + "&sat=");

  SerialMon.print("Payload Size: ");
  SerialMon.println(urlParams.length());
  SerialMon.println(urlParams);

  int err = http.post(urlParams);
  if (err != 0) {
    SerialMon.println(F("Failed to connect to server"));
    http.stop();
    return;
  }

  int status = http.responseStatusCode();
  SerialMon.println("Server Response Code: " + String(status));
  if (status < 200 || status >= 300) {
    SerialMon.println("Warning: Traccar rejected telemetry or response timed out");
  }
  http.stop();
}

void setup() {
  SerialMon.begin(115200);
  delay(10);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  delay(5000);

  modemPowerOn();

  Serial.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Modem restart timed out, proceeding...");
  }

  if (GSM_PIN[0] != '\0' && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }
}

void loop() {
  // Keep servicing GNSS even when cellular service is temporarily unavailable.
  static bool gpsEnabled = false;
  if (!gpsEnabled) {
    enableGPS();
    gpsEnabled = true;
  }

  if (!modem.isNetworkConnected()) {
    SerialMon.println("Waiting for cellular network...");
    if (!modem.waitForNetwork(60000L)) {
      SerialMon.println("No cellular network; retrying");
      delay(NETWORK_RETRY_MS);
      return;
    }
  }

  if (!modem.isGprsConnected()) {
    SerialMon.println("Connecting packet data...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      SerialMon.println("Packet data connection failed; retrying");
      delay(NETWORK_RETRY_MS);
      return;
    }
  }

  float lat = 0, lon = 0, speed = 0, alt = 0, accuracy = 0;
  int vsat = 0, usat = 0, year = 0, month = 0, day = 0;
  int hour = 0, minute = 0, second = 0;
  battery = ReadBattery();

  if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat,
                   &accuracy, &year, &month, &day, &hour, &minute, &second)) {
    if (isfinite(lat) && isfinite(lon) &&
        lat >= -90.0f && lat <= 90.0f &&
        lon >= -180.0f && lon <= 180.0f &&
        !(lat == 0.0f && lon == 0.0f)) {
      int rssi = modem.getSignalQuality(); // CSQ index 0-31, or 99 unknown; NOT dBm
      String operatorName = modem.getOperator();
      if (!operatorName.length()) operatorName = "Unknown";
      // DOP is not available in this version of TinyGSM's portable GPS API.
      send_data(lat, lon, speed, alt, accuracy, battery,
                vsat, usat, rssi, operatorName, 0.0f, 0.0f);
    } else {
      SerialMon.println("Invalid GPS coordinates ignored");
    }
  } else {
    SerialMon.println("Waiting for GPS fix");
  }

  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  delay(REPORT_INTERVAL_MS);
}
