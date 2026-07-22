#define SIGNAL_PIN A0


void setup() {
  Serial.begin(9600);
}

void loop() {
  int waterLevelSignal = analogRead(SIGNAL_PIN);
  Serial.println(waterLevelSignal);
  delay(500);
}
