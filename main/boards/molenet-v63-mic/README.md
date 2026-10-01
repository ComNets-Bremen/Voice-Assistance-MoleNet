# MoleNet V6.3 (INMP441 Microphone Setup)

This guide shows how to connect an **INMP441 I2S microphone** to the **MoleNet V6.3** board and run XiaoZhi AI.

---

## 1. Hardware Wiring

Power comes from **J3** and audio signals from **J9**:

| INMP441 Pin | MoleNet V6.3 Pin | Function |
| :--- | :--- | :--- |
| **VDD** | **J3 Pin 2** | 3.3V Power (**WARNING: Never use J3 Pin 1 which is +12V!**) |
| **GND** | **J3 Pin 4** (or **J9 Pin 4**) | Ground |
| **L/R** | **J9 Pin 6** (or **J9 Pin 8**) | Ground (Selects Left Channel) |
| **SCK** | **J9 Pin 7** | I2S Clock (GPIO 40) |
| **WS** | **J9 Pin 5** | I2S Word Select (GPIO 41) |
| **SD** | **J9 Pin 3** | I2S Data (GPIO 42) |

> **Push-to-Talk Button:** Onboard **SW2 (BOOT button)** on **GPIO 0**.




## 2. How to Talk

1. Press the onboard **SW2 (BOOT)** button once.
2. Speak into the INMP441 microphone (e.g. *"Hello, what is the weather?"*).
3. The chatbot will recognize your speech and respond in the serial console/dashboard!

## 5. Local Offline Commands

When Wi-Fi or the XiaoZhi service is unavailable, the firmware switches to the
embedded TinyML YES/NO/UP detector. It uses the same INMP441 audio stream and
does not use MultiNet for local commands. Recognition results and responses are
printed in the ESP-IDF serial monitor:

| Spoken word | Offline response |
| --- | --- |
| YES | 
| NO | 
| UP | 


