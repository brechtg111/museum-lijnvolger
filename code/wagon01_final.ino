#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// =============================================
// NETWERK EN MQTT CONFIGURATIE
// =============================================
const char* ssid        = "brechtiot";
const char* password    = "brechtiot";
const char* mqtt_server = "192.168.1.154";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "wagon01";
const char* mqtt_pass   = "pi123";

// MQTT Topics
const char* TOPIC_TELEMETRY = "museum/wagon/01/telemetry";
const char* TOPIC_ALERT     = "museum/wagon/01/event/error";
const char* TOPIC_CMD_STOP  = "museum/wagon/01/cmd/stop";
const char* TOPIC_CMD_START = "museum/wagon/01/cmd/start";
const char* TOPIC_BROADCAST = "museum/system/broadcast/stop";

// =============================================
// PIN DEFINITIES
// =============================================
#define IR_L 34
#define IR_M 35
#define IR_R 33   // verplaatst van 17 naar 33 (ADC1_CH5)

#define ENA 19
#define IN1 18
#define IN2 5
#define ENB 16
#define IN3 4
#define IN4 2

#define BTN_START  15
#define BTN_STOP   26

#define BAT_PIN    32
#define LED_GREEN  23
#define LED_YELLOW 21
#define LED_RED    22

// =============================================
// IR SENSOR KALIBRATIE + MOVING AVERAGE Q=8
// =============================================
#define Q 8

int calBlack[3] = {0, 0, 0};
int calWhite[3] = {0, 0, 0};

int bufL[Q] = {}, bufM[Q] = {}, bufR[Q] = {};
int bufIndex = 0;

int movingAvg(int* buf, int newVal) {
  buf[bufIndex] = newVal;
  long sum = 0;
  for (int i = 0; i < Q; i++) sum += buf[i];
  return sum / Q;
}

int normalize(int raw, int white, int black) {
  int val = map(raw, white, black, 0, 100);
  return constrain(val, 0, 100);
}

void calibrateSensors() {
  Serial.println("=== KALIBRATIE START ===");

  Serial.println("Plaats sensoren op WIT oppervlak. Druk START...");
  while (digitalRead(BTN_START) == HIGH) delay(10);
  delay(200);

  long sumW[3] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    sumW[0] += analogRead(IR_L);
    sumW[1] += analogRead(IR_M);
    sumW[2] += analogRead(IR_R);
    delay(5);
  }
  for (int i = 0; i < 3; i++) calWhite[i] = sumW[i] / 100;
  Serial.printf("WIT:   L=%d  M=%d  R=%d\n", calWhite[0], calWhite[1], calWhite[2]);

  Serial.println("Plaats sensoren op ZWARTE lijn. Druk START...");
  while (digitalRead(BTN_START) == HIGH) delay(10);
  delay(200);

  long sumB[3] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    sumB[0] += analogRead(IR_L);
    sumB[1] += analogRead(IR_M);
    sumB[2] += analogRead(IR_R);
    delay(5);
  }
  for (int i = 0; i < 3; i++) calBlack[i] = sumB[i] / 100;
  Serial.printf("ZWART: L=%d  M=%d  R=%d\n", calBlack[0], calBlack[1], calBlack[2]);

  Serial.println("=== KALIBRATIE KLAAR ===");
}

void readSensors(int &outL, int &outM, int &outR) {
  int rawL = movingAvg(bufL, analogRead(IR_L));
  int rawM = movingAvg(bufM, analogRead(IR_M));
  int rawR = movingAvg(bufR, analogRead(IR_R));
  bufIndex = (bufIndex + 1) % Q;

  outL = normalize(rawL, calWhite[0], calBlack[0]);
  outM = normalize(rawM, calWhite[1], calBlack[1]);
  outR = normalize(rawR, calWhite[2], calBlack[2]);
}

// =============================================
// RIJPARAMETERS EN STATUS
// =============================================
#define DREMPEL 50

int baseSpeed  = 180;
int correction = 40;
bool running   = false;

unsigned long stopTime             = 0;
bool stopTimerActive               = false;
const unsigned long STOP_DURATION  = 30000;

// =============================================
// MQTT EN WIFI
// =============================================
WiFiClient espClient;
PubSubClient mqttClient(espClient);

struct SensorData {
  int irL = 0, irM = 0, irR = 0;
  float battery_v   = 8.4;
  int battery_pct   = 100;
  String status     = "STOP";
} sensorData;

unsigned long lastMqttPublish = 0;
const long mqttInterval = 2000;

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("WiFi verbinden");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.setSleep(false);  // ← toevoegen
  Serial.println("\nVerbonden! IP: " + WiFi.localIP().toString());
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
}

void setMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  0, 255);
  rightSpeed = constrain(rightSpeed, 0, 255);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  ledcWrite(ENA, leftSpeed);
  ledcWrite(ENB, rightSpeed);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t   = String(topic);
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("MQTT ontvangen [" + t + "]: " + msg);

  if (t == TOPIC_CMD_STOP || t == TOPIC_BROADCAST) {
    running         = false;
    stopTimerActive = true;
    stopTime        = millis();
    stopMotors();
  } else if (t == TOPIC_CMD_START) {
    running         = true;
    stopTimerActive = false;
  }
}

void connectMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  while (!mqttClient.connected()) {
    Serial.print("MQTT verbinden...");
    String clientId = "ESP32-wagon01";
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("verbonden");
      mqttClient.subscribe(TOPIC_CMD_STOP);
      mqttClient.subscribe(TOPIC_CMD_START);
      mqttClient.subscribe(TOPIC_BROADCAST);
      mqttClient.publish(TOPIC_TELEMETRY, "{\"status\":\"online\"}");
    } else {
      Serial.print("mislukt, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" opnieuw in 2s");
      delay(2000);
    }
  }
}

float readBatteryVoltage() {
  const float divider = 4.87;
  const float adcRef  = 3.3;
  const float adcMax  = 4095.0;
  int raw = analogRead(BAT_PIN);
  return (raw / adcMax) * adcRef * divider;
}

int voltageToPct(float v) {
  const float full  = 8.4;
  const float empty = 6.4;
  int pct = (v - empty) / (full - empty) * 100;
  return constrain(pct, 0, 100);
}

void updateBatteryLEDs(int pct) {
  digitalWrite(LED_GREEN,  pct > 30              ? HIGH : LOW);
  digitalWrite(LED_YELLOW, pct <= 30 && pct > 10 ? HIGH : LOW);
  digitalWrite(LED_RED,    pct <= 10             ? HIGH : LOW);
}

void publishMQTT() {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<256> doc;
  doc["battery_v"]   = sensorData.battery_v;
  doc["battery_pct"] = sensorData.battery_pct;
  doc["ir_left"]     = sensorData.irL;
  doc["ir_mid"]      = sensorData.irM;
  doc["ir_right"]    = sensorData.irR;
  doc["status"]      = sensorData.status;

  char buf[256];
  serializeJson(doc, buf);
  mqttClient.publish(TOPIC_TELEMETRY, buf);

  if (sensorData.battery_pct <= 10) {
    StaticJsonDocument<64> alert;
    alert["type"]    = "low_battery";
    alert["battery"] = sensorData.battery_pct;
    char alertBuf[64];
    serializeJson(alert, alertBuf);
    mqttClient.publish(TOPIC_ALERT, alertBuf);
  }
}

// =============================================
// SETUP
// =============================================
void setup() {
  Serial.begin(115200);

  // Pin Modes
  pinMode(IR_L, INPUT);
  pinMode(IR_M, INPUT);
  pinMode(IR_R, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(BTN_START, INPUT_PULLUP);
  pinMode(BTN_STOP,  INPUT_PULLUP);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);

  stopMotors();
  ledcAttach(ENA, 5000, 8);
  ledcAttach(ENB, 5000, 8);

  // ADC instellen voor 0-3.3V bereik
  analogSetPinAttenuation(IR_L, ADC_11db);
  analogSetPinAttenuation(IR_M, ADC_11db);
  analogSetPinAttenuation(IR_R, ADC_11db);

  // Netwerkverbindingen opbouwen
  connectWiFi();
  connectMQTT();

  // Moving average buffers voorvullen (voorkomt nullen bij opstart)
  for (int i = 0; i < Q; i++) {
    bufL[i] = analogRead(IR_L);
    bufM[i] = analogRead(IR_M);
    bufR[i] = analogRead(IR_R);
    delay(5);
  }

  // Kalibratie uitvoeren
  calibrateSensors();

  Serial.println("Systeem klaar voor gebruik.");
}

// =============================================
// LOOP
// =============================================
void loop() {

    Serial.printf("RAW → L=%d  M=%d  R=%d\n", 
    analogRead(IR_L), 
    analogRead(IR_M), 
    analogRead(IR_R));
  delay(500);

  // WiFi-herstel
  if (WiFi.status() != WL_CONNECTED) {
    stopMotors();
    running = false;
    connectWiFi();
  }

  // MQTT-herstel en loop
  if (!mqttClient.connected()) connectMQTT();
  mqttClient.loop();

  // Fysieke knoppen
  static bool lastBtnStart = HIGH;
  static bool lastBtnStop  = HIGH;

  bool currentBtnStart = digitalRead(BTN_START);
  bool currentBtnStop  = digitalRead(BTN_STOP);

  if (currentBtnStart == LOW && lastBtnStart == HIGH) {
    Serial.println("[KNOP] START ingedrukt → wagon rijdt");
    running         = true;
    stopTimerActive = false;
  }

  if (currentBtnStop == LOW && lastBtnStop == HIGH) {
    Serial.println("[KNOP] STOP ingedrukt → wagon stopt 30s");
    running         = false;
    stopTimerActive = true;
    stopTime        = millis();
    stopMotors();
  }

  lastBtnStart = currentBtnStart;
  lastBtnStop  = currentBtnStop;

  // Auto-herstart na 30 seconden
  if (stopTimerActive && (millis() - stopTime >= STOP_DURATION)) {
    running         = true;
    stopTimerActive = false;
  }

  // Batterij uitlezen en LEDs aansturen
  float v = readBatteryVoltage();
  int pct = voltageToPct(v);
  sensorData.battery_v   = v;
  sensorData.battery_pct = pct;
  updateBatteryLEDs(pct);

  // Noodstop bij lege batterij
  if (pct <= 10) {
    running         = false;
    stopTimerActive = false;
    stopMotors();
    sensorData.status = "ALARM_BATTERY";
  }

  // IR sensoren uitlezen via ADC + moving average + normalisatie
  int normL, normM, normR;
  readSensors(normL, normM, normR);
  sensorData.irL = normL;
  sensorData.irM = normM;
  sensorData.irR = normR;

  // Rijlogica
  if (!running) {
    if (sensorData.status != "ALARM_BATTERY") {
      sensorData.status = "STOP";
    }
    stopMotors();
  } else {
    bool L = normL > DREMPEL;
    bool M = normM > DREMPEL;
    bool R = normR > DREMPEL;

    // Dwarslijn: alle 3 sensoren zien lijn → stop 30 seconden
    if (L && M && R) {
      running           = false;
      stopTimerActive   = true;
      stopTime          = millis();
      sensorData.status = "DWARSLIJN_STOP";
      stopMotors();
    }
    // Alleen midden → rechtdoor
    else if (M && !L && !R) {
      sensorData.status = "RIJDEN";
      setMotors(baseSpeed, baseSpeed);
    }
    // Lijn naar rechts → stuur rechts bij
    else if (R && !M && !L) {
      sensorData.status = "CORRECTIE_RECHTS";
      setMotors(baseSpeed + correction, baseSpeed - correction);
    }
    // Lijn naar links → stuur links bij
    else if (L && !M && !R) {
      sensorData.status = "CORRECTIE_LINKS";
      setMotors(baseSpeed - correction, baseSpeed + correction);
    }
    // Midden + rechts → sterker rechts bijsturen
    else if (M && R && !L) {
      sensorData.status = "CORRECTIE_RECHTS";
      setMotors(baseSpeed + correction, baseSpeed - correction);
    }
    // Midden + links → sterker links bijsturen
    else if (M && L && !R) {
      sensorData.status = "CORRECTIE_LINKS";
      setMotors(baseSpeed - correction, baseSpeed + correction);
    }
    // Geen lijn → lijn verloren
    else if (!L && !M && !R) {
      sensorData.status = "LIJN_VERLOREN";
      stopMotors();
    }
    else {
      stopMotors();
    }
  }

  // Periodiek MQTT publiceren
  if (millis() - lastMqttPublish > mqttInterval) {
    lastMqttPublish = millis();
    publishMQTT();
  }

  delay(10);
}
