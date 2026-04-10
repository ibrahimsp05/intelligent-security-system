
# Intelligent Garage Security System (IGSS)

An embedded garage security system that combines RFID-based access control, thermal human
detection, and electromechanical actuation. The system integrates sensing, decision-making,
and actuation under a real-time control architecture implemented on the Arduino Uno R4.

---

## 🚀 Project Overview

The **Intelligent Garage Security System (IGSS)** enhances conventional garage controllers by
combining **identity verification** with **autonomous intrusion detection**. In addition to
RFID-based access control, the system continuously monitors human presence using thermal
infrared sensing and triggers a **latched alarm** when unauthorised access is detected.

Key features:
- RFID authentication for authorised users
- Thermal-based human presence detection
- Motorised garage door with encoder feedback
- Servo-actuated physical locking mechanism
- Audible and visual system feedback
- Safety features including motion timeouts and fault detection

---

## 🧠 System Architecture

The system is built around an **Arduino Uno R4 WiFi**, interfacing with sensors, actuators, and
user-interface components using multiple communication protocols.

**Subsystems**
- Authentication: 125 kHz RFID reader (EM4100)
- Intrusion detection: AMG8833 8×8 thermal infrared camera
- Actuation:
  - DC motor with quadrature encoder (door movement)
  - Motoron M3S550 motor controller (I2C)
  - Servo motor for physical locking
- User interface:
  - OLED display (256×64)
  - RGB status LED
  - Passive buzzer
  - Push-button override

---

## 🔌 Pinout and Connections

### Arduino Uno R4 – Pin Assignment

#### RFID Reader (SoftwareSerial)
| Function | Arduino Pin |
|--------|-------------|
| RX | 2 |
| TX | 7 |

---

#### RGB LED (via 100 Ohms resistors)
| Colour | Arduino Pin |
|------|-------------|
| Red | 4 |
| Green | 8 |
| Blue | 9 |

---

#### User Inputs and Feedback
| Device | Arduino Pin |
|------|-------------|
| Push Button (INPUT_PULLUP) | A1 |
| Passive Buzzer | A0 |
| Servo Motor (Lock) | 3 |

---

#### Encoder Inputs
| Encoder Channel | Arduino Pin |
|----------------|-------------|
| Channel A | A2 |
| Channel B | A3 |

---

#### OLED Display (SSD1362 – Software SPI)
| Signal | Arduino Pin |
|--------|-------------|
| SCK | 13 |
| MOSI | 11 |
| CS | 10 |
| DC | 12 |
| RST | 5 |

---

#### I²C Devices
| Device | I²C Bus | Address |
|-------|--------|---------|
| AMG8833 Thermal Camera | Wire1 (Qwiic) |
| Motoron M3S550 | Wire |

---

## ⚙️ Functional Behaviour

- System starts in **LOCKED** mode
- Valid RFID scan unlocks the system and opens the door
- Thermal frames are continuously analysed while locked
- Detection of human presence without authentication triggers a **latched alarm**
- Three consecutive invalid RFID attempts also trigger the alarm
- Alarm can only be cleared using an authorised RFID tag
- Door motion uses encoder-based displacement control with timeout safety

---

## 💻 Software

- Platform: Arduino (C++)
- Features:
  - State-based control logic
  - Thermal image processing
  - Encoder-based door control
  - Motor and sensor fault handling
  - OLED status display updates
  - Audible and visual alerts

The firmware is documented in the accompanying project report.

---

## 🔧 How to Use

1. Power the Arduino Uno R4 and the external motor supply.
2. System initialises in **LOCKED** state.
3. Present an authorised RFID tag to unlock and open the door.
4. Alarm triggers on unauthorised thermal detection or repeated invalid RFID scans.
5. Clear alarm using a valid RFID tag.

---

## ⚠️ Disclaimer

This project is an academic prototype developed for educational purposes.
It is not certified for real-world security deployment.

---

## 📄 License

This project is released under an **academic open-source license**.
Use, modify, and distribute for educational purposes.

---

## 👤 Author

**Ibrahim Syawwal Sofian**
**Timeo Ayme**
``