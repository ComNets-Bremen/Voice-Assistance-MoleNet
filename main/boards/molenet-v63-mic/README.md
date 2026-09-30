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

---

## 2. Build & Flash

Open PowerShell and run:

```powershell
# 1. Activate ESP-IDF environment
. C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1

# 2. Build the firmware
python scripts/build.py molenet-v63-mic --language en-US --wake-word disabled

# 3. Flash to board (replace COMxx with your board's COM port)
idf.py -p COMxx flash monitor --no-reset
```

---

## 3. Wi-Fi & First-Time Activation

1. **Connect to Hotspot:** On your phone or laptop, connect to the Wi-Fi named `XiaoZhi-XXXX`.
2. **Set Wi-Fi:** Open browser at `http://192.168.4.1`, choose your 2.4 GHz Wi-Fi, and save.
3. **Activate:** Look at the serial monitor for:
   ```text
   ACTIVATION CODE: 123456
   ```
   Enter this 6-digit code into your XiaoZhi server dashboard.

---

## 4. How to Talk

1. Press the onboard **SW2 (BOOT)** button once.
2. Speak into the INMP441 microphone (e.g. *"Hello, what is the weather?"*).
3. XiaoZhi will recognize your speech and respond in the serial console/dashboard!

## 5. Local Offline Commands

When Wi-Fi or the XiaoZhi service is unavailable, the firmware switches to the
embedded TinyML YES/NO/UP detector. It uses the same INMP441 audio stream and
does not use MultiNet for local commands. Recognition results and responses are
printed in the ESP-IDF serial monitor:

| Spoken word | Offline response |
| --- | --- |
| YES | Prints the offline assistant greeting |
| NO | Prints the reconnect message and turns off D6 (GPIO38) |
| UP | Prints `Today is Friday.` |

The Friday response is fixed text. Reconnecting to Wi-Fi returns the device to
the normal XiaoZhi activation and cloud path.
