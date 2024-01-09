#include <ArduinoHttpClient.h>
#include <TinyGsmClient.h>

const int UART_BAUD = 115200;
const int PWR_PIN = 4;
const int LED_PIN = 12;
const int BAT_ADC = 35;

const uint32_t uS_TO_S_FACTOR = 1000000ULL;
const int TIME_TO_SLEEP = 60;

const char apn[] = "YOUR-APN";
const char gprsUser[] = "";
const char gprsPass[] = "";
const char server[] = "Your Traccar Server IP";
const int port = 5055;
const String myid = "Traccar ID";
const String GSM_PIN = "";

TinyGsm modem(Serial1);
TinyGsmClient client(modem);
HttpClient http(client, server, port);

float ReadBattery() {
  float vref = 1.100;
  uint16_t volt = analogRead(BAT_ADC);
  return ((float)volt / 4095.0) * 2.0 * 3.3 * (vref);
}

void powerControl(int state) {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, state);
  delay(state == HIGH ? 1000 : 1500);
}

void modemRestart() {
  powerControl(LOW);
  delay(1000);
  powerControl(HIGH);
}

void initializeModem() {
  Serial.println("Initializing modem...");
  if (!modem.restart()) {
    Serial.println("Failed to restart modem, attempting to continue without restarting");
  }

  if (!GSM_PIN.isEmpty() && modem.getSimStatus() != 3) {
    modem.simUnlock(GSM_PIN);
  }
}

void connectToGPRS() {
  Serial.print("Connecting to ");
  Serial.print(apn);

  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    Serial.println(" fail");
    delay(30000);
    return;
  }

  Serial.println(" success");
}

void enableGPS() {
  Serial.println("Start positioning. Make sure to locate outdoors.");
  Serial.println("The blue indicator light flashes to indicate positioning.");

  modem.sendAT("+SGPIO=0,4,1,1");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println("SGPIO=0,4,1,1 false");
  }

  modem.enableGPS();
}

void disableGPS() {
  modem.sendAT("+SGPIO=0,4,1,0");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println("SGPIO=0,4,1,0 false");
  }

  modem.disableGPS();
}

void sendData(float lat, float lon, float speed, float alt, float accuracy, float battery) {
  String params = String("/?id=") + myid +
                  "&lat=" + String(lat, 8) +
                  "&lon=" + String(lon, 8) +
                  "&accuracy=" + String(accuracy, 2) +
                  "&altitude=" + String(alt, 0) +
                  "&speed=" + String(speed, 2) +
                  "&battery=" + String(battery, 2);

  if (battery == 0) {
    params += "&ignition=true";
  } else {
    float batteryLevel = constrain(((battery - 3) / 1.2) * 100, 0, 100);
    params += "&ignition=false&batteryLevel=" + String(batteryLevel, 0);
  }

  int err = http.post(params);
  Serial.println(params);

  if (err != 0) {
    Serial.println("Failed to connect");
    delay(10000);
    return;
  }

  int status = http.responseStatusCode();

  if (!status) {
    delay(10000);
    return;
  }

  String body = http.responseBody();
  Serial.println("Response:");
  Serial.println(body);

  http.stop();
  Serial.println("Server disconnected. Will connect soon.");
}

void setup() {
  Serial.begin(UART_BAUD);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial1.begin(UART_BAUD, SERIAL_8N1, 26, 27);
  delay(10000);

  powerControl(HIGH);
  initializeModem();
}

void loop() {
  connectToGPRS();

  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected");
  }

  enableGPS();

  while (true) {
    float battery = ReadBattery();
    float lat, lon, speed, alt, accuracy;
    int vsat, usat, year, month, day, hour, min, sec;

    if (modem.getGPS(&lat, &lon, &speed, &alt, &vsat, &usat, &accuracy, &year, &month, &day, &hour, &min, &sec)) {
      sendData(lat, lon, speed, alt, accuracy, battery);
    }

    digitalWrite(LED_PIN, !digitalRead(LED_PIN));

    if (battery == 0) {
      delay(10000);
    } else {
      int count = 0;
      while (battery > 0 && count < 90) {
        battery = ReadBattery();
        delay(10000);
        count++;
      }
    }
  }
}
