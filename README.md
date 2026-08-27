# ESG/TEMIC Fan Controller Firmware v2.4

## 🇷🇺 Русский

Управляющая прошивка и схема подключения для контроллеров вентиляторов охлаждения **TEMIC** и **ESG** на базе **Arduino Nano** (ATmega328P). Обеспечивает генерацию low-frequency PWM (10 Гц) с плавной рампой скважности, обработку сигналов NTC-термистора, климатической установки (AC), звуковую индикацию и управление реле (для ESG).

### 🌟 Основные возможности
* **Единый флаг конфигурации (`IS_TEMIC_MODE`):**
  * `true` (TEMIC): минимальный Duty для AC — 50%, силовое реле **отключено**.
  * `false` (ESG): минимальный Duty для AC — 30%, силовое реле **активно** (поддержка перегрева и AC).
* **Антизависание и отказоустойчивость:** 
  * Аппаратный **Watchdog (WDT 2 сек)** с защитой от зависаний при перезагрузках.
  * **Неблокирующий парсер Serial:** Взаимодействие через UART (115200 baud) без задержек.
  * **Безопасная работа с АЦП:** Фильтрация и защита от помех бортовой сети 12V.
* **Звуковая индикация (Buzzer):** Информирование о старте системы, переключении режимов (Auto / Max / Turbo) и предупреждающие сигналы в режиме Turbo.
* **Многофункциональная кнопка:** Обработка одиночных, двухкратных нажатий и длинных удержаний.
* **Защитные алгоритмы:** 
  * Определение обрыва и короткого замыкания NTC-датчика с уходом в аварийный режим (90% Duty).
  * Фильтрация сигналов кондиционера (AC) и гистерезис по температуре (88°C вкл / 85°C выкл).

---

### 🔌 Схема подключения компонентов

#### 1. Питание (Mini360 & Сглаживающий конденсатор)
* **Бортовая сеть (Клемма 15 / +12V):** `In+` Mini360
* **Масса (GND):** `In-` Mini360
* **Конденсатор 16V 100мФ:** 
  * `+` -> `In+` Mini360
  * `-` -> `In-` Mini360
* **Arduino Nano:**
  * `Out+` Mini360 (настроен на **5V**) -> Пин `5V` Arduino
  * `Out-` Mini360 -> Пин `GND` Arduino

#### 2. NTC Датчик температуры (10 кОм)
* **Резистор 10 кОм (подтяжка):** Между `5V` и `A0`
* **NTC Термистор:**
  * Нога 1 -> `A0`
  * Нога 2 -> `GND`

#### 3. Силовой транзистор ШИМ (BUK101)
* **Нога 1 (Gate / Затвор):** Пин `D9` Arduino
* **Нога 2 (Drain / Сток):** Выход ШИМ на управляющий провод вентилятора
* **Нога 3 (Source / Исток):** `GND`
* **Резистор 10 кОм (стяжка):** Между `D9` и `GND`

#### 4. Вход опроса кондиционера (Делитель напряжения)
* **Резистор 4.7 кОм:** Между `A2` и `GND`
* **Резистор 10 кОм:** Между `A2` и сигнальным проводом +12V (муфта/вентилятор кондиционера)

#### 5. Кнопка управления
* **Резистор 10 кОм:** Между `A3` и `5V`
* **Кнопка:**
  * Контакт 1 -> `A3`
  * Контакт 2 -> `GND`

#### 6. Зуммер (Buzzer 5V)
* `+` -> Пин `D5` Arduino
* `-` -> `GND`

#### 7. Реле управления (Активно в режиме ESG)
* `VCC` -> `5V` Arduino
* `GND` -> `GND`
* `IN` -> Пин `D3` Arduino
* `COM` -> `GND`
* `NO / OUT` -> Сигнальный выход на реле включения вентилятора кондиционера

---

### 🛠 Настройка пинов (Pinout Summary)

| Модуль | Пин Arduino | Подключение | Примечание |
| :--- | :--- | :--- | :--- |
| **BUK101** | **D9** | Gate (Затвор) | Выход ШИМ 10 Гц (Low-Side). Стяжка 10кОм на GND |
| **Buzzer** | **D5** | Positive (+) | Активный зуммер 5V |
| **Relay** | **D3** | Signal (IN) | Управление реле (только в режиме ESG) |
| **NTC Sensor**| **A0** | Сигнал датчика | Подтяжка 10кОм на 5V |
| **AC Input** | **A2** | Делитель напряжения | 10k (+12V AC) / 4.7k (GND) |
| **Button** | **A3** | Контакт 1 | Подтяжка 10кОм на 5V, контакт 2 на GND |

---

### 💬 Serial Управление (115200 baud)
* `0`–`100` — Установка фиксированной скважности ШИМ (переход в режим `Serial`).
* `999` — Возврат в автоматический режим (`Auto`).

---

## 🇬🇧 English

Control firmware and wiring diagram for **TEMIC** and **ESG** cooling fan modules based on **Arduino Nano** (ATmega328P). Provides low-frequency PWM generation (10 Hz) with smooth duty cycle ramping, NTC thermistor reading, AC signal filtering, buzzer audio feedback, and optional relay control (ESG mode).

### 🌟 Key Features
* **Unified Hardware Mode Flag (`IS_TEMIC_MODE`):**
  * `true` (TEMIC): minimum AC Duty — 50%, auxiliary relay **disabled**.
  * `false` (ESG): minimum AC Duty — 30%, auxiliary relay **enabled** (overheat & AC support).
* **Anti-Freeze & Fault Tolerance:**
  * Hardware **Watchdog Timer (2s WDT)** with reboot protection.
  * **Non-blocking Serial Parser:** Fully async UART processing at 115200 baud.
  * **ADC Protection:** Software filtering against 12V onboard noise.
* **Audio Feedback (Buzzer):** Sound indications for system boot, mode switching (Auto / Max / Turbo), and Turbo warnings.
* **Multi-function Button:** Single-click, double-click, and long-press handling.

---

### 🔌 Pinout & Wiring Specification

* **Mini360 Step-Down:** `In+` to Terminal 15 (+12V), `In-` to GND. `16V 100uF Capacitor` parallel across inputs. Output set to 5V -> Arduino `5V` & `GND`.
* **NTC 10k Sensor:** Connected between `A0` and `GND`. Pull-up `10k Ohm` resistor from `A0` to `5V`.
* **BUK101 MOSFET:** Gate -> Pin `D9`, Drain -> Fan PWM Output, Source -> `GND`. Pull-down `10k Ohm` resistor from `D9` to `GND`.
* **AC Sensing Divider:** `A2` to `GND` via `4.7k Ohm` resistor. `A2` to +12V AC Clutch via `10k Ohm` resistor.
* **Control Button:** Connected between `A3` and `GND`. Pull-up `10k Ohm` resistor from `A3` to `5V`.
* **Buzzer 5V:** Positive (+) to Pin `D5`, Negative (-) to `GND`.
* **Relay Module:** `VCC` -> 5V, `GND` -> GND, `IN` -> Pin `D3`. `COM` -> GND, `NO` -> AC Fan Relay output.
