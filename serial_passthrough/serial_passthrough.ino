// Throwaway bridge: USB monitor at 115200 <-> C4001 on D6/D7 at 9600.
// Type a command (sensorStop, getRange, setRunApp 1, ...) and press send.
// Module TX goes straight to D6. Module RX is D7, preferably through the
// 1k/2.2k divider. Nothing is saved unless you type saveConfig yourself.
// D4 is held HIGH so a stacked TFT shield's SD slot stays off the bus.

#include <SoftwareSerial.h>

SoftwareSerial radarPort(6, 7);

void setup() {
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);
  Serial.begin(115200);
  radarPort.begin(9600);
  Serial.println(F("passthrough 115200 <-> 9600 D6/D7"));
}

void loop() {
  while (radarPort.available() > 0) {
    Serial.write(radarPort.read());
  }
  while (Serial.available() > 0) {
    radarPort.write(Serial.read());
  }
}
