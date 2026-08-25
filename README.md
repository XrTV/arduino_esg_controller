# ESG/TEMIC Fan Controller Firmware v2.1

## 🇷🇺 Русский

Управляющая прошивка для контроллеров вентиляторов охлаждения **TEMIC** и **ESG** на базе микроконтроллеров AVR (Arduino / ATmega328P). Обеспечивает генерацию low-frequency PWM (10 Гц) с плавной рампой скважности, обработку сигналов NTC-термистора, климатической установки (AC) и управление дополнительным реле.

### 🌟 Основные возможности
* **Адаптивные режимы работы:** Поддержка силовых блоков **TEMIC** (минимальный Duty для AC — 50%) и **ESG** (минимальный Duty для AC — 30%).
* **Антизависание и отказоустойчивость:** 
  * Аппаратный **Watchdog (WDT на 2 сек)** с защитой от зависаний при перезагрузках.
  * **Неблокирующий парсер Serial:** Взаимодействие через UART без использования задерживающих функций `parseInt()`.
  * **Защита АЦП:** Безопасный опрос датчиков без риска застревания при помехах в бортовой сети.
* **Гибкое управление кнопкой:** Поддержка обработки одиночных, двухкратных нажатий и длинных удержаний (Auto / Max / Turbo режимы).
* **Защитные алгоритмы:** 
  * Определение обрыва и короткого замыкания NTC-датчика с уходом в аварийный режим (90% Duty).
  * Фильтрация помех по входу кондиционера (AC) и гистерезис по температуре (88°C вкл / 85°C выкл).
  * Плавное изменение скважности PWM без резких бросков тока.

### 🛠 Настройка пинов
| Название | Пин Arduino | Описание |
| :--- | :--- | :--- |
| `fanPin` | **D9** | Выход ШИМ 10 Гц (Low-Side / Инвертированный) |
| `thermistorPin` | **A0** | Вход NTC-термистора (10k, B=3950) |
| `acPin` | **A2** | Сигнал включения кондиционера (AC) |
| `buttonPin` | **A3** | Вход кнопки управления |
| `relayPin` | **D3** | Управление дополнительным реле |
| `buzzerPin` | **D5** | Зуммер индикации режимов |

### 💬 Serial Управление (115200 baud)
* `0`–`100` — Установка фиксированной скважности ШИМ (переход в режим `Serial`).
* `999` — Возврат в автоматический режим (`Auto`).

---

## 🇬🇧 English

Control firmware for **TEMIC** and **ESG** cooling fan modules based on AVR microcontrollers (Arduino / ATmega328P). Provides low-frequency PWM generation (10 Hz) with smooth duty cycle ramping, NTC thermistor reading, AC signal filtering, and auxiliary relay control.

### 🌟 Key Features
* **Adaptive Controller Modes:** Supports **TEMIC** units (minimum AC Duty: 50%) and **ESG** units (minimum AC Duty: 30%).
* **Anti-Freeze & Fault Tolerance:**
  * Hardware **Watchdog Timer (2s WDT)** with reboot protection.
  * **Non-blocking Serial Parser:** Fully async UART processing without blocking calls like `parseInt()`.
  * **ADC Fault Recovery:** Prevents microcontroller lockups during onboard 12V voltage spikes or ADC hardware delays.
* **Multi-function Button Engine:** Single-click, double-click, and long-press detection (Auto / Max / Turbo modes).
* **Failsafe & Protection:**
  * Automatic fail-open/short detection for the NTC sensor (defaults to 90% Emergency Duty).
  * Software debouncing and history filtering for the AC signal.
  * Hysteresis-based thermal control (88°C ON / 85°C OFF) and smooth PWM transitions.

### 🛠 Pin Mapping
| Function | Arduino Pin | Description |
| :--- | :--- | :--- |
| `fanPin` | **D9** | 10 Hz PWM Output (Low-Side / Inverted) |
| `thermistorPin` | **A0** | NTC Thermistor Input (10k, B=3950) |
| `acPin` | **A2** | Air Conditioner (AC) Signal Input |
| `buttonPin` | **A3** | Control Button Input |
| `relayPin` | **D3** | Auxiliary Relay Control Output |
| `buzzerPin` | **D5** | Mode Beeper / Buzzer Output |

### 💬 Serial Commands (115200 baud)
* `0`–`100` — Set manual PWM duty cycle (switches mode to `Serial`).
* `999` — Return to automatic thermal control (`Auto`).
