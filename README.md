# Обновленная документация и схема: ESG/TEMIC Fan Controller v2.5

Управляющая прошивка и схема подключения для контроллеров вентиляторов охлаждения **TEMIC** и **ESG** на базе **Arduino Nano** (ATmega328P). Обеспечивает генерацию low-frequency PWM (10 Гц) с плавной рампой скважности, обработку сигналов NTC-термистора, климатической установки (AC), звуковую индикацию и управление реле (для ESG).

---

## 🇷🇺 Русский

### 🌟 Основные возможности
* **Единый флаг конфигурации (`IS_TEMIC_MODE`):**
  * `true` (TEMIC): минимальный Duty для AC — 50%, силовое реле **отключено**.
  * `false` (ESG): минимальный Duty для AC — 30%, силовое реле **активно** (поддержка перегрева и AC).
* **Отказоустойчивость и интерфейс:** 
  * **Неблокирующий парсер Serial:** Взаимодействие через UART (115200 baud) без задержек.
  * **Безопасная работа с АЦП:** Фильтрация и защита от помех бортовой сети 12V.
* **Звуковая индикация (Buzzer):** Информирование о старте системы, переключении режимов (Auto / Max / Turbo) и предупреждающие сигналы в режиме Turbo.
* **Многофункциональная кнопка:** Обработка одиночных, двухкратных нажатий и длинных удержаний.
* **Защитные алгоритмы:** 
  * Определение обрыва и короткого замыкания NTC-датчика с уходом в аварийный режим (90% Duty).
  * Фильтрация сигналов кондиционера (AC) и гистерезис по температуре (88°C вкл / 85°C выкл).

---

### 🔌 Схема подключения компонентов и питания

#### 1. Питание (Mini360, фильтрация и защита)
* **Стабилитрон (12V):** Установлен на входе питания Mini360 для защиты от всплесков напряжения бортовой сети.
* **Бортовая сеть (Клемма 15 / +12V):** `In+` Mini360 через защитный стабилитрон.
* **Масса (GND):** `In-` Mini360.
* **Входные конденсаторы (параллельно на `In+` и `In-`):**
  * Электролитический конденсатор 16V **220 мкФ**.
  * Неполярный керамический конденсатор **104** (0.1 мкФ) для подавления ВЧ-помех.
* **Выходные конденсаторы (параллельно на `Out+` и `Out-`):**
  * Электролитический конденсатор 10V **470 мкФ**.
* **Защитный диод:** Установлен в разрыв плюсового выхода (`Out+`) Mini360 для защиты от обратного тока.
* **Arduino Nano:**
  * `Out+` (через диод) -> Пин `5V` Arduino.
  * `Out-` -> Пин `GND` Arduino.

#### 2. Аппаратный сброс (Reset)
* **Резистор 10 кОм (подтяжка):** Между `5V` и пином `RESET` Arduino.
* **Кнопка сброса:**
  * Контакт 1 -> Пин `RESET` Arduino.
  * Контакт 2 -> `GND`.

#### 3. NTC Датчик температуры (10 кОм)
* **Резистор 10 кОм (подтяжка):** Между `5V` и `A0`.
* **NTC Термистор:**
  * Нога 1 -> `A0`
  * Нога 2 -> `GND`

#### 4. Силовой транзистор ШИМ (BUK101)
* **Нога 1 (Gate / Затвор):** Пин `D9` Arduino.
* **Нога 2 (Drain / Сток):** Выход ШИМ на управляющий провод вентилятора.
* **Нога 3 (Source / Исток):** `GND`.
* **Резистор 10 кОм (стяжка):** Между `D9` и `GND`.

#### 5. Вход опроса кондиционера (Делитель напряжения)
* **Резистор 4.7 кОм:** Между `A2` и `GND`.
* **Резистор 10 кОм:** Между `A2` и сигнальным проводом +12V (муфта/вентилятор кондиционера).

#### 6. Кнопка управления режимами
* **Резистор 10 кОм:** Между `A3` и `5V`.
* **Кнопка:**
  * Контакт 1 -> `A3`
  * Контакт 2 -> `GND`

#### 7. Зуммер (Buzzer 5V)
* `+` -> Пин `D5` Arduino
* `-` -> `GND`

#### 8. Реле управления (Активно в режиме ESG)
* `VCC` -> `5V` Arduino
* `GND` -> `GND`
* `IN` -> Пин `D3` Arduino
* `COM` -> `GND`
* `NO / OUT` -> Сигнальный выход на реле включения вентилятора кондиционера

---

### 🛠 Настройка пинов и компонентов (Pinout Summary)

| Модуль / Цепь | Пин Arduino / Цепь | Подключение / Номинал | Примечание |
| :--- | :--- | :--- | :--- |
| **Защита входа 360** | `In+` | Стабилитрон на 12V | Защита от бросков питания |
| **Входной фильтр 360** | `In+` / `In-` | Электролит 16V **220 мкФ** + Керамика 104 | Параллельно входу |
| **Выходной фильтр 360** | `Out+` / `Out-` | Электролит 10V 470 мкФ | Параллельно выходу |
| **Защита выхода 360** | `Out+` | Диод | Защита от обратного тока |
| **Кнопка Reset** | Пин `RESET` | Через кнопку на GND | Аппаратный сброс |
| **Подтяжка Reset** | `5V` — `RESET` | Резистор 10 кОм | Подтяжка питания |
| **BUK101** | **D9** | Gate (Затвор) | ШИМ 10 Гц (Low-Side). Стяжка 10кОм на GND |
| **Buzzer** | **D5** | Positive (+) | Активный зуммер 5V |
| **Relay** | **D3** | Signal (IN) | Управление реле (только в режиме ESG) |
| **NTC Sensor** | **A0** | Сигнал датчика | Подтяжка 10кОм на 5V |
| **AC Input** | **A2** | Делитель напряжения | 10k (+12V AC) / 4.7k (GND) |
| **Button Mode** | **A3** | Контакт 1 | Подтяжка 10кОм на 5V, контакт 2 на GND |

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
* **Fault Tolerance & Interface:**
  * **Non-blocking Serial Parser:** Fully async UART processing at 115200 baud.
  * **ADC Protection:** Software filtering against 12V onboard noise.
* **Audio Feedback (Buzzer):** Sound indications for system boot, mode switching (Auto / Max / Turbo), and Turbo warnings.
* **Multi-function Button:** Single-click, double-click, and long-press handling.

### 🔌 Pinout & Wiring Specification
* **Mini360 Step-Down & Filtering:** 
  * Input protected via `12V Zener diode`. 
  * Input parallel capacitors: `16V 220uF` electrolytic + `104` (0.1uF) ceramic.
  * Output parallel capacitor: `10V 470uF` electrolytic.
  * Output positive line protected via `Diode`. Output set to 5V -> Arduino `5V` & `GND`.
* **Reset Circuit:** Push button between `RESET` and `GND`, pulled up to `5V` via `10k Ohm` resistor.
* **NTC 10k Sensor:** Connected between `A0` and `GND`. Pull-up `10k Ohm` resistor from `A0` to `5V`.
* **BUK101 MOSFET:** Gate -> Pin `D9`, Drain -> Fan PWM Output, Source -> `GND`. Pull-down `10k Ohm` resistor from `D9` to `GND`.
* **AC Sensing Divider:** `A2` to `GND` via `4.7k Ohm` resistor. `A2` to +12V AC Clutch via `10k Ohm` resistor.
* **Control Button:** Connected between `A3` and `GND`. Pull-up `10k Ohm` resistor from `A3` to `5V`.
* **Buzzer 5V:** Positive (+) to Pin `D5`, Negative (-) to `GND`.
* **Relay Module:** `VCC` -> 5V, `GND` -> GND, `IN` -> Pin `D3`. `COM` -> GND, `NO` -> AC Fan Relay output.
