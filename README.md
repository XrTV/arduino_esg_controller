# Arduino ESG/Temic controller
# ESG/Temic Fan Controller v2.1 (by xrbullet)

Firmware for Arduino microcontrollers (ATmega328P / Nano / Uno) designed for autonomous control of high-power **Temic / ESG** engine cooling fans (found in Mercedes-Benz W163, W220, W211, etc.) via low-frequency PWM protocol (10 Hz) with inverted logic.

---

## 📌 Key Features & Capabilities

* **Hardware Reliability:** Integrated Watchdog Timer (WDT, 2 seconds) for automatic reboot in case of voltage drops or lockups. Completely stripped of freeze-prone I2C display code.
* **Inverted 10 Hz PWM:** Signal output generated within a physical duty cycle range of 10%–67% (strictly matching ESG module specs).
* **Smooth Ramping:** Speed increments and decrements are stepped (+/-5% with a 3-second delay) to prevent massive voltage spikes in the vehicle's electrical system.
* **Sensor Fault Protection:** Direct transition to 100% emergency cooling if ADC readings fall out of safe bounds (`rawAnalog >= 1015` or `<= 8`) or if catastrophic overheating occurs.
* **Auxiliary Power Relay Control:** Dedicated output for an external heavy-duty backup relay with time-based hysteresis (5 seconds) during overheat events.
* **Manual & Emergency Overrides:** Toggle operational modes (Auto / 100% Fan without Relay / 100% Fan with Relay) using a single physical button (short click / 3s long press) or via Serial commands.
* **Configurable Audio Alerts:** Buzzer support with distinct sound patterns for boot, sensor fault, and overheat warnings. Can be completely disabled via `#define USE_BUZZER 0`.

---

## ⚙️ Specifications & Pinouts

* **PWM Frequency:** 10 Hz (100,000 µs period)
* **Temperature Sensor:** NTC 120 kΩ (configurable via `SERIES_RESISTOR`, `THERMISTOR_NOMINAL`, `B_COEFFICIENT`)
* **PWM Output Pin (`fanPin`):** Pin 9
* **NTC Sensor Pin (`thermistorPin`):** Pin A0
* **Relay Output Pin (`relayPin`):** Pin 3 (Inverted logic: LOW = ON)
* **Button Pin (`buttonPin`):** Pin 4 (INPUT_PULLUP)
* **Buzzer Pin (`buzzerPin`):** Pin 5
