# Edge AI Gesture Classification System on RISC-V(ARIES v2.0)

## Overview

This project implements an **Edge AI gesture classification system** running on the **RISC-V VEGA (ARIES v2.0)** platform. The firmware processes spatial time-series signals from a dual HC-SR04 ultrasonic sensor setup to classify hand gestures in real time and trigger dynamic RGB LED feedback.

### Supported Gesture Classes

- **Push_Left:** Detects a localized forward approach in front of the left sensor (observed as a continuous decrease in distance).
- **Push_Right:** Detects a localized forward approach in front of the right sensor (observed as a continuous decrease in distance).
- **Swipe:** Detects a sequential lateral motion across both sensors (observed as time-lagged sharp distance pulses from left to right).

---

## Tech Stack & Key Concepts

- **Hardware:** VEGA ARIES v2.0 Board, Dual HC-SR04 Ultrasonic Sensors
- **Frameworks & Tools:** Edge Impulse Studio, VEGA SDK, RISC-V GNU Toolchain,Arduino IDE
- **Languages:** C, C++
- **Key Concepts:** Edge AI, INT8 Model Quantization, Bare-Metal Firmware, Real-Time Signal Processing, GPIO & Peripheral Drivers

---
##  Prerequisites & Dependencies

To compile, build, and flash this project, you must set up the **VEGA SDK** toolchain and workspace environment for the **ARIES v2.0** board.

### 1. Board & Processor Architecture
* **Hardware:** ARIES v2.0 Development Board powered by the indigenous **THEJAS32 / VEGA** RISC-V processor family developed by C-DAC.

### 2. Software & Toolchain Setup
* **VEGA SDK:** The project relies on board support package (BSP) drivers, header definitions, and peripheral libraries provided by C-DAC's VEGA SDK.
* **Installation Guide:** Follow the official [C-DAC VEGA SDK User Guide](https://cdac-vega.gitlab.io/sdkuserguide.html) for complete step-by-step setup instructions for your operating system (Linux).

### 3. Core Requirements
Ensure the following components from the SDK setup are configured in your system path:
- **Toolchain:** `riscv32-unknown-elf-gcc` (RISC-V GCC cross-compiler)
- **Flashing & Serial Utilities:** VEGA Flasher / XMODEM protocol for uploading binaries over Serial/UART
- **Build System:** `make` / CMake

---
##  Key Features

*  Executes an Edge Impulse-trained C++ model directly on bare-metal hardware without OS overhead.
*  Utilizes **INT8 quantization** to optimize memory footprint and decrease execution latency in an FPU-less embedded system.
*  Handles real-time sensor reading and hardware control using low-level C drivers via the VEGA SDK.

---

## Repository Content & Build Notes

> **Note on Firmware Architecture & Dependencies:**
> This repository contains the standalone application source code, the custom-built RGB LED control driver, and the Edge Impulse C++ model headers/metadata.
> 
> * **Custom Middleware & Firmware:** The higher-level RGB LED state controller (`rgb.c`/`rgb.h`) and application state machine were engineered specifically for this project.
> * **SDK Hardware Abstraction:** The custom LED driver builds directly on low-level GPIO (`GPIO_write_pin`) and UART/Serial communication routines provided by C-DAC's **VEGA SDK** Board Support Package (BSP).
> * **Header Availability:** Standard SDK drivers (`gpio.h`, `uart.h`, `config.h`) and hardware register maps are provided by the local VEGA SDK environment workspace and are omitted here to prevent committing vendor SDK files.
 
