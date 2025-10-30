# Nano Every 6-DOF Bluetooth Servo Controller

This project controls six hobby servos on an Arduino Nano Every over Bluetooth (HC-06) and includes a modern web UI to drive each joint with sliders, save presets, and log communication.

## Hardware
- Board: Arduino Nano Every
- Bluetooth: HC-06 (SoftwareSerial RX=D2, TX=D3) @ 9600 baud
- Servos and ranges (enforced in firmware):
  - A @ D4: 0–180
  - B @ D5: 45–90
  - C @ D6: 0–85
  - D @ D7: 90–180
  - E @ D8: 0–180
  - F @ D9: 90–180
- Power: 5V supply with adequate current; only one servo is powered at a time in firmware to reduce peaks.

## Firmware (PlatformIO)
- Environment: `nano_every`
- Location: `src/main.cpp`
- Build and upload from VS Code (PlatformIO):
  - Build: PlatformIO: Build
  - Upload: PlatformIO: Upload
- Serial baud (USB + BT): 9600

### Bluetooth protocol
- Commands are letter + angle, e.g. `A90`, `F150`.
- `?` prints HELP with ranges; `G` prints STATUS.
- Only one servo moves at a time (attach/detach to save power).

## Web app
A single-file web interface is available at:
- `webapp/servo_controller.html`

Open in Chrome or Edge (Web Serial API):
1. Open the file in the browser (double-click or drag into a tab).
2. Click "Connect to Bluetooth" and choose the HC-06 COM port.
3. Move sliders (A–F); the app throttles commands to avoid saturating serial.
4. Save/load presets and export as JSON.

## Troubleshooting
- If commands don’t go through reliably:
  - Ensure baud = 9600 on both firmware and web app.
  - Use a stable 5V supply. High current spikes from servos can cause resets.
  - Keep only one servo attached at a time (firmware does this already).
- If the web app can’t connect, use Chrome/Edge and allow serial permissions.

## Repository hygiene
- Build outputs (`.pio/`) and dependency caches (`libdeps/`) are ignored by `.gitignore`.
- VS Code workspace files are ignored except `extensions.json`.

---
Created October 2025. 