# MoleNet Voice Assistant

Customized XiaoZhi voice-assistant firmware for the MoleNet v6.3 board and its INMP441 microphone. MoleNet v7.1 uses the same board layout for this build.

## Features

- Online voice-assistant mode through the configured XiaoZhi service.
- Offline TinyML keyword detection for **YES**, **NO**, and **UP** when the network is unavailable. The responses are printed in the serial monitor; **NO** also turns off the GPIO38 LED, and **UP** prints “Today is Friday.” Offline model loading is still being validated against the board’s available RAM.


This project is a customized and extended version of the open-source [XiaoZhi ESP32](https://github.com/78/xiaozhi-esp32) project.
**Original project:** [https://github.com/78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)\
**Original author/organization:** 78\
**License:** MIT
