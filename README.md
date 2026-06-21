<div align="center">

# 🔕 Gizmo Reolink Switch

### An ESP32-powered quiet-exit switch for Reolink NVR systems

Gizmo Reolink Switch is a small ESP32 project that lets you temporarily mute Reolink NVR notifications and recording so you can leave home without triggering your own cameras. It combines a one-button interface, buzzer feedback, a 16x2 I2C LCD, WiFi connectivity, and direct Reolink API control.

<p>
  <img alt="ESP32" src="https://img.shields.io/badge/esp32-ESP32-E7352C?logo=espressif&logoColor=white">
  <img alt="PlatformIO" src="https://img.shields.io/badge/platformio-PlatformIO-FF6B35?logo=platformio&logoColor=white">
  <img alt="Arduino" src="https://img.shields.io/badge/framework-Arduino-00979D?logo=arduino&logoColor=white">
  <img alt="Reolink" src="https://img.shields.io/badge/integration-Reolink-2563EB">
  <img alt="LCD 16x2" src="https://img.shields.io/badge/display-16x2%20I2C%20LCD-0F766E">
  <img alt="License MIT" src="https://img.shields.io/badge/license-MIT-2563EB">
</p>

<p>
  🏠 Quiet exit workflow • 📺 Friendly 16x2 LCD UI • 🔔 Buzzer feedback • 📡 WiFi-enabled • 🎥 Reolink NVR muting and restore
</p>

<p>
	<a href="#overview"><strong>Overview</strong></a> •
	<a href="#features"><strong>Features</strong></a> •
	<a href="#architecture"><strong>Architecture</strong></a> •
	<a href="#quick-start"><strong>Quick Start</strong></a> •
	<a href="#hardware"><strong>Hardware</strong></a> •
	<a href="#firmware-overview"><strong>Firmware Overview</strong></a> •
	<a href="#configuration"><strong>Configuration</strong></a> •
	<a href="#build-and-deploy"><strong>Build and Deploy</strong></a> •
	<a href="#troubleshooting"><strong>Troubleshooting</strong></a> •
	<a href="#license"><strong>License</strong></a>
</p>

</div>

<a id="overview"></a>
## 📘 Overview

This project is designed for a simple real-world problem: you want to leave the house without generating useless Reolink alerts or recordings from your own movement.

The firmware runs on an ESP32 and gives you a compact standalone interface:

- Short press the button to choose a mute duration.
- Long press to start the countdown.
- The ESP32 connects to WiFi and tells the Reolink NVR to disable notifications and recording on active channels.
- When the timer finishes, or when you cancel it, the firmware restores the original monitoring state.

The device also provides:

- a buzzer for immediate feedback
- a 16x2 LCD with clear user-facing status screens
- serial logs for development and troubleshooting

<a id="features"></a>
## ✨ Features

- One-button interface with short-press and long-press actions.
- Smart timer selection: `+1 minute` below 5 minutes, then `+5 minutes` increments.
- Reolink integration using direct API calls from the ESP32.
- Automatic restore of notifications and recording when the timer ends.
- Friendly 16x2 I2C LCD screens optimized for quick readability.
- Buzzer cues for selection, start, countdown warning, completion, and cancellation.
- WiFi and Reolink credentials managed through `platformio_secrets.ini`.
- Optional development logging controlled through `PIO_DEV_MODE`.
- GitHub Actions release workflow for automatic tagged releases on `main` pushes.

<a id="architecture"></a>
## 🏗️ Architecture

Gizmo Reolink Switch is built around three cooperating parts:

- 🔌 **ESP32 firmware** handles the button, timer logic, buzzer feedback, WiFi connection, LCD output, and Reolink API calls.
- 📺 **16x2 I2C LCD** provides the user-facing status screens for idle mode, countdown mode, and restore events.
- 🎥 **Reolink NVR** receives the API commands that mute and restore notifications and recording on active channels.

### 🔄 Data Flow

1. The user presses the physical button to choose a quiet-mode duration or start/cancel the timer.
2. The firmware updates the menu state, shows the current status on the LCD, and plays buzzer feedback.
3. When quiet mode starts, the ESP32 authenticates with the Reolink NVR and disables notifications and recording on active channels.
4. While the timer runs, the firmware keeps the display updated and warns near the end with short buzzer cues.
5. When the timer completes or is cancelled, the ESP32 restores the Reolink state and returns to the idle screen.

<a id="quick-start"></a>
## 🚀 Quick Start

### 📋 Requirements

- ESP32 development board
- Reolink NVR reachable on the same network as the ESP32
- 1 push button
- 1 buzzer
- 16x2 I2C LCD with backpack, detected at `0x27`
- VS Code with PlatformIO, or PlatformIO Core installed locally

### 🔧 Initial Setup

1. Clone the repository:

```bash
git clone https://github.com/open-gizmo/gizmo-reolink-switch.git
cd gizmo-reolink-switch
```

2. Create your local secrets file:

```bash
copy platformio_secrets.example.ini platformio_secrets.ini
```

3. Fill in:

- your WiFi SSID and password
- your Reolink host
- your Reolink username and password
- `PIO_DEV_MODE=1` if you want serial logs

4. Build the firmware:

```bash
pio run
```

5. Upload to the ESP32:

```bash
pio run --target upload
```

6. Open the serial monitor if needed:

```bash
pio device monitor
```

<a id="hardware"></a>
## 🔌 Hardware

### Pinout

Current pin assignments from the firmware configuration:

- Buzzer: `GPIO12`
- Button: `GPIO13`
- LCD SDA: `GPIO26`
- LCD SCL: `GPIO25`

### Display

- Type: `16x2 I2C LCD`
- I2C address: `0x27`
- Driver library: `hd44780_I2Cexp`

<a id="firmware-overview"></a>
## 🔍 Firmware Overview

### ⚙️ General

The main firmware entry point is [src/main.cpp](src/main.cpp). It:

- Initializes all modules.
- Halts safely if WiFi or Reolink credentials are missing.
- Polls the button.
- Updates the timer state machine.
- Keeps the LCD and buzzer in sync with the current state.

### 🧩 Firmware Layers

The firmware is split into focused modules under `src/`:

- 🔘 `button/` reads the physical button and emits semantic events.
- 🔔 `buzzer/` drives audible feedback.
- 📺 `display/` manages the LCD screens and layout.
- ⏱️ `menu/` implements timer selection and countdown state.
- 📡 `wifi/` connects the ESP32 to the local network.
- 🎥 `reolink/` authenticates against the NVR and toggles notifications and recording.
- 🧾 `serial/` handles development logs when dev mode is enabled.
- ⚙️ `config/` centralizes pins, timing values, credentials, and display settings.

### 🎥 Reolink integration

The Reolink API logic lives in [src/reolink/reolink.cpp](src/reolink/reolink.cpp).

Current implementation highlights:

- HTTPS API calls from the ESP32.
- Session login and token reuse.
- Retry on expired session.
- Active-channel discovery.
- Batch updates for notifications and recording state.

The firmware is designed to mute active channels when quiet mode starts and restore them afterward.

### 🎨 User interface

The LCD intentionally shows concise, human-friendly screens rather than raw log output. Serial logs stay verbose for debugging, while the display stays simple and readable for everyday use.

<a id="configuration"></a>
## ⚙️ Configuration

### Secrets file

The project uses PlatformIO extra configs:

```ini
[platformio]
extra_configs = platformio_secrets.ini
```

Example secrets file:

```ini
[env:esp32dev]
build_flags =
	-D PIO_WIFI_SSID='"your-wifi-ssid"'
	-D PIO_WIFI_PASSWORD='"your-wifi-password"'
	-D PIO_REOLINK_HOST='"your-reolink-host"'
	-D PIO_REOLINK_USERNAME='"your-reolink-username"'
	-D PIO_REOLINK_PASSWORD='"your-reolink-password"'
	-D PIO_DEV_MODE=0
```

Use [platformio_secrets.example.ini](platformio_secrets.example.ini) as the template.

### Key runtime settings

The main shared constants live in [src/config/config.h](src/config/config.h) and [src/config/config.cpp](src/config/config.cpp), including:

- pin assignments
- button debounce and long-press timing
- WiFi and HTTP timing
- timer step rules
- buzzer durations
- LCD geometry and address

<a id="build-and-deploy"></a>
## 🛠️ Build and Deploy

### 🔌 Firmware

Build:

```bash
pio run
```

Upload:

```bash
pio run --target upload
```

Monitor serial output:

```bash
pio device monitor
```

<a id="troubleshooting"></a>
## 🧰 Troubleshooting

- LCD powers on but shows no text: verify SDA/SCL wiring and confirm the backpack address matches the configured `0x27`.
- Button does nothing: verify the wiring against the input mode expected by the firmware.
- Quiet mode does not start: check WiFi connectivity and Reolink credentials first.
- NVR state does not restore: enable `PIO_DEV_MODE=1` and inspect the serial logs.

<a id="license"></a>
## 📄 License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.