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
const String FIRMWARE_VERSION = "v2.1.0-MaxTelemetry";

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>

#define DUMP_AT_COMMANDS

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
  modem.sendAT("+SGPIO=0,4,1,1");
  modem.waitResponse(5000L);
  modem.enableGPS();
}

void send_data(float lat, float lon, float speed, float alt, float accuracy, float currentBattery, 
                  int vsat, int usat, int rssi, String operatorName, float hdop, float vdop) {
  
  FINALLATI = String(lat, 8);
  FINALLOGI = String(lon, 8);
  FINALSPEED = String(speed, 2);
  FINALALT = String(alt, 0);
  
  String FINALBAT = "";
  String FINALBATLEVEL = "";
  String FINALIGNITION = "false";
  String FINALCHARGE = "false";

  // Engine Status Logic via Battery Detection
  if (currentBattery <= 0.1) {
    FINALBATLEVEL = "";
    FINALBAT = "0.0";
    FINALIGNITION = "true";
    FINALCHARGE = "true";
  } else {
    float batterylevel = ((currentBattery - 3.0) / 1.2) * 100.0;
    if (batterylevel > 100.0) batterylevel = 100.0;
    if (batterylevel < 0.0) batterylevel = 0.0;
    
    FINALBAT = String(currentBattery, 2);
    FINALBATLEVEL = String(batterylevel, 0);
    FINALIGNITION = "false";
    FINALCHARGE = "false";
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

  // URL Encode any spaces in operator name if necessary
  urlParams.replace(" ", "%20");

  SerialMon.print("Payload Size: ");
  SerialMon.println(urlParams.length());
  SerialMon.println(urlParams);

  int err = http.post(urlParams);
  if (err != 0) {
    SerialMon.println(F("Failed to connect to server"));
    delay(5000);
    return;
  }

  int status = http.responseStatusCode();
  if (status) {
    String body = http.responseBody();
    SerialMon.println("Server Response Code: " + String(status));
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

  if (GSM_PIN && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }
}

void loop() {
  SerialMon.print(F("Connecting to cellular network: "));
  SerialMon.println(apn);
  
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    SerialMon.println("GPRS connection failed. Retrying...");
    delay(15000);
    return;
  }
  SerialMon.println("GPRS Connected Successfully!");

  enableGPS();

  float lat, lon, speed, alt, accuracy;
  int vsat, usat, year, month, day, hour, min, sec;
  float hdop = 0.0, vdop = 0.0, pdop = 0.0;
  
  while (1) {
    battery = ReadBattery();
    
    // Fetch Cellular Metadata 
    int rssi = modem.getSignalQuality(); // Returns standard dBm indicators
    String operatorName = modem.getOperator(); // Pulls connected carrier string
    if(operatorName.length() == 0) operatorName = "Unknown";

    // Request GPS Fix
    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, &year, &month, &day, &hour, &min, &sec)) {
      
      // Attempt to pull granular dilution of precision metrics if modem supports it
      modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, 
                   &year, &month, &day, &hour, &min, &sec, &hdop, &vdop, &pdop);

      // Ship out the master telemetry array
      send_data(lat, lon, speed, alt, accuracy, battery, vsat, usat, rssi, operatorName, hdop, vdop);
    }
    
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    
    // Manage loop pacing depending on vehicle deployment power state
    if (battery <= 0.1) {
      delay(10000); // Active updates when tracking with vehicle power
    } else {
      int count = 0;
      while ((battery > 0.1) && (count < 60)) { // Conserve juice during fallback state
        battery = ReadBattery();
        delay(10000);
        count++;
      }
    }
  }
}
