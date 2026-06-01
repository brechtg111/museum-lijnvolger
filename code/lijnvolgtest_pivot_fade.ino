// =============================================
// LIJNVOLGTEST - 3 DIGITALE IR SENSOREN
// Met pivot turn, fade en achteruit test
// =============================================

#define IR_L 34
#define IR_M 35
#define IR_R 33

#define ENA 19
#define IN1 18
#define IN2 5
#define ENB 16
#define IN3 4
#define IN4 2

#define Q 8

int baseSpeed  = 170;
int correction = 50;

// Moving average buffers
int bufL[Q] = {};
int bufM[Q] = {};
int bufR[Q] = {};
int bufIndex = 0;

// Huidige snelheid bijhouden voor fade
int currentLeft  = 0;
int currentRight = 0;

bool majorityVote(int* buf) {
  int sum = 0;
  for (int i = 0; i < Q; i++) sum += buf[i];
  return sum > (Q / 2);
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);
  currentLeft  = 0;
  currentRight = 0;
}

void setMotors(int leftSpeed, int rightSpeed) {
  // Links wiel richting
  if (leftSpeed >= 0) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  } else {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    leftSpeed = -leftSpeed;
  }

  // Rechts wiel richting
  if (rightSpeed >= 0) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    rightSpeed = -rightSpeed;
  }

  ledcWrite(ENA, constrain(leftSpeed,  0, 255));
  ledcWrite(ENB, constrain(rightSpeed, 0, 255));
}

// Fade naar doelsnelheid (alleen voor rechtdoor)
void setMotorsFade(int targetLeft, int targetRight, int step = 5) {
  // Richting instellen
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);

  // Geleidelijk naar doelsnelheid
  while (currentLeft != targetLeft || currentRight != targetRight) {
    if (currentLeft < targetLeft)  currentLeft  = min(currentLeft  + step, targetLeft);
    if (currentLeft > targetLeft)  currentLeft  = max(currentLeft  - step, targetLeft);
    if (currentRight < targetRight) currentRight = min(currentRight + step, targetRight);
    if (currentRight > targetRight) currentRight = max(currentRight - step, targetRight);

    ledcWrite(ENA, constrain(currentLeft,  0, 255));
    ledcWrite(ENB, constrain(currentRight, 0, 255));
    delay(5);
  }
}

// Test achteruit rijden bij opstart
void testAchteruit() {
  Serial.println("=== TEST: 1 seconde achteruit ===");
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // links achteruit
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // rechts achteruit
  ledcWrite(ENA, 100);
  ledcWrite(ENB, 100);
  delay(1000);
  stopMotors();
  delay(500);
  Serial.println("=== TEST KLAAR ===");
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_L, INPUT);
  pinMode(IR_M, INPUT);
  pinMode(IR_R, INPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  ledcAttach(ENA, 5000, 8);
  ledcAttach(ENB, 5000, 8);

  // Buffers voorvullen
  for (int i = 0; i < Q; i++) {
    bufL[i] = digitalRead(IR_L);
    bufM[i] = digitalRead(IR_M);
    bufR[i] = digitalRead(IR_R);
  }

  // Test achteruit bij opstart
  testAchteruit();

  Serial.println("Lijnvolgtest gestart!");
}

void loop() {
  // Sensoren uitlezen
  bufL[bufIndex] = digitalRead(IR_L);
  bufM[bufIndex] = digitalRead(IR_M);
  bufR[bufIndex] = digitalRead(IR_R);
  bufIndex = (bufIndex + 1) % Q;

  bool L = majorityVote(bufL);
  bool M = majorityVote(bufM);
  bool R = majorityVote(bufR);

  // Debug
  Serial.print("L="); Serial.print(L ? "LIJN" : "WIT ");
  Serial.print("  M="); Serial.print(M ? "LIJN" : "WIT ");
  Serial.print("  R="); Serial.print(R ? "LIJN" : "WIT ");
  Serial.print("  → ");

  // Rijlogica
  if (!L && !M && !R) {
    Serial.println("RECHTDOOR (geen lijn)");
    setMotorsFade(baseSpeed, baseSpeed);
  }
  else if (!L && M && !R) {
    Serial.println("RECHTDOOR");
    setMotorsFade(baseSpeed, baseSpeed);
  }
  else if (L && !M && !R) {
    // Pivot links: links vooruit, rechts achteruit
    Serial.println("PIVOT LINKS");
    currentLeft = 0; currentRight = 0;
    setMotors(baseSpeed, -correction);
  }
  else if (!L && !M && R) {
    // Pivot rechts: rechts vooruit, links achteruit
    Serial.println("PIVOT RECHTS");
    currentLeft = 0; currentRight = 0;
    setMotors(-correction, baseSpeed);
  }
  else if (L && M && !R) {
    Serial.println("LINKS (M+L)");
    currentLeft = 0; currentRight = 0;
    setMotors(baseSpeed, baseSpeed - correction);
  }
  else if (!L && M && R) {
    Serial.println("RECHTS (M+R)");
    currentLeft = 0; currentRight = 0;
    setMotors(baseSpeed - correction, baseSpeed);
  }
  else if (L && M && R) {
    Serial.println("DWARSLIJN - STOP");
    stopMotors();
  }
  else {
    Serial.println("ONBEKEND - STOP");
    stopMotors();
  }

  delay(10);
}
