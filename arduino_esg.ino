#include <Arduino.h>
#include <avr/wdt.h> // Аппаратный ватчдог от зависаний

#define DEBUG 1   

const int fanPin = 9;         
const int thermistorPin = A0;  
const int relayPin = 3;       
const int buttonPin = 4;      
const int buzzerPin = 5;      

const float SERIES_RESISTOR = 100000.0;     
const float THERMISTOR_NOMINAL = 120500.0;  
const float TEMPERATURE_NOMINAL = 20.0;     
const float B_COEFFICIENT = 3950.0;         

const unsigned long PERIOD_MICROS = 100000; 

bool fanIsActive = false; 
bool overheatActive = false;      
unsigned long relayTimer = 0;     
const unsigned long RELAY_DELAY = 5000; 

int currentDuty = 0;       
int targetDuty = 0;        
int stableTargetDuty = 0;  
unsigned long dutyTimer = 0;     
const unsigned long DUTY_DELAY = 3000; 

int mode = 0;             
int manualDuty = 0;       

unsigned long lastUpdateTime = 0; 
unsigned long pwmStartTime = 0;

unsigned long buzzerBlockTimer = 0;
unsigned long buzzerStepTimer = 0;
int buzzerStep = 0;
bool isSensorError = false;
bool forceOpenError = false;

bool lastBtnState = HIGH;
unsigned long btnPressTime = 0;
bool isLongPressHandled = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

int getTemperatureNTC();
void handleBuzzer();

void setup() {
  wdt_enable(WDTO_2S); // Включаем ватчдог на 2 сек (автоперезагрузка при зависании)

#if DEBUG
  Serial.begin(115200);
  Serial.println(F("--- ZAPUSKATR ESG (NO LCD) ---"));
#endif

  pinMode(fanPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  
  unsigned long highTimeMicros = (unsigned long)(PERIOD_MICROS * (1.0 - (10.0 / 100.0)));
  pwmStartTime = micros();
  while (micros() - pwmStartTime < highTimeMicros) { 
    digitalWrite(fanPin, HIGH); 
  }
  
  digitalWrite(relayPin, HIGH); 
  pinMode(relayPin, OUTPUT);
  
  pinMode(buttonPin, INPUT_PULLUP);

  // Звуковой сигнал старта
  digitalWrite(buzzerPin, HIGH); delay(60); 
  digitalWrite(buzzerPin, LOW);  delay(40); 
  digitalWrite(buzzerPin, HIGH); delay(160); 
  digitalWrite(buzzerPin, LOW);
}

void loop() {
  wdt_reset(); // Сбрасываем таймер ватчдога в каждом цикле

  // Чтение команд из Serial
  if (Serial.available() > 0) {
    int input = Serial.parseInt(); 
    if (input >= 0 && input <= 100) {
      mode = 99; 
      manualDuty = input;
    } else if (input == 999) {
      mode = 0; 
    }
    while(Serial.available() > 0) { Serial.read(); }
  }

  // Обработка кнопки
  bool reading = digitalRead(buttonPin);
  if (reading != lastBtnState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      if (btnPressTime == 0) {
        btnPressTime = millis();
        isLongPressHandled = false;
      }
      if (!isLongPressHandled && (millis() - btnPressTime >= 3000)) {
        mode = 2; 
        isLongPressHandled = true;
      }
    } else {
      if (btnPressTime > 0) {
        if (!isLongPressHandled) {
          if (mode == 99) mode = 0;
          else {
            mode++;
            if (mode > 2) mode = 0; 
          }
        }
        btnPressTime = 0;
      }
    }
  }
  lastBtnState = reading;

  // Опрос датчика температуры
  bool targetRelayState = false; 
  int rawAnalog = analogRead(thermistorPin);
  int currentTemp = getTemperatureNTC(); 

  if (currentTemp <= -40 && rawAnalog < 1015 && rawAnalog > 8) {
    forceOpenError = true;
  } else {
    forceOpenError = false;
  }

  bool bypassFilter = false; 

  // Выбор режима
  if (mode == 99) {
    targetDuty = manualDuty;
    targetRelayState = false;
    bypassFilter = true; 
    isSensorError = false;
  }
  else if (mode == 1) {
    targetDuty = 100; 
    targetRelayState = false;
    bypassFilter = true; 
    isSensorError = false;
  } 
  else if (mode == 2) {
    targetDuty = 100;
    targetRelayState = true;
    bypassFilter = true; 
    isSensorError = false;
  } 
  else {
    // Авария датчика
    if (rawAnalog >= 1015 || rawAnalog <= 8 || forceOpenError) {
      targetDuty = 100;    
      targetRelayState = true; 
      bypassFilter = true; 
      isSensorError = true;
    } else {
      isSensorError = false;
      if (!fanIsActive && currentTemp >= 87) fanIsActive = true;
      else if (fanIsActive && currentTemp <= 84) fanIsActive = false;

      if (fanIsActive) {
        if (currentTemp >= 100) targetDuty = 100; 
        else targetDuty = map(currentTemp, 87, 100, 30, 100); 
      } else {
        targetDuty = 0;
      }

      // Задержка защиты по перегреву (силовое реле)
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
      targetRelayState = overheatActive;
      bypassFilter = false; 
    }
  }

  // Фильтрация плавности изменения оборотов
  if (bypassFilter) {
    stableTargetDuty = targetDuty; 
    dutyTimer = 0;
  } else {
    if (targetDuty != stableTargetDuty) {
      if (dutyTimer == 0) {
        dutyTimer = millis(); 
      } else if (millis() - dutyTimer >= DUTY_DELAY) {
        stableTargetDuty = targetDuty; 
        dutyTimer = 0;
      }
    } else {
      dutyTimer = 0; 
    }
  }

  // Шаг изменения скважности (5%)
  if (currentDuty < stableTargetDuty) {
    currentDuty += 5;
    if (currentDuty > stableTargetDuty) currentDuty = stableTargetDuty;
  } else if (currentDuty > stableTargetDuty) {
    currentDuty -= 5;
    if (currentDuty < stableTargetDuty) currentDuty = stableTargetDuty;
  }

  // Расчет инверсного программного PWM (10 Гц)
  float physicalDuty = 10.0 + ((float)currentDuty * 0.57);
  unsigned long highTimeMicros = (unsigned long)(PERIOD_MICROS * (1.0 - (physicalDuty / 100.0)));
  unsigned long currentMicros = micros();

  if (currentMicros - pwmStartTime >= PERIOD_MICROS) {
    pwmStartTime = currentMicros;
  }

  if (currentMicros - pwmStartTime < highTimeMicros) {
    digitalWrite(fanPin, HIGH); 
  } else {
    digitalWrite(fanPin, LOW);  
  }

  // Управление силовым реле
  if (targetRelayState) digitalWrite(relayPin, LOW);  
  else digitalWrite(relayPin, HIGH);                 

  handleBuzzer();

  // Логгирование в Serial каждые 500 мс
#if DEBUG
  if (millis() - lastUpdateTime >= 500) { 
    lastUpdateTime = millis();

    Serial.print(F("ADC: ")); Serial.print(rawAnalog);
    if (rawAnalog >= 1015 || forceOpenError) { Serial.print(F(" | Temp: ОБРЫВ")); }
    else if (rawAnalog <= 8) { Serial.print(F(" | Temp: КЗ ДАТЧИКА")); }
    else { Serial.print(F(" | Temp: ")); Serial.print(currentTemp); Serial.print(F("C")); }
    Serial.print(F(" | Overheat: ")); Serial.print(overheatActive ? F("ДА") : F("НЕТ"));
    Serial.print(F(" | Mode: ")); Serial.print(mode);
    Serial.print(F(" | Relay: ")); Serial.print(targetRelayState ? F("ВКЛ") : F("ВЫКЛ"));
    Serial.print(F(" | LogDUTY: ")); Serial.print(currentDuty); Serial.print(F("%"));
    Serial.print(F(" | PhysDUTY: ")); Serial.print((int)physicalDuty); Serial.println(F("%"));
  }
#endif
}

void handleBuzzer() {
  if (mode != 0) {
    digitalWrite(buzzerPin, LOW);
    buzzerStep = 0;
    return;
  }

  if (isSensorError) {
    if (millis() - buzzerStepTimer >= 500) {
      buzzerStepTimer = millis();
      digitalWrite(buzzerPin, !digitalRead(buzzerPin)); 
    }
    return;
  }

  if (overheatActive) {
    if (buzzerStep == 0) {
      if (millis() - buzzerBlockTimer >= 3000) {
        buzzerStep = 1;
        buzzerStepTimer = millis();
        digitalWrite(buzzerPin, HIGH); 
      }
    } else {
      if (millis() - buzzerStepTimer >= 150) { 
        buzzerStepTimer = millis();
        buzzerStep++;
        
        if (buzzerStep == 2)  digitalWrite(buzzerPin, LOW);  
        if (buzzerStep == 3)  digitalWrite(buzzerPin, HIGH); 
        if (buzzerStep == 4)  digitalWrite(buzzerPin, LOW);  
        if (buzzerStep == 5)  digitalWrite(buzzerPin, HIGH); 
        if (buzzerStep == 6) {
          digitalWrite(buzzerPin, LOW);  
          buzzerStep = 0;
          buzzerBlockTimer = millis();   
        }
      }
    }
  } else {
    digitalWrite(buzzerPin, LOW);
    buzzerStep = 0;
  }
}

int getTemperatureNTC() {
  int raw = analogRead(thermistorPin);
  if (raw == 0) return -99; 
  
  float resistance = SERIES_RESISTOR / ((1023.0 / raw) - 1.0);
  
  float steinhart;
  steinhart = resistance / THERMISTOR_NOMINAL;     
  steinhart = log(steinhart);                       
  steinhart /= B_COEFFICIENT;                       
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15); 
  steinhart = 1.0 / steinhart;                      
  steinhart -= 273.15;                              
  
  return (int)round(steinhart); 
}
