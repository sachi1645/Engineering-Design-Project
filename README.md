# Wireless Emergency Alert System for Live Performers

> **Group Debuggers** > *Department of Electronic & Telecommunication Engineering, University of Moratuwa*

## 📖 Abstract
Live performance environments demand fast, discreet, and effective communication between performers and the technical crew. Conventional methods such as hand signals or physically reaching out to technicians are inefficient and disruptive. 

This project presents a **Wireless Emergency Alert System** designed specifically for stage performers. Built around a custom PCB featuring the **ESP32-WROOM-32** microcontroller, the wearable device enables one-touch alert transmission over a local Wi-Fi network. Alerts are displayed in real-time on a mobile-responsive web dashboard.

## 🚀 Key Features
* **Real-Time Alerts:** Instantly displays performer name, device ID, and timestamp on the dashboard when the alert button is pressed.
* **Offline Operation:** Runs entirely on a local Wi-Fi network (LAN) with no internet dependency, ensuring reliability in any venue.
* **Audible Notifications:** The dashboard triggers an alert sound to grab the crew's attention during live scenarios.
* **Cross-Platform Dashboard:** A mobile-responsive web app (Flask) that works on laptops, tablets, and phones.
* **Easy Access:** Generates a QR code for the server URL, allowing devices to join the dashboard instantly.
* **Battery Operated:** Powered by a 3.7V 500mAh Li-Po battery with up to 5 hours of active usage.

## 🛠️ Tech Stack
### Hardware
* **Microcontroller:** ESP32-WROOM-32 Chip
* **Power:** 3.7V 500mAh Li-Po Battery with TP4056 Charging Module
* **Connectivity:** 2.4GHz Wi-Fi (Local Network)
* **Interface:** Tactile Push Button, Status LED, Slide Switch

### Software
* **Firmware:** Arduino C++ (HTTP POST, UDP Discovery)
* **Backend:** Python Flask (Web Server, Alert Queue)
* **Frontend:** HTML, CSS, JavaScript (WebSockets for live updates)

## 🏗️ System Architecture
The system consists of two main parts:
1.  **Wearable Transmitter (ESP32):** Reads button inputs and sends alert data via Wi-Fi.
2.  **Central Server (Flask):** Hosted on a PC or Raspberry Pi, it receives alerts and broadcasts them to the dashboard.

**Data Flow:**
`Button Press` -> `ESP32` -> `Wi-Fi Router` -> `Flask Server` -> `Web Dashboard`

## 🔌 Hardware Specifications
| Component | Specification |
|-----------|---------------|
| **Current Draw (Idle)** | ~80-100 mA |
| **Current Draw (Peak)** | ~150-160 mA |
| **Battery Life** | ~4-5 Hours (Active) |
| **Charging Time** | ~1.5-2 Hours |

## 🚀 Getting Started

### Prerequisites
* **Hardware:** ESP32 Development Board (or custom PCB), Push Button, LED.
* **Software:** Arduino IDE, Python 3.x.

### 1. Firmware Setup (ESP32)
1.  Open the firmware folder in **Arduino IDE**.
2.  Install the `ESP32` board manager.
3.  Update the `ssid` and `password` variables to match your local router.
4.  Upload the code to the ESP32.

## 👥 Team: Group Debuggers
* **Manitha Ayanaja** - Firmware, Web App, PCB
* **Sachinthaka Dilhara** - PCB Design
* **Kaveesha Divyanga** - Enclosure Design
* **Mishen Janodya** - PCB Design

## 📄 License
This project is submitted for the **EN1190 Engineering Design Project** module at the University of Moratuwa.
