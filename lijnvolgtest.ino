// =============================================
// LIJNVOLGTEST - 3 DIGITALE IR SENSOREN
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

int baseSpeed  = 130;  // basissnelheid (0-255)
int correction = 50;   // bijsturingssterkte

// Moving average buffers
int bufL[Q] = {};
int bufM[Q] = {};
int bufR[Q] = {};
int bufIndex = 0;

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
}

void setMotors(int leftSpeed, int rightSpeed) {
  leftSpeed  = constrain(leftSpeed,  0, 255);
  rightSpeed = constrain(rightSpeed, 0, 255);
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  ledcWrite(ENA, leftSpeed);
  ledcWrite(ENB, rightSpeed);
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
    setMotors(baseSpeed, baseSpeed);
  }
  else if (!L && M && !R) {
    Serial.println("RECHTDOOR");
    setMotors(baseSpeed, baseSpeed);
  }
  else if (L && !M && !R) {
    Serial.println("LINKS");
    setMotors(baseSpeed + correction, baseSpeed - correction);
  }
  else if (!L && !M && R) {
    Serial.println("RECHTS");
    setMotors(baseSpeed - correction, baseSpeed + correction);
  }
  else if (L && M && !R) {
    Serial.println("LINKS (M+L)");
    setMotors(baseSpeed + correction, baseSpeed - correction);
  }
  else if (!L && M && R) {
    Serial.println("RECHTS (M+R)");
    setMotors(baseSpeed - correction, baseSpeed + correction);
  }
  else if (L && M && R) {
    Serial.println("DWARSLIJN - STOP");
    stopMotors();
  }
  else {
    Serial.println("ONBEKEND - STOP");
    stopMotors();
  }

}
