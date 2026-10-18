#include "HardwareSerial.h"
#include "esp32-hal-gpio.h"
#include "esp32-hal.h"
#include <Arduino.h>
#include <HC_SR04.h>
#include <cstdint>
#include <sys/_types.h>

constexpr int CYCLE_MS = 100;

enum Operation {
  IDLE = -1,
  OFF,
  ON,
};
enum LedColor { RED = 0, GREEN, BLUE };
enum Proximity { FAR = 0, MID, NEAR, CLOSE, DANGER };

constexpr uint8_t LED[3] = {25, 26, 27};
constexpr uint8_t CONTROL[2] = {32, 33};
constexpr uint8_t BUZZER = 18;

constexpr uint8_t TRIGGER = 22;
constexpr uint8_t ECHO = 23;
HC_SR04<ECHO> sensor(TRIGGER);

Operation buzzerStatus = IDLE;
Operation sensorStatus = IDLE;
long mmDistance = 0;

constexpr int BUZZ_DEFAULT_MS = 300;
constexpr int BUZZ_SHORT_MS = 200;
constexpr int PROXIMITY[4] = {300, 150, 100, 50};
constexpr int INTERVAL[4] = {1000, 500, 350, 200};

unsigned long lastBuzzerStatus = 0;

void setLedColor(LedColor color, int value = 150) {
  analogWrite(LED[color], value);
}

void setBuzzerStatus(Operation status) {
  lastBuzzerStatus = millis();
  buzzerStatus = status;
  switch (status) {
  case IDLE:
    break;
  case OFF:
    digitalWrite(BUZZER, LOW);
    break;
  case ON:
    digitalWrite(BUZZER, HIGH);
    break;
  }
}

void setSensorStatus(Operation status) {
  sensorStatus = status;

  if (status == ON) {
    sensor.startAsync(100000);
    setLedColor(BLUE, 255);
  } else if (status == OFF) {
    setBuzzerStatus(OFF);
    setLedColor(BLUE, 10);
  }
}

void setupLed() {
  for (int i = 0; i < 3; i++) {
    const uint8_t pin = LED[i];
    pinMode(pin, OUTPUT);
    setLedColor((LedColor)i, 0);
  }
}

void setupControl() {
  for (int i = 0; i < 2; i++) {
    pinMode(CONTROL[i], INPUT_PULLUP);
  }
}

void setupBuzzer() {
  pinMode(BUZZER, OUTPUT);
  setBuzzerStatus(OFF);
}

void setupSensor() { sensor.beginAsync(); }

bool isControlPressed(Operation operation) {
  switch (operation) {
  case IDLE:
    return false;
  case OFF:
  case ON:
    return digitalRead(CONTROL[operation]) == LOW;
  }
}

Operation getPressedControl() {
  if (digitalRead(CONTROL[OFF]) == LOW) {
    return OFF;
  } else if (digitalRead(CONTROL[ON]) == LOW) {
    return ON;
  } else {
    return IDLE;
  }
}

void updateStatus() {
  const Operation control = getPressedControl();
  if (control != IDLE && sensorStatus != control) {
    setSensorStatus(control);
  }
}

void printDistance(long mmDistance) {
  const float cmDistance = (float)mmDistance / 10;

  Serial.print('\r');
  Serial.print(cmDistance);
  Serial.print(" cm");
}

void toggleBuzzer(long mmDistance) {
  switch (buzzerStatus) {
  case IDLE:
    break;
  case OFF:
    setBuzzerStatus(ON);
    break;
  case ON:
    setBuzzerStatus(OFF);
    break;
  }
}

Proximity getProximity(long mmDistance) {
  if (mmDistance > PROXIMITY[FAR]) {
    return FAR;
  } else if (mmDistance > PROXIMITY[MID]) {
    return MID;
  } else if (mmDistance > PROXIMITY[NEAR]) {
    return NEAR;
  } else if (mmDistance > PROXIMITY[CLOSE]) {
    return CLOSE;
  } else {
    return DANGER;
  }
}

bool shouldToggleBuzzer(Proximity proximity) {
  unsigned long now = millis();
  unsigned long elapsed = now - lastBuzzerStatus;

  bool toggleOn =
      buzzerStatus == ON &&
      (((proximity == FAR || proximity == MID) && elapsed > BUZZ_DEFAULT_MS) ||
       (proximity == NEAR || proximity == CLOSE) && elapsed > BUZZ_SHORT_MS);
  bool toggleFar =
      proximity == FAR && buzzerStatus == OFF && elapsed > INTERVAL[FAR];
  bool toggleMid =
      proximity == MID && buzzerStatus == OFF && elapsed > INTERVAL[MID];
  bool toggleNear =
      proximity == NEAR && buzzerStatus == OFF && elapsed > INTERVAL[NEAR];
  bool toggleClose =
      proximity == CLOSE && buzzerStatus == OFF && elapsed > INTERVAL[CLOSE];
  bool danger = proximity == DANGER && buzzerStatus == OFF;

  return toggleOn || toggleFar || toggleMid || toggleNear || toggleClose ||
         danger;
}

void measureDistance() {
  if (sensor.isFinished()) {
    mmDistance = sensor.getDist_mm();
    printDistance(mmDistance);
    sensor.startAsync();
  }

  Proximity proximity = getProximity(mmDistance);
  if (shouldToggleBuzzer(proximity)) {
    toggleBuzzer(mmDistance);
  }
}

void setup() {
  Serial.begin(115200);

  setupLed();
  setupControl();
  setupBuzzer();
  setupSensor();

  delay(2000);
  setSensorStatus(OFF);
}

void loop() {
  updateStatus();

  switch (sensorStatus) {
  case IDLE:
    Serial.println("IDLE...");
    break;
  case OFF:
    break;
  case ON:
    measureDistance();
    break;
  }

  delay(CYCLE_MS);
}
