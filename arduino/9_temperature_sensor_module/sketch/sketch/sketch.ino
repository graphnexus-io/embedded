#define ANALOG_INPUT A0
#define DIGITAL_INPUT 2
#define DIGITAL_OUTPUT 3

const unsigned long SWITCH_DELAY_MS = 1000;

int fanState = LOW;
int pendingState = LOW;
unsigned long stateChangeStarted = 0;

void setup() {
  Serial.begin(9600);

  pinMode(DIGITAL_INPUT, INPUT);
  pinMode(DIGITAL_OUTPUT, OUTPUT);

  digitalWrite(DIGITAL_OUTPUT, fanState);
}

void printTelemetry(int asignal, int dsignal) {
  Serial.print("ANALOG_SIGNAL=");
  Serial.print(asignal);
  Serial.print(",DIGITAL_SIGNAL=");
  Serial.println(dsignal);
}

void controlFan(int dsignal) {
  if (dsignal != pendingState) {
    pendingState = dsignal;
    stateChangeStarted = millis();
  }

  if (
    pendingState != fanState &&
    millis() - stateChangeStarted >= SWITCH_DELAY_MS
  ) {
    fanState = pendingState;
    digitalWrite(DIGITAL_OUTPUT, fanState);
  }
}

void loop() {
  int asignal = analogRead(ANALOG_INPUT);
  int dsignal = digitalRead(DIGITAL_INPUT);

  printTelemetry(asignal, dsignal);
  controlFan(dsignal);

  delay(100);
}
