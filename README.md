# Real-Time Fire Alarm Detection System

An embedded IoT system that monitors ambient audio for fire alarm frequencies using FFT-based signal processing on a Raspberry Pi, with a web dashboard and instant push notifications.

![Dashboard](docs/dashboard.png)

---

## Overview

This project implements a full-stack detection pipeline: a C++ audio processing engine captures and analyzes audio in real time, a Python/Flask REST API serves detection state, and a React dashboard provides live monitoring from any device on the network. Designed for low-latency, resource-constrained hardware.

---

## Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  I2S MEMS Mic   │────▶│  C++ Detector    │────▶│  JSON Logs      │
│  (SPH0645LM4H)  │     │  (FFT Analysis)  │     │  status.json    │
└─────────────────┘     └──────────────────┘     └────────┬────────┘
                                                          │
                                                          ▼
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  ntfy.sh        │◀────│  Flask API       │◀────│  React Dashboard│
│  (Push Alerts)  │     │  (Python)        │     │  (Web UI)       │
└─────────────────┘     └──────────────────┘     └─────────────────┘
```

---

## Tech Stack

| Layer | Technology |
|---|---|
| Audio Processing | C++17, ALSA, FFTW3 |
| Backend API | Python, Flask |
| Frontend | React, Vite |
| Hardware | Raspberry Pi Zero W, I2S MEMS Microphone (SPH0645LM4H) |
| Notifications | ntfy.sh |

---

## Detection Algorithm

1. ALSA captures audio at 48 kHz via the I2S interface
2. FFTW3 applies a Fast Fourier Transform over 4096-sample windows (~85ms each)
3. Signal is filtered to the 3000–3600 Hz range (standard fire alarm frequency band)
4. A magnitude threshold of -20 dB gates noise from true detections
5. Pattern matching confirms 3+ consecutive beeps within a 10-second window to eliminate false positives
6. On confirmation, the event is logged and a push notification is dispatched

### Design Decisions

- **C++ over Python** — deterministic performance and lower latency for real-time audio processing
- **FFTW3** — industry-standard FFT library with highly optimized transforms
- **I2S over USB audio** — direct digital signal path eliminates analog noise floor
- **Flask** — minimal overhead, appropriate for a resource-constrained Pi Zero W

---

## Performance

| Metric | Value |
|---|---|
| Detection Latency | < 500ms |
| Sample Rate | 48 kHz |
| FFT Window Size | 4096 samples (~85ms) |
| CPU Usage (Pi Zero W) | ~15% |
| Memory Footprint | ~8 MB |

---

## Hardware

**Required components:**
- Raspberry Pi (Zero W, 3, or 4)
- I2S MEMS Microphone (Adafruit SPH0645LM4H or equivalent)
- MicroSD card (8GB+)

**Wiring:**

| Microphone Pin | Raspberry Pi GPIO |
|---|---|
| VDD | 3.3V |
| GND | GND |
| BCLK | GPIO18 |
| LRCL | GPIO19 |
| DOUT | GPIO21 |

Full wiring reference: [Adafruit I2S MEMS Microphone — Raspberry Pi Setup](https://learn.adafruit.com/adafruit-i2s-mems-microphone-breakout/raspberry-pi-wiring-test)

---

## Installation

**1. Clone the repository**
```bash
git clone https://github.com/tahmidft/realtime-fire-alarm-detector.git
cd realtime-fire-alarm-detector
```

**2. Install system dependencies**
```bash
sudo apt update
sudo apt install -y libasound2-dev libfftw3-dev python3-pip python3-venv
```

**3. Compile the detector**
```bash
g++ -O2 -o fire_alarm_detector src/fire_alarm_detector.cpp -lasound -lfftw3 -lm
```

**4. Set up Python environment**
```bash
python3 -m venv venv
source venv/bin/activate
pip install flask flask-cors requests
```

**5. Run the system**

Terminal 1 — start the detector:
```bash
sudo ./fire_alarm_detector
```

Terminal 2 — start the API server:
```bash
source venv/bin/activate
python3 api.py
```

**6. Open the dashboard**

Navigate to `http://<pi-ip-address>:5000` in any browser on the same network.

---

## Push Notification Setup

1. Open **Settings** in the dashboard
2. Enter a unique topic name (e.g., `apartment-fire-alarm`)
3. Enable notifications and save
4. Subscribe on your phone via the [ntfy app](https://ntfy.sh) or at `https://ntfy.sh/<your-topic>`

---

## Project Structure

```
realtime-fire-alarm-detector/
├── src/
│   ├── fire_alarm_detector.cpp   # Core FFT detection engine
│   └── audio_capture.cpp         # ALSA audio capture
├── api.py                        # Flask REST API
├── frontend/                     # React dashboard
│   ├── src/
│   │   ├── App.jsx
│   │   └── App.css
│   └── dist/
├── docs/
│   └── dashboard.png
└── README.md
```

---

## Roadmap

- [ ] ML-based classifier for improved detection accuracy across alarm models
- [ ] Multi-sensor fusion (smoke, CO, temperature)
- [ ] Remote access via Tailscale
- [ ] Home Assistant integration
- [ ] Historical analytics dashboard

---

## Author

**Farhan Tahmid** — Software Engineer

[GitHub](https://github.com/tahmidft) · Built to demonstrate embedded systems, real-time signal processing, and full-stack development.

---

## License

MIT — see [LICENSE](LICENSE) for details.

