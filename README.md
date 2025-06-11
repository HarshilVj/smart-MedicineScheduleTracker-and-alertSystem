# 💊smart-MedicineScheduleTracker-and-alertSystem
A real-time medicine reminder and tracking system developed for embedded platforms using a TM4C123 microcontroller. The system interfaces with a 4x4 keypad and I2C LCD, enabling users to input medicine schedules and receive timely alerts using LEDs/Buzzer.

## ✨ Features

- Track up to **3 medicines**(can be modified) concurrently.
- Input medicine **names and dosage intervals** (in minutes).
- Visual feedback via **I2C 16x2 LCD** having medicine name and real time countdown for medicine scheduled time.
- User input through a **4x4 matrix keypad**.
- Medicine countdown with periodic updates.
- **Alerts** when it's time to take a medicine.
- Real-time timer interrupt using **Timer0A**.

## 🛠️ Hardware Requirements

- TM4C123GH6PM (Tiva C Series LaunchPad)
- 16x2 I2C LCD display 
- 4x4 Matrix Keypad
- Jumper wires
- Breadboard / PCB (optional)
- Power Supply (USB or 5V regulated)
- LEDs/Buzzer

  ## 📐 Pin Connections

| Component | Tiva C Pin | Description         |
|---------- |------------|-------------------- |
| LCD SCL   | PB2        | I2C0 Clock Line     |
| LCD SDA   | PB3        | I2C0 Data Line      |
| LED/Buzzer   | PF4    | Positive/Signal pin  |
| Keypad R0–R3 | PD0–PD3 | Output (Rows)       |
| Keypad C0–C3 | PE0–PE3 | Input (Columns)     |


## 🧰 Software Requirements

- [Code Composer Studio (TI CCS)](https://www.ti.com/tool/CCSTUDIO)
- TivaWare™ Peripheral Driver Library

## 📋 Usage Instructions/Workflow
- Initial Screen:
Medicine Tracker
Press # to start
- Steps to Input Medicine:
Press # to start entry mode.

- Enter medicine name (max 10 chars) using keypad:

- Use C to delete a character.

- Press D to confirm name.

- Enter interval time (1–9 minutes) for that medicine.

- Repeat for up to 3 medicines. (cutomisable)

- After last entry, tracking starts automatically.

- While Tracking:
  LCD shows next upcoming medicine and countdown.

- Press # anytime to reset.

- When Alerting:
  LCD displays:

   TAKE MEDICINE:
    medicineName


    Buzzer/LED starts working.
- Automatically resets timer for next medicine with remaining time for the respective medicine.

## 🔄 System States
State	Descriptions


- STATE_WELCOME	Idle state waiting for user input.
- STATE_GET_MED_NAME	Captures medicine name input.
- STATE_GET_MED_TIME	Captures time interval.
- STATE_TRACKING	Timer countdown and monitoring mode.
- STATE_ALERT	Notifies user to take medicine.

  ## ⚙️ How It Works
- Timer0A triggers an interrupt every second.

- Countdown timers for active medicines are decremented.

- On timeout, the system enters alert mode for 2 seconds.

- LCD is updated every 5 seconds or on user interaction.

## 🤝 Contributing
- Pull requests are welcome! If you’d like to enhance UI, add EEPROM support, or support longer intervals—feel free to fork and improve.
- Let me know if you'd like a version with embedded images, GIFs of the LCD interface, or a printable circuit diagram.

## 🧠 Credits

- [Code Composer Studio (CCS)](https://www.ti.com/tool/CCSTUDIO) – for debugging and development on TI microcontroller codes.
- [Tiva C Series Community](https://e2e.ti.com/support/microcontrollers/arm-based-microcontrollers-group/arm-based-microcontrollers/f/arm-based-microcontrollers-forum) – for their support and open discussions on TM4C123GH6PM development.
