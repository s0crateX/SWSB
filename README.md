# SWSB - Smart Waste Sorting Bin

An ESP32-based automated waste sorting bin that classifies waste into:
- Biodegradable (paper)
- Non-biodegradable (plastic/metal)

The device exposes a built-in web interface over Wi-Fi Access Point mode, so you can control it directly from a phone or laptop without a router.

## What This Project Does

When an item is placed in the bin:
1. The capacitive sensor checks for plastic/metal.
2. The IR sensor checks for paper.
3. The ESP32 decides the category.
4. The matching servo opens the flap.
5. The web dashboard updates the counter.

## Current Firmware Behavior

The firmware in `IKADUHA.ino` includes these fixes:
- Auto mode now triggers only when a sensor detects an item.
- Empty-bin sorting is prevented in `classifyAndSort()`.
- Wi-Fi runs in Access Point mode (`WiFi.softAP`) for offline demos.
- Servo movement helper formatting/structure is clean and consistent.

## Bugs and Errors Found (Fixed)

The original firmware had four issues. These are now fixed in `IKADUHA.ino`.

| # | Type | Original line(s) in old code | Issue | Impact | Fix |
|---|---|---|---|---|---|
| 1 | Bug (Critical) | `IKADUHA.ino:145` | Auto mode condition was always true | Bin kept sorting even when empty | Removed always-true branch from condition |
| 2 | Bug (High) | `IKADUHA.ino:168-172` | Fallback path sorted as plastic when no sensor detected item | False plastic count and unnecessary servo movement | Return early when no item is detected |
| 3 | Error (High) | `IKADUHA.ino:6-7`, `IKADUHA.ino:86-100` | Wi-Fi used `WiFi.begin()` (router dependency) | Web UI unavailable without matching external router | Switched to `WiFi.softAP()` |
| 4 | Error (Low) | `IKADUHA.ino:179` | `slowMoveServo()` indentation/style inconsistency | Harder to maintain and review safely | Cleaned structure and indentation |

### Bug 1 (Auto mode always triggered)

Original location in old code: `IKADUHA.ino:145`

Original:

```cpp
if (capVal == LOW || irVal == 0 || (capVal == HIGH && irVal == 1)) {
```

Fixed:

```cpp
if (capVal == LOW || irVal == 0) {
```

### Bug 2 (Sorted with empty bin)

Original location in old code: `IKADUHA.ino:168-172`

Original:

```cpp
} else {
    Serial.println("Plastic detected (fallback) -> Servo2 (right)");
    sortPlastic();
    nonBioCount++;
}
```

Fixed:

```cpp
} else {
    Serial.println("No item detected, skipping.");
    return;
}
```

### Bug 3 (Router-dependent Wi-Fi setup)

Original location in old code: `IKADUHA.ino:6-7`, `IKADUHA.ino:86-100`

Original:

```cpp
WiFi.begin(ssid, password);
```

Fixed:

```cpp
WiFi.softAP(ssid, password);
```

### Error 4 (Indentation issue in `slowMoveServo()`)

Original location in old code: `IKADUHA.ino:179`

Original:

```cpp
if (currentAngle < targetAngle) {
for (int pos = currentAngle; pos <= targetAngle; pos++) {
```

Fixed:

```cpp
if (currentAngle < targetAngle) {
    for (int pos = currentAngle; pos <= targetAngle; pos++) {
```

## Pin Mapping

| Function | ESP32 Pin | Notes |
|---|---|---|
| Capacitive sensor output | GPIO 21 | Detects plastic/metal |
| IR sensor output | GPIO 19 | Detects paper |
| Manual push button | GPIO 4 | Active LOW, `INPUT_PULLUP` |
| Servo 1 (left flap) | GPIO 13 | Paper bin |
| Servo 2 (right flap) | GPIO 14 | Plastic bin |

## Hardware Requirements

- 1x ESP32 development board (example: DevKit V1)
- 2x SG90 servo motors (or equivalent)
- 1x Capacitive sensor (digital output)
- 1x IR obstacle sensor (digital output)
- 1x Push button (optional)
- Jumper wires
- External 5V power supply for servos
- USB cable for programming

## Important Power Note

Do not power servos from the ESP32 3.3V pin.

Use a separate 5V supply for servos and make sure grounds are common:
- Servo PSU GND <-> ESP32 GND
- Sensor GND <-> ESP32 GND

## Software Setup

1. Install Arduino IDE:
   - https://www.arduino.cc/en/software
2. Add ESP32 boards package:
   - `File > Preferences > Additional Board Manager URLs`
   - Add:
     `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Install board package:
   - `Tools > Board > Board Manager`
   - Install `esp32 by Espressif Systems`
4. Install ESP32Servo library:
   - Option A: Copy `pr/ESP32Servo` to your Arduino libraries folder.
   - Option B: `Sketch > Include Library > Add .ZIP Library` and select the folder/zip.
5. Open `IKADUHA.ino`.
6. Select board and port:
   - Board: `ESP32 Dev Module`
   - Port: your ESP32 COM port
7. Upload firmware.
   - If upload stalls at `Connecting...`, hold `BOOT` until flashing starts.

## Wi-Fi and Web Interface

After boot, connect your device to:
- SSID: `SmartBin`
- Password: `smartbin123`

Then open:
- `http://192.168.4.1`

Available controls:
- `Automatic Mode`: sorts automatically when sensors detect an item
- `Manual Mode`: disables automatic sorting
- `Sort Item`: one-time sort trigger
- `Reset Counters`: resets paper/plastic counters

## Sensor Decision Logic

- `capVal == LOW` -> plastic/metal -> Servo 2 (right flap)
- `irVal == 0` -> paper -> Servo 1 (left flap)
- no detection -> no action

## Servo Calibration

Adjust these values in `IKADUHA.ino` based on your physical build:

```cpp
int servo1Home = 0;
int servo1Left = 90;
int servo2Home = 90;
int servo2Right = 180;
```

If motion is too fast/slow, tune:

```cpp
int stepDelay = 10; // ms per degree
```

## Troubleshooting

- Servos not moving:
  - Recheck signal pins (`GPIO 13`, `GPIO 14`) and external 5V supply.
- ESP32 resets/twitching servos:
  - Usually power instability. Use separate servo power and common ground.
- `SmartBin` network not visible:
  - Confirm firmware upload succeeded and reset ESP32.
- Web page not loading:
  - Ensure device is connected to `SmartBin`, then open `http://192.168.4.1`.
- Wrong bin opens:
  - Verify sensor placement and pin wiring.
- Upload fails:
  - Hold `BOOT` during upload and use a data-capable USB cable.

## Project Structure

```text
SWSB/
|- IKADUHA.ino
|- README.md
|- SETUP_GUIDE.txt
`- pr/
   |- ESP32Servo/
   `- sketch_oct17a/
```

## Notes

- `SETUP_GUIDE.txt` includes extra setup-oriented notes.
