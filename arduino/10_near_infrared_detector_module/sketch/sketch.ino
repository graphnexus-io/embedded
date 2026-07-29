#define INFRARED_ANALOG_INPUT A0
#define INFRARED_DIGITAL_INPUT 2

const unsigned long SAMPLE_INTERVAL_MS = 100;

void setup() {
  Serial.begin(9600);
  pinMode(INFRARED_DIGITAL_INPUT, INPUT);
}

void printTelemetry(int infraredIntensity, int digitalSignal) {
  Serial.print("INFRARED_INTENSITY=");
  Serial.print(infraredIntensity);
  Serial.print(",DIGITAL_SIGNAL=");
  Serial.println(digitalSignal);
}

void loop() {
  int infraredIntensity = analogRead(INFRARED_ANALOG_INPUT);
  int digitalSignal = digitalRead(INFRARED_DIGITAL_INPUT);

  printTelemetry(infraredIntensity, digitalSignal);
  delay(SAMPLE_INTERVAL_MS);
}
