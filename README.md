# IKEA Warehousing 4.0 — Smart Warehouse IoT System

Transforming a traditional warehouse into a smart, data-driven warehouse ("Warehousing 4.0") using the Internet of Things, on-device machine learning, and a unified cloud dashboard.

> Academic group project for **COMP8295 – Industrial Applications of IoT**, Macquarie University (2026).

---

## Overview

This project implements an end-to-end IoT solution that lets a warehouse **sense its own state, reason about it, and act when conditions drift out of bounds** — in contrast to a traditional facility that relies on periodic manual checks. Four independent use cases run on low-cost embedded hardware and report to a single shared cloud dashboard, so an operator sees the whole warehouse at once rather than switching between disconnected tools.

## Use Cases

The system is built around four use cases:

1. **Warehouse Safety & Inventory Management** — fall detection and perimeter-intrusion sensing with immediate local alarms, plus cloud-synchronised stock tracking.
2. **Visual Package Inspection** — an on-device image classifier flags damaged packages on the conveyor in real time.
3. **Environmental & Air-Quality Monitoring with Prediction** — monitors temperature, humidity, pressure, and air quality, forecasts breaches 30–60 minutes ahead with a lightweight regression model, and closes the loop by driving a fan.
4. **Fire Detection (TinyML)** — an on-device sensor-fusion model combines a camera image with a temperature reading for early fire detection that does not depend on a network connection.

> This repository covers my contributions as **IoT Design Engineer**: Use Case 3 (Environmental & Air-Quality Monitoring) and Use Case 4 (Fire Detection), plus consolidation of the final report and presentation.

## Key Features

- On-device ("edge") machine-learning inference, so time-critical decisions don't depend on a network round-trip
- Multi-sensor fusion for more robust detection than any single sensor
- Short-horizon forecasting to warn *before* a threshold is breached, not just react to it
- A closed sense–predict–act loop (e.g. automatically switching a fan)
- A single shared cloud dashboard unifying all use cases

## Tech Stack

| Area | Tools / Technologies |
|------|----------------------|
| Hardware | Arduino MKR WiFi 1010, Arduino MKR IoT Carrier Rev2, Arduino Nano 33 BLE Sense, OV7675 camera, BME688 sensor |
| On-device ML | TinyML, Edge Impulse, TensorFlow Lite Micro |
| Forecasting | Python, linear regression over a rolling window |
| Connectivity | Wi-Fi, BLE, MQTT, Arduino IoT Cloud |
| Project management | ProjectLibre (Gantt, dependencies, milestones) |



## Getting Started

### Prerequisites

- [Arduino IDE version, e.g. 2.x]
- [Required board packages, e.g. Arduino MBED OS Nano Boards]
- [Required libraries, e.g. Arduino_BME68x, ArduinoBLE]
- Python [version] for the forecasting service
- An Arduino IoT Cloud account (for the dashboard)





## Acknowledgements

Developed for COMP8295 – Industrial Applications of IoT, Macquarie University, under Dr. Adnan Mahmood.
