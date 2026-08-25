#include <Arduino.h>
#include <avr/wdt.h>

#define DEBUG      1   
const bool USE_BUZZER = false; 
const bool USE_RELAY  = false; 

// --- Fan controller mode ---
const bool IS_TEMIC_MODE = true; 

// --- Sensors & Peripherals ---
const int fanPin = 9;         // 10 Hz PWM output
const int thermistorPin = A0; // NTC sensor
const int acPin = A2;         // AC signal input
const int relayPin = 3;       // Auxiliary fan relay
const int buttonPin = A3;     // Кнопка на A3 (внешняя подтяжка 5V)
const int buzzerPin = 5;      // Buzzer pin

// --- NTC Thermistor parameters ---
const float SERIES_RESISTOR = 10000.0f;     
const float THERMISTOR_NOMINAL = 10000.0f;  
const float TEMPERATURE_NOMINAL = 25.0f;    
const float B_COEFFICIENT = 3950.0f;        

const int TEMP_OFFSET = -2; 

// --- PWM Period (10 Hz = 100,000 microseconds) ---
const unsigned long PERIOD_MICROS = 100000UL; 

// --- State variables ---
bool fanIsActive = false; 
bool overheatActive = false;      
unsigned long relayTimer = 0;     
const unsigned long RELAY_DELAY = 5000; 

float physDuty = 10.0f;              
float targetPhysDuty = 10.0f;    
float stableTargetDuty = 10.0f;  
unsigned long dutyTimer = 0;    
const unsigned long DUTY_DELAY = 3000; 

int mode = 0;                   // 0 - Auto, 1 - Max, 2 - Turbo, 99 - Serial
float manualPhysDuty = 10.0f;        

unsigned long lastUpdateTime = 0; 
unsigned long pwmStartTime = 0;

bool isSensorError = false;
bool acActive = false;

// --- Analog Divider Button Engine ---
bool stableBtnState = HIGH;
unsigned long btnPressTime = 0;
bool isHoldHandled = false;

int clickCount = 0;
unsigned long lastClickTime = 0;
const unsigned long MULTI_CLICK_TIMEOUT = 350; 

// --- Turbo Buzzer Timer ---
unsigned long lastTurboBeepTime = 0;

int getTemperatureNTC();
bool getFilteredACState();
void rebootController();
void processNonBlockingSerial();

void setup() {
  MCUSR = 0; // Сброс флагов причин перезагрузки для WDT
  wdt_enable(WDTO_2S); 

#if DEBUG
  Serial.begin(115200);
  Serial.println(F("\n=========================================="));
  Serial.println(F("       ESG/TEMIC CONTROLLER v2.1          "));
  Serial.println(F("            by xrbullet                   "));
  Serial.println(F("=========================================="));
  Serial.print(F("System Mode: "));
  Serial.println(IS_TEMIC_MODE ? F("TEMIC (Min AC Duty: 50%)") : F("ESG (Min AC Duty: 30%)"));
  Serial.println(F("Status: Watchdog (2s) Enabled. System Ready.\n"));
#endif

  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);

  pinMode(acPin, INPUT); 
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW); 
  
  pinMode(buttonPin, INPUT); 
  
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  pwmStartTime = micros();
}

void loop() {
  wdt_reset(); // Сброс собачьего таймера

  // --- Non-blocking Serial Processing ---
  processNonBlockingSerial();

  // --- Analog Button Read (A3 Divider Logic) ---
  int btnADC = analogRead(buttonPin);
  bool rawPinState = (btnADC < 100) ? LOW : HIGH;

  // Обработка переходов состояния кнопки
  if (rawPinState == LOW && stableBtnState == HIGH) {
    stableBtnState = LOW;
    btnPressTime = millis();
    isHoldHandled = false;
  } 
  else if (rawPinState == HIGH && stableBtnState == LOW) {
    stableBtnState = HIGH;
    unsigned long holdDuration = millis() - btnPressTime;

    if (btnPressTime > 0 && !isHoldHandled && holdDuration >= 40) { 
      if (mode != 0) {
        mode = 0; // Возврат в AUTO
        clickCount = 0;
      } else {
        clickCount++;
        lastClickTime = millis();
      }
    }
    btnPressTime = 0;
  }

  // Обработка длинных удержаний
  if (stableBtnState == LOW && btnPressTime > 0) {
    unsigned long currentHold = millis() - btnPressTime;

    if (currentHold >= 10000) {
      rebootController();
    }
    else if (currentHold >= 3000 && currentHold < 5000 && !isHoldHandled) {
      mode = 2; // Turbo
      isHoldHandled = true;
      clickCount = 0;
    }
  }

  // Тайм-аут мультиклика
  if (clickCount > 0 && (millis() - lastClickTime >= MULTI_CLICK_TIMEOUT)) {
    if (mode == 0) { 
      if (clickCount == 1) mode = 1;      // Max
      else if (clickCount >= 2) mode = 2; // Turbo
    }
    clickCount = 0;
  }

  // --- Turbo Beeper Logic ---
  if (mode == 2) {
    if (USE_BUZZER) {
      if (millis() - lastTurboBeepTime >= 1000) {
        lastTurboBeepTime = millis();
        tone(buzzerPin, 1000, 100); 
      }
    }
  } else {
    if (USE_BUZZER) {
      noTone(buzzerPin);
    }
  }

  // --- Temperature & Logic Cycle ---
  static unsigned long lastLogicCycle = 0;
  static int currentTemp = 0;
  static int rawAnalog = 0;
  static bool targetRelayState = false;

  if (millis() - lastLogicCycle >= 150) {
    lastLogicCycle = millis();

    acActive = getFilteredACState();
    rawAnalog = analogRead(thermistorPin);
    currentTemp = getTemperatureNTC(); 

    bool bypassFilter = false; 

    if (mode == 99) {
      targetPhysDuty = manualPhysDuty;
      targetRelayState = false;
      bypassFilter = true; 
      isSensorError = false;
    } 
    else if (mode == 1) { 
      targetPhysDuty = 90.0f;
      targetRelayState = false;
      bypassFilter = true;
      isSensorError = false;
    }
    else if (mode == 2) { 
      targetPhysDuty = 90.0f;
      targetRelayState = (USE_RELAY && !IS_TEMIC_MODE);
      bypassFilter = true;
      isSensorError = false;
    }
    else { // Mode 0: AUTO
      if (rawAnalog >= 1018 || rawAnalog <= 5 || currentTemp == -99) {
        targetPhysDuty = 90.0f; 
        targetRelayState = (USE_RELAY && !IS_TEMIC_MODE); 
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
          if (IS_TEMIC_MODE) {
            if (targetPhysDuty < 50.0f) targetPhysDuty = 50.0f; 
          } else {
            if (targetPhysDuty < 30.0f) targetPhysDuty = 30.0f;
          }
        }

        if (!overheatActive) {
          if (currentTemp >= 100) {
            if (relayTimer == 0) relayTimer = millis();
            else if (millis() - relayTimer >= RELAY_DELAY) { overheatActive = true; relayTimer = 0; }
          } else { relayTimer = 0; }
        } 
        else {
          if (currentTemp <= 97) {
            if (relayTimer == 0) relayTimer = millis();
            else if (millis() - relayTimer >= RELAY_DELAY) { overheatActive = false; relayTimer = 0; }
          } else { relayTimer = 0; }
        }

        if (USE_RELAY && !IS_TEMIC_MODE) {
          targetRelayState = overheatActive || acActive;
        } else {
          targetRelayState = false; 
        }

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

  // --- PWM Generation (10 Hz, Low-side inverted) ---
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

  if (USE_RELAY && targetRelayState) {
    digitalWrite(relayPin, HIGH);   
  } else {
    digitalWrite(relayPin, LOW);                
  }

  // --- Serial Output ---
  if (millis() - lastUpdateTime >= 500) { 
    lastUpdateTime = millis();

#if DEBUG
    Serial.print(F("ADC NTC: "));   Serial.print(rawAnalog);
    Serial.print(F(" | Btn ADC (A3): ")); Serial.print(btnADC);
    if (rawAnalog >= 1018 || currentTemp == -99) { Serial.print(F(" | Error: Sensor Open")); }
    else if (rawAnalog <= 5) { Serial.print(F(" | Error: Sensor Short")); }
    else { Serial.print(F(" | Temp: "));  Serial.print(currentTemp); Serial.print(F("C")); }
    Serial.print(F(" | Mode: ")); 
    if (mode == 0) Serial.print(F("Auto"));
    else if (mode == 1) Serial.print(F("Max"));
    else if (mode == 2) Serial.print(F("Turbo"));
    else if (mode == 99) Serial.print(F("Serial"));
    if (overheatActive) Serial.print(F(" [OVERHEAT!]"));
    Serial.print(F(" | AC: ")); Serial.print(acActive ? F("ON") : F("OFF"));
    Serial.print(F(" | PWM: "));    Serial.print(physDuty, 1); Serial.println(F("%"));
#endif
  }
}

// --- Неблокирующее считывание команд из Serial ---
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

int getTemperatureNTC() {
  static float filteredRaw = 512.0f; 
  int currentRaw = analogRead(thermistorPin);
  
  filteredRaw = filteredRaw * 0.80f + (float)currentRaw * 0.20f;

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

void rebootController() {
  cli(); // Отключаем прерывания перед уходом в ребут
  wdt_enable(WDTO_15MS); 
  while (true) {} // Сброс по Watchdog
}
