#include <Arduino.h>

#define DEBUG 1

const bool USE_BUZZER = true;
const bool IS_TEMIC_MODE = true;

const int fanPin = 9;
const int thermistorPin = A0;
const int acPin = A2;
const int relayPin = 3;
const int buttonPin = A3;
const int buzzerPin = 5;

const float SERIES_RESISTOR = 10000.0f;
const float THERMISTOR_NOMINAL = 10000.0f;
const float TEMPERATURE_NOMINAL = 25.0f;
const float B_COEFFICIENT = 3950.0f;
const int TEMP_OFFSET = -2;

const unsigned long PERIOD_MICROS = 100000UL;

bool fanIsActive = false;
bool overheatActive = false;
unsigned long relayTimer = 0;
const unsigned long RELAY_DELAY = 5000;

float physDuty = 10.0f;
float targetPhysDuty = 10.0f;
float stableTargetDuty = 10.0f;
unsigned long dutyTimer = 0;
const unsigned long DUTY_DELAY = 3000;

int mode = 0; 
float manualPhysDuty = 10.0f;

unsigned long lastUpdateTime = 0;
unsigned long pwmStartTime = 0;
bool isSensorError = false;
bool acActive = false;

bool stableBtnState = HIGH;
unsigned long btnPressTime = 0;
bool isHoldHandled = false;
int clickCount = 0;
unsigned long lastClickTime = 0;
const unsigned long MULTI_CLICK_TIMEOUT = 350;

unsigned long lastTurboBeepTime = 0;

int getTemperatureNTC(int rawAnalog);
bool getFilteredACState();
void processNonBlockingSerial();

void setup() {
  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);

  pinMode(acPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

#if DEBUG
  Serial.begin(115200);
  Serial.println(F("\n=========================================="));
  Serial.println(F("       ESG/TEMIC CONTROLLER v2.4          "));
  Serial.println(F("=========================================="));
  Serial.print(F("System Mode: "));
  if (IS_TEMIC_MODE) {
    Serial.println(F("TEMIC (Min AC Duty: 50%, Relay: OFF)"));
  } else {
    Serial.println(F("ESG (Min AC Duty: 30%, Relay: ON)"));
  }
  Serial.println(F("Status: System Ready.\n"));
#endif

  if (USE_BUZZER) {
    tone(buzzerPin, 2000, 80);
  }

  pwmStartTime = micros();
}

void loop() {
  processNonBlockingSerial();

  int btnADC = analogRead(buttonPin);
  bool rawPinState = (btnADC < 100) ? LOW : HIGH;

  if (rawPinState == LOW && stableBtnState == HIGH) {
    stableBtnState = LOW;
    btnPressTime = millis();
    isHoldHandled = false;
  } else if (rawPinState == HIGH && stableBtnState == LOW) {
    stableBtnState = HIGH;
    unsigned long holdDuration = millis() - btnPressTime;

    if (btnPressTime > 0 && !isHoldHandled && holdDuration >= 40) {
      if (mode != 0) {
        mode = 0;
        if (USE_BUZZER) tone(buzzerPin, 1200, 60);
        clickCount = 0;
      } else {
        clickCount++;
        lastClickTime = millis();
      }
    }
    btnPressTime = 0;
  }

  if (stableBtnState == LOW && btnPressTime > 0) {
    unsigned long currentHold = millis() - btnPressTime;
    if (currentHold >= 3000 && currentHold < 5000 && !isHoldHandled) {
      mode = 2;
      if (USE_BUZZER) tone(buzzerPin, 2500, 150);
      isHoldHandled = true;
      clickCount = 0;
    }
  }

  if (clickCount > 0 && (millis() - lastClickTime >= MULTI_CLICK_TIMEOUT)) {
    if (mode == 0) {
      if (clickCount == 1) {
        mode = 1;
        if (USE_BUZZER) tone(buzzerPin, 1800, 80);
      } else if (clickCount >= 2) {
        mode = 2;
        if (USE_BUZZER) tone(buzzerPin, 2500, 150);
      }
    }
    clickCount = 0;
  }

  if (mode == 2 && USE_BUZZER) {
    if (millis() - lastTurboBeepTime >= 1500) {
      lastTurboBeepTime = millis();
      tone(buzzerPin, 1000, 50);
    }
  }

  static unsigned long lastLogicCycle = 0;
  static int currentTemp = 0;
  static int rawAnalog = 0;
  static bool targetRelayState = false;

  if (millis() - lastLogicCycle >= 150) {
    lastLogicCycle = millis();

    acActive = getFilteredACState();
    rawAnalog = analogRead(thermistorPin);
    currentTemp = getTemperatureNTC(rawAnalog);

    bool bypassFilter = false;

    if (mode == 99) {
      targetPhysDuty = manualPhysDuty;
      targetRelayState = false;
      bypassFilter = true;
      isSensorError = false;
    } else if (mode == 1) {
      targetPhysDuty = 90.0f;
      targetRelayState = false;
      bypassFilter = true;
      isSensorError = false;
    } else if (mode == 2) {
      targetPhysDuty = 90.0f;
      targetRelayState = !IS_TEMIC_MODE;
      bypassFilter = true;
      isSensorError = false;
    } else {
      if (rawAnalog >= 1018 || rawAnalog <= 5 || currentTemp == -99) {
        targetPhysDuty = 90.0f;
        targetRelayState = !IS_TEMIC_MODE;
        bypassFilter = true;
        isSensorError = true;
      } else {
        isSensorError = false;

        if (!fanIsActive && currentTemp >= 88) fanIsActive = true;
        else if (fanIsActive && currentTemp <= 85) fanIsActive = false;

        if (fanIsActive) {
          if (currentTemp >= 100) {
            targetPhysDuty = 90.0f;
          } else {
            targetPhysDuty = map(currentTemp, 88, 100, 30, 90);
          }
        } else {
          targetPhysDuty = 10.0f;
        }

        if (acActive) {
          float minAcDuty = IS_TEMIC_MODE ? 50.0f : 30.0f;
          if (targetPhysDuty < minAcDuty) targetPhysDuty = minAcDuty;
        }

        if (!overheatActive) {
          if (currentTemp >= 100) {
            if (relayTimer == 0) relayTimer = millis();
            else if (millis() - relayTimer >= RELAY_DELAY) {
              overheatActive = true;
              relayTimer = 0;
            }
          } else {
            relayTimer = 0;
          }
        } else {
          if (currentTemp <= 97) {
            if (relayTimer == 0) relayTimer = millis();
            else if (millis() - relayTimer >= RELAY_DELAY) {
              overheatActive = false;
              relayTimer = 0;
            }
          } else {
            relayTimer = 0;
          }
        }

        targetRelayState = !IS_TEMIC_MODE && (overheatActive || acActive);
        bypassFilter = false;
      }
    }

    if (bypassFilter) {
      stableTargetDuty = targetPhysDuty;
      dutyTimer = 0;
    } else {
      if (targetPhysDuty != stableTargetDuty) {
        if (dutyTimer == 0) {
          dutyTimer = millis();
        } else if (millis() - dutyTimer >= DUTY_DELAY) {
          stableTargetDuty = targetPhysDuty;
          dutyTimer = 0;
        }
      } else {
        dutyTimer = 0;
      }
    }

    if (physDuty < stableTargetDuty) {
      physDuty += 5.0f;
      if (physDuty > stableTargetDuty) physDuty = stableTargetDuty;
    } else if (physDuty > stableTargetDuty) {
      physDuty -= 5.0f;
      if (physDuty < stableTargetDuty) physDuty = stableTargetDuty;
    }
  }

  unsigned long currentMicros = micros();
  unsigned long elapsedMicros = currentMicros - pwmStartTime;

  if (elapsedMicros >= PERIOD_MICROS) {
    pwmStartTime = currentMicros;
    elapsedMicros = 0;
  }

  unsigned long highTimeMicros = (unsigned long)(PERIOD_MICROS * (1.0f - (physDuty / 100.0f)));

  if (elapsedMicros < highTimeMicros) {
    digitalWrite(fanPin, HIGH);
  } else {
    digitalWrite(fanPin, LOW);
  }

  digitalWrite(relayPin, targetRelayState ? HIGH : LOW);

  if (millis() - lastUpdateTime >= 500) {
    lastUpdateTime = millis();
#if DEBUG
    Serial.print(F("ADC NTC: "));
    Serial.print(rawAnalog);
    Serial.print(F(" | Btn ADC (A3): "));
    Serial.print(btnADC);

    if (rawAnalog >= 1018 || currentTemp == -99) {
      Serial.print(F(" | Error: Sensor Open"));
    } else if (rawAnalog <= 5) {
      Serial.print(F(" | Error: Sensor Short"));
    } else {
      Serial.print(F(" | Temp: "));
      Serial.print(currentTemp);
      Serial.print(F("C"));
    }

    Serial.print(F(" | Mode: "));
    if (mode == 0) Serial.print(F("Auto"));
    else if (mode == 1) Serial.print(F("Max"));
    else if (mode == 2) Serial.print(F("Turbo"));
    else if (mode == 99) Serial.print(F("Serial"));

    if (overheatActive) Serial.print(F(" [OVERHEAT!]"));
    Serial.print(F(" | AC: "));
    Serial.print(acActive ? F("ON") : F("OFF"));
    Serial.print(F(" | Relay: "));
    Serial.print(targetRelayState ? F("ON") : F("OFF"));
    Serial.print(F(" | PWM: "));
    Serial.print(physDuty, 1);
    Serial.println(F("%"));
#endif
  }
}

void processNonBlockingSerial() {
  static char buffer[8];
  static byte index = 0;

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (index > 0) {
        buffer[index] = '\0';
        int input = atoi(buffer);
        if (input >= 0 && input <= 100) {
          mode = 99;
          manualPhysDuty = (float)input;
        } else if (input == 999) {
          mode = 0;
        }
        index = 0;
      }
    } else if (isdigit(c) && index < sizeof(buffer) - 1) {
      buffer[index++] = c;
    }
  }
}

bool getFilteredACState() {
  static byte acHistory = 0;
  bool currentState = (analogRead(acPin) > 500);
  acHistory = (acHistory << 1) | (currentState ? 1 : 0);
  return (acHistory & 0x0F) == 0x0F;
}

int getTemperatureNTC(int rawAnalog) {
  static float filteredRaw = 512.0f;
  filteredRaw = filteredRaw * 0.80f + (float)rawAnalog * 0.20f;

  if (filteredRaw >= 1018.0f || filteredRaw <= 5.0f) return -99;

  float denominator = (1023.0f / filteredRaw) - 1.0f;
  if (denominator <= 0.0001f) return -99;

  float resistance = SERIES_RESISTOR / denominator;
  if (resistance <= 0.0f) return -99;

  float steinhart = resistance / THERMISTOR_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= B_COEFFICIENT;
  steinhart += 1.0f / (TEMPERATURE_NOMINAL + 273.15f);

  if (steinhart <= 0.0f) return -99;

  steinhart = 1.0f / steinhart;
  steinhart -= 273.15f;

  int result = (int)round(steinhart) + TEMP_OFFSET;
  if (result < -40 || result > 150) return -99;

  return result;
}
