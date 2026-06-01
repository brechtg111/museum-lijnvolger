// =============================================
// LIJNVOLGTEST - 3 DIGITALE IR SENSOREN
// Met pivot turn voor scherpe bochten
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

int baseSpeed  = 160;  // basissnelheid (0-255)
int correction = 50;   // bijsturingssterkte zachte bocht

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
    // Geen sensor ziet lijn → rechtdoor
    Serial.println("RECHTDOOR (geen lijn)");
    setMotors(baseSpeed, baseSpeed);
  }
  else if (!L && M && !R) {
    // Alleen midden → rechtdoor
    Serial.println("RECHTDOOR");
    setMotors(baseSpeed, baseSpeed);
  }
  else if (L && !M && !R) {
    // Alleen links → pivot turn links (links achteruit, rechts vooruit)
    Serial.println("PIVOT LINKS");
    setMotors(-correction, baseSpeed);
  }
  else if (!L && !M && R) {
    // Alleen rechts → pivot turn rechts (rechts achteruit, links vooruit)
    Serial.println("PIVOT RECHTS");
    setMotors(baseSpeed, -correction);
  }
  else if (L && M && !R) {
    // Midden + links → zachte bocht links
    Serial.println("LINKS (M+L)");
    setMotors(baseSpeed - correction, baseSpeed);
  }
  else if (!L && M && R) {
    // Midden + rechts → zachte bocht rechts
    Serial.println("RECHTS (M+R)");
    setMotors(baseSpeed, baseSpeed - correction);
  }
  else if (L && M && R) {
    // Alle sensoren → dwarslijn, stop
    Serial.println("DWARSLIJN - STOP");
    stopMotors();
  }
  else {
    // Onbekend → stop
    Serial.println("ONBEKEND - STOP");
    stopMotors();
  }

  delay(10);
}
