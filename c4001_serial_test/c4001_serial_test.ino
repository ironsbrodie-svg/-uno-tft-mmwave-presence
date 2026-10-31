// C4001 UART test. Prints raw module readings to the USB serial port.
// The TFT is not used. Numbers here are NOT smoothed; the handheld filter
// lives in presence_display. Open the monitor at 115200 if you want the log.
// The sketch does not wait for the monitor.
//
// Keep RADAR_LINK identical to the line in presence_display.ino.
// Uno: RADAR_LINK_SOFT, sensor TX -> D6, sensor RX -> D7.
// Do not use D4. The TFT shield reserves it for the microSD chip select.

#define RADAR_LINK_SOFT 0
#define RADAR_LINK_SERIAL1 1
#define RADAR_LINK_SERIAL2 2
#define RADAR_LINK_SERIAL3 3
#define RADAR_LINK RADAR_LINK_SOFT

#define RADAR_RX_PIN 6
#define RADAR_TX_PIN 7
#define C4001_BAUD 9600
#define SD_CS_PIN 4

// Same gate as the indoor preset in presence_display.ino.
#define RANGE_MIN_CM 60
#define RANGE_MAX_CM 600
#define RANGE_THRESHOLD 22

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
#if RADAR_LINK != RADAR_LINK_SOFT
#error "Uno test builds use SoftwareSerial. Set RADAR_LINK to RADAR_LINK_SOFT."
#endif
#ifndef _SS_MAX_RX_BUFF
#define _SS_MAX_RX_BUFF 128
#endif
#else
#if RADAR_LINK == RADAR_LINK_SOFT
#error "Set RADAR_LINK to RADAR_LINK_SERIAL1 for a Mega (pins 19 RX / 18 TX)."
#endif
#endif

#include <DFRobot_C4001.h>

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
SoftwareSerial radarPort(RADAR_RX_PIN, RADAR_TX_PIN);
DFRobot_C4001_UART radar(&radarPort, C4001_BAUD);
#elif RADAR_LINK == RADAR_LINK_SERIAL1
DFRobot_C4001_UART radar(&Serial1, C4001_BAUD);
#elif RADAR_LINK == RADAR_LINK_SERIAL2
DFRobot_C4001_UART radar(&Serial2, C4001_BAUD);
#elif RADAR_LINK == RADAR_LINK_SERIAL3
DFRobot_C4001_UART radar(&Serial3, C4001_BAUD);
#else
#error "Set RADAR_LINK to a hardware serial port."
#endif

static Stream &radarLink() {
#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
  return radarPort;
#elif RADAR_LINK == RADAR_LINK_SERIAL1
  return Serial1;
#elif RADAR_LINK == RADAR_LINK_SERIAL2
  return Serial2;
#elif RADAR_LINK == RADAR_LINK_SERIAL3
  return Serial3;
#endif
}

static bool gConfigured = false;

static bool probeRadar() {
  Stream &link = radarLink();
  while (link.available() > 0) {
    link.read();
  }
  const char startCmd[] = "sensorStart";
  link.write(reinterpret_cast<const uint8_t *>(startCmd), sizeof(startCmd) - 1);
  const uint32_t startMs = millis();
  while ((uint32_t)(millis() - startMs) < 2000UL) {
    if (link.available() > 0 && link.read() == '$') {
      return true;
    }
  }
  return false;
}

static void configureRadar() {
  radar.setSensorMode(eSpeedMode);
  radar.setDetectThres(RANGE_MIN_CM, RANGE_MAX_CM, RANGE_THRESHOLD);
  radar.setFrettingDetection(eON);
  gConfigured = true;
  if (Serial.availableForWrite() > 16) {
    Serial.println(F("ranging mode, raw values follow"));
  }
}

void setup() {
  Serial.begin(115200);
  // Shield may be stacked. Keep the SD card off the bus if one is fitted.
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);

  if (Serial.availableForWrite() > 40) {
    Serial.println(F("C4001 raw serial test, 115200"));
#if RADAR_LINK == RADAR_LINK_SERIAL1
    Serial.println(F("Sensor TX -> pin 19, sensor RX -> pin 18, 9600"));
#elif RADAR_LINK == RADAR_LINK_SERIAL2
    Serial.println(F("Sensor TX -> pin 17, sensor RX -> pin 16, 9600"));
#elif RADAR_LINK == RADAR_LINK_SERIAL3
    Serial.println(F("Sensor TX -> pin 15, sensor RX -> pin 14, 9600"));
#else
    Serial.println(F("Sensor TX -> D6, sensor RX -> D7, 9600"));
#endif
    Serial.println(F("D4 is held HIGH (shield SD chip select)"));
  }

  radar.begin();
  if (probeRadar()) {
    if (Serial.availableForWrite() > 24) {
      Serial.println(F("frame seen, configuring (10-20 s)"));
    }
    configureRadar();
  } else if (Serial.availableForWrite() > 24) {
    Serial.println(F("no frame yet, check power and TX/RX"));
  }
}

void loop() {
  static uint32_t lastMs = 0;
  const uint32_t now = millis();
  if (!gConfigured) {
    if ((uint32_t)(now - lastMs) < 2000UL) {
      return;
    }
    lastMs = now;
    if (!probeRadar()) {
      if (Serial.availableForWrite() > 12) {
        Serial.println(F("no frame"));
      }
      return;
    }
    if (Serial.availableForWrite() > 24) {
      Serial.println(F("frame seen, configuring (10-20 s)"));
    }
    configureRadar();
    return;
  }
  if ((uint32_t)(now - lastMs) < 300UL) {
    return;
  }
  lastMs = now;
  if (Serial.availableForWrite() < 40) {
    return;
  }

  const uint8_t count = radar.getTargetNumber();
  if (count == 0) {
    Serial.println(F("clear"));
    return;
  }
  Serial.print(F("targets="));
  Serial.print(count);
  Serial.print(F("  m="));
  Serial.print(radar.getTargetRange(), 2);
  Serial.print(F("  m/s="));
  Serial.print(radar.getTargetSpeed(), 2);
  Serial.print(F("  e="));
  Serial.println(radar.getTargetEnergy());
}
