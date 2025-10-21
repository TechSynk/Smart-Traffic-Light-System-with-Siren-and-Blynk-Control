# Smart Traffic Light System with Siren and Blynk Control

📄 Project Summary

  This project implements an intelligent traffic light system that automatically detects emergency vehicles (like ambulances) using a microphone (I²S) to sense siren sound frequencies and IR sensors to locate the path of the emergency.

The ESP32 acts as the master controller, handling:

  * Siren detection using FFT analysis of microphone input
  * Path identification using IR sensors
  * Remote manual control via Blynk App
  * Communication with Arduino Mega (slave) through I²C

The Arduino Mega manages:

  * Real-time control of traffic light LEDs
  * Display of system status on a 16×2 I²C LCD
  * Switching to emergency mode or random light cycle

  Thus, the system ensures an emergency vehicle gets a green signal automatically on the correct road while others remain red, ensuring faster and safer passage.

⚙️ Hardware Components

| Component                        | Quantity | Description                                 |
| -------------------------------- | -------- | ------------------------------------------- |
| ESP32 Dev Board                  | 1        | Main controller for siren detection + Blynk |
| Arduino Mega 2560                | 1        | Traffic light controller                    |
| INMP441 / SPH0645 I²S Microphone | 1        | Detects siren sound                         |
| IR Sensor modules                | 3        | Detect presence of ambulance on each path   |
| 16x2 I²C LCD Display             | 1        | Shows traffic mode & emergency path         |
| Red, Yellow, Green LEDs          | 9        | Traffic lights (3 per path)                 |
| Jumper wires                     | —        | For interconnection                         |
| Breadboard / PCB                 | —        | For prototyping                             |
| USB cables                       | 2        | For ESP32 & Mega programming                |

🔌 Pin Connections

🟢 ESP32 Connections

| Function     | ESP32 Pin | Connected To              |
| ------------ | --------- | ------------------------- |
| I²C SDA      | GPIO 21   | Arduino Mega SDA (Pin 20) |
| I²C SCL      | GPIO 22   | Arduino Mega SCL (Pin 21) |
| IR Path 1    | GPIO 14   | IR Sensor 1 OUT           |
| IR Path 2    | GPIO 27   | IR Sensor 2 OUT           |
| IR Path 3    | GPIO 26   | IR Sensor 3 OUT           |
| I²S WS       | GPIO 25   | Mic WS / LRCL             |
| I²S SCK      | GPIO 33   | Mic SCK / BCLK            |
| I²S SD       | GPIO 32   | Mic DOUT                  |
| Built-in LED | GPIO 2    | Status indicator          |

Power:

Mic VCC → 3.3V
IR sensors VCC → 5V
All GNDs connected together

🔵 Arduino Mega Connections

| Function    | Mega Pin   | Description              |
| ----------- | ---------- | ------------------------ |
| SDA         | Pin 20     | I²C data from ESP32      |
| SCL         | Pin 21     | I²C clock from ESP32     |
| LCD SDA     | To I²C bus | Same as SDA (20)         |
| LCD SCL     | To I²C bus | Same as SCL (21)         |
| Red LEDs    | 6, 10, 13  | One for each path        |
| Yellow LEDs | 5, 8, 12   | One for each path        |
| Green LEDs  | 7, 9, 11   | One for each path        |
| GND         | —          | Common ground with ESP32 |

📱 Blynk Configuration

In Blynk Cloud, create a new template:


| Virtual Pin | Widget     | Function                    |
| ----------- | ---------- | --------------------------- |
| V4          | Button     | Manual Emergency Path 1     |
| V5          | Button     | Manual Emergency Path 2     |
| V6          | Button     | Manual Emergency Path 3     |
| V1          | LCD Widget | Displays system mode/status |

Set Buttons to:

Mode: Switch
Values: ON = 1, OFF = 0

🧩 Working Principle

1️⃣ Normal Mode

  * Arduino Mega runs a random traffic light cycle (each path gets a random color pattern every 3       seconds).
  * LCD shows "RANDOM".
  * ESP32 continuously listens via I²S microphone and reads IR sensors.

2️⃣ Automatic Emergency Detection

  * When a siren sound (600–1500 Hz) is detected with sufficient amplitude:
  * ESP32 activates the built-in LED and starts checking the IR sensors.
  * The IR sensor that detects a vehicle corresponds to the path number.
  * ESP32 sends an emergency signal (1, 2, or 3) to Arduino Mega over I²C.
  * The corresponding path LED turns GREEN, others turn RED.
  * LCD and Blynk display “AMBULANCE!” blinking.
  * Once the siren stops and the IR sensor is cleared, ESP32 automatically resets the system to         normal "RANDOM" mode.

3️⃣ Manual Emergency via Blynk

  * User can manually select a path (1–3) in the Blynk app to grant emergency access.
  * ESP32 sends this path to the Mega.
  * The same emergency light pattern activates immediately.
  * When user switches it off, system returns to normal mode.

4️⃣ Display & Feedback

  * LCD on Arduino Mega: Shows "RANDOM", "EMERGENCY PATH X", or "AMBULANCE!".
  * Blynk LCD: Displays synchronized status remotely.

📚 Required Libraries

For ESP32

  * BlynkSimpleEsp32.h
  * WiFi.h
  * Wire.h
  * arduinoFFT.h

For Mega

  * Wire.h
  * LiquidCrystal_I2C.h
