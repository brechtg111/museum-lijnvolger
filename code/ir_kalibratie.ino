#define IR_L 34
#define IR_M 35
#define IR_R 33

#define Q 8

int bufL[Q] = {};
int bufM[Q] = {};
int bufR[Q] = {};
int bufIndex = 0;

bool majorityVote(int* buf) {
  int sum = 0;
  for (int i = 0; i < Q; i++) sum += buf[i];
  return sum > (Q / 2);
}

void setup() {
  Serial.begin(115200);
  pinMode(IR_L, INPUT);
  pinMode(IR_M, INPUT);
  pinMode(IR_R, INPUT);

  // Buffers voorvullen
  for (int i = 0; i < Q; i++) {
    bufL[i] = digitalRead(IR_L);
    bufM[i] = digitalRead(IR_M);
    bufR[i] = digitalRead(IR_R);
  }
}

void loop() {
  bufL[bufIndex] = digitalRead(IR_L);
  bufM[bufIndex] = digitalRead(IR_M);
  bufR[bufIndex] = digitalRead(IR_R);
  bufIndex = (bufIndex + 1) % Q;

  bool L = majorityVote(bufL);
  bool M = majorityVote(bufM);
  bool R = majorityVote(bufR);

  Serial.print("L="); Serial.print(L ? "LIJN" : "WIT ");
  Serial.print("  M="); Serial.print(M ? "LIJN" : "WIT ");
  Serial.print("  R="); Serial.println(R ? "LIJN" : "WIT ");

  delay(10);
}
