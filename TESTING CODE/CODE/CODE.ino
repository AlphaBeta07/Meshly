#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);

  Serial.println("Ready");
}

void loop() {
  while (Serial.available()) {
    Serial2.write(Serial.read());
  }

  while (Serial2.available()) {
    Serial.write(Serial2.read());
  }
}
