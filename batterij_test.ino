#define BAT_PIN    32
#define LED_GREEN  23
#define LED_YELLOW 21
#define LED_RED    22

const float divider  = 4.87;   // (58K + 15K) / 15K
const float adcRef   = 3.3;
const float adcMax   = 4095.0;
const float BAT_FULL = 8.4;
const float BAT_EMPTY = 6.4;

float readBatteryVoltage() {
  // Meerdere metingen middelen voor stabiliteit (ESP32 ADC is niet super nauwkeurig)
  int som = 0;
  for (int i = 0; i < 10; i++) {
    som += analogRead(BAT_PIN);
    delay(10);
  }
  float raw = som / 10.0;
  return (raw / adcMax) * adcRef * divider;
}

int voltageToPct(float v) {
  int pct = (v - BAT_EMPTY) / (BAT_FULL - BAT_EMPTY) * 100;
  return constrain(pct, 0, 100);
}

void updateBatteryLEDs(int pct) {
  digitalWrite(LED_GREEN,  LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED,    LOW);

  if (pct > 30) {
    digitalWrite(LED_GREEN, HIGH);   // 31% - 100% → groen
  } else if (pct > 10) {
    digitalWrite(LED_YELLOW, HIGH);  // 11% - 30%  → geel
  } else {
    digitalWrite(LED_RED, HIGH);     // 0%  - 10%  → rood
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_GREEN,  OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED,    OUTPUT);
  Serial.println("=== Batterij test gestart ===");
  Serial.println("Sluit multimeter aan op BAT+ en GND om te vergelijken.");
  Serial.println("-------------------------------------");
}

void loop() {
  float voltage = readBatteryVoltage();
  int   pct     = voltageToPct(voltage);

  updateBatteryLEDs(pct);

  Serial.print("Spanning : ");
  Serial.print(voltage, 2);
  Serial.print("V  |  Percentage: ");
  Serial.print(pct);
  Serial.println("%");

  // Visuele balk in Serial monitor
  Serial.print("[");
  int blokjes = pct / 5;  // 20 blokjes totaal
  for (int i = 0; i < 20; i++) {
    Serial.print(i < blokjes ? "#" : "-");
  }
  Serial.println("]");
  Serial.println();

  delay(1000);
}