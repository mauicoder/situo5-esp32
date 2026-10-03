# 🪟 situo5-esp32

### ESP32 Somfy Situo 5 io-homecontrol® Remote Emulator & Smart Home Gateway

[![Web Flasher](https://img.shields.io/badge/⚡_Web_Flasher-Install_in_Browser-007acc?style=for-the-badge&logo=googlechrome&logoColor=white)](https://mauicoder.github.io/situo5-esp32/flasher/)
[![Home Assistant](https://img.shields.io/badge/Home_Assistant-MQTT_Auto--Discovery-41BDF5?style=for-the-badge&logo=homeassistant&logoColor=white)](https://www.home-assistant.io/)
[![Hardware](https://img.shields.io/badge/Hardware-ESP32_+_LoRa_868MHz-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![License](https://img.shields.io/badge/License-CC0_1.0-lightgrey?style=for-the-badge)](LICENSE)

---

> ### ⚡ Flash your board in under 60 seconds — No command line or IDE needed!
> Head over to the **[Web Firmware Flasher](https://mauicoder.github.io/situo5-esp32/flasher/)** in Google Chrome, Microsoft Edge, or Brave. Connect your ESP32 via USB and install the firmware with a single click.

---

**situo5-esp32** transforms an inexpensive ESP32 LoRa board into a bridge for your **Somfy Situo 5 io-homecontrol®** motorized blinds, roller shutters, and awnings.

Forget expensive proprietary hubs and vendor lock-in: control your covers directly from your local network through an on-board web portal or seamlessly integrate them into **Home Assistant** via MQTT Auto-Discovery—all 100% locally with zero cloud dependence.

---

## ✨ Features

* 🎮 **5-Channel Remote Emulation**: Emulates all 5 independent channels of a Somfy Situo 5 remote.
* ⚡ **1-Click Web Flashing**: Install firmware directly from your browser—no compiling, no Python, no IDE required.
* 🪄 **Automatic Key & Device Learning**: Simply press the **PROG** button on your physical remote; the ESP32 sniffs the RF packets, extracts the 1-Way AES encryption key, and captures your awning/shutter addresses automatically.
* 📱 **Hassle-Free QR Wi-Fi Setup**: Scan the Wi-Fi provisioning QR code right off the ESP32's OLED screen using the free ESP BLE Prov app.
* 🏡 **Native Home Assistant Integration**: Full MQTT Auto-Discovery creates instant Cover entities with **Open**, **Close**, and **Stop / My** controls grouped under a dedicated Gateway device.
* 🌐 **Built-in Web Portal**: Live packet sniffer, manual command triggers (`UP`, `MY`, `DOWN`, `POLL`), device naming, and MQTT broker settings.
* 📺 **OLED Live Status**: Displays Wi-Fi IP address, live radio traffic count, MQTT status, and received packet summaries.

---

## 📟 Supported Hardware

This project runs on ESP32 development boards paired with a LoRa radio transceiver configured for FSK modulation on the European 868 MHz band (**868.25 MHz – 869.85 MHz**):

| Board | Microcontroller | Radio Module | Display | Status |
| :--- | :--- | :--- | :--- | :--- |
| **Heltec WiFi LoRa 32 (V3 / V3.1)** | ESP32-S3 | SX1262 (868 MHz) | 0.96" OLED | ✅ Fully Supported & Tested |
| **LilyGo TTGO LoRa32 (T3 V1.6 / V2.1)** | ESP32 | SX1276 (868 MHz) | 0.96" OLED | ✅ Fully Supported & Tested |

> [!NOTE]
> Other ESP32 boards with SX126x/SX127x modules may work through RadioLib, but might require custom pin configuration adjustments.

---

## 🚀 Quick Start Guide

Get your bridge running in 5 simple steps:

### 1️⃣ Flash with the Web Flasher
1. Connect your ESP32 board to your computer using a reliable USB data cable.
2. Open the **[Web Firmware Flasher](https://mauicoder.github.io/situo5-esp32/flasher/)** in a supported browser (Google Chrome, Microsoft Edge, Brave, or Opera).
3. Select your hardware model (**Heltec V3** or **LilyGo T3**) and choose the **Release** profile.
4. Click **Install Firmware**, pick your board's serial port from the popup, and let the installer finish.

### 2️⃣ Connect to Wi-Fi via BLE
1. Install the free **ESP BLE Provisioning** app on your phone:
   * [Download for iOS (App Store)](https://apps.apple.com/app/esp-ble-provisioning/id1473535741)
   * [Download for Android (Google Play)](https://play.google.com/store/apps/details?id=com.espressif.provble)
2. Launch the app and scan the **QR code** displayed directly on your ESP32's OLED screen.  
   *(Alternatively, select manual pairing: Device name `PROV_IoHome`, Proof of Possession: `iown1234`)*.
3. Select your home Wi-Fi network and submit credentials. Once connected, the board's OLED will show its assigned IP address.

### 3️⃣ Configure MQTT
1. Open your browser and navigate to `http://<ESP32_IP_ADDRESS>` (port 80).
2. Scroll to the **MQTT Configuration** section:
   * Enter your MQTT Broker IP address, port (default `1883`), username, and password.
   * Click **Save MQTT**. The ESP32 will connect to your broker immediately.

### 4️⃣ Clone Your Physical Remote
1. Select the desired channel on your existing physical Somfy Situo 5 remote.
2. **Capture the AES Key**: Press and hold the **PROG** button on the back of your physical remote until the motor jogs (or ~2 seconds). The ESP32 sniffs the 1-Way Key Transfer frame, extracts the AES stack key, and saves it into NVRAM.
3. **Capture the Device Address**: Press **UP** or **DOWN** on the physical remote. The ESP32 intercepts the frame and automatically links the shutter/awning's address to that channel.
4. *(Repeat for up to 5 channels/devices).*

### 5️⃣ Name & Control in Home Assistant
1. In the ESP32 web interface, enter a friendly name for each learned channel (e.g., *"Living Room Blind"*, *"Terrace Awning"*) and click **Save Name**.
2. Open **Home Assistant**. Your newly cloned devices will appear automatically under the **MQTT** integration as ready-to-use **Cover** entities grouped under the `ESP32 io-homecontrol Gateway`.
3. You're all set! Control them from your dashboard, create automations, or ask voice assistants to open and close your blinds.

---

<details>
<summary>🛠️ <strong>Advanced: Building & Flashing via PlatformIO</strong></summary>

If you prefer compiling from source or contributing to the codebase:

1. Clone this repository:
   ```bash
   git clone https://github.com/mauicoder/situo5-esp32.git
   cd situo5-esp32
   ```

2. Build and flash using [PlatformIO](https://platformio.org/):
   ```bash
   # For Heltec WiFi LoRa 32 V3:
   pio run -e heltec_wifi_lora_32_V3 --target upload

   # For LilyGo T3 V1.6:
   pio run -e lilygo_t3_v16 --target upload
   ```

3. Open the serial monitor:
   ```bash
   pio device monitor -b 115200
   ```
</details>

---

## 🙏 Acknowledgments

This software implementation is heavily based on the incredible reverse-engineering work of the io-homecontrol protocol. A huge thanks to **Velocet** for the big work performed on the documentation and experimentation in the parent repository: **[Velocet/iown-homecontrol](https://github.com/Velocet/iown-homecontrol)**.

Kudos to all the persons that contributed to the progress of the project!

---

## ⚠️ Disclaimer

> [!CAUTION]
> This tool is designed for educational and testing purposes, provided "as is", without warranty of any kind. Creators and contributors are not responsible for any misuse or damage caused by this tool. Keep in mind that it is forbidden in most countries to try to interact with io-homecontrol devices that are not yours and you may be sued for doing it.

_I give limited support depending on my free time._
