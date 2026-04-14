# ClauBlink

Physical LED indicator that shows Claude AI activity status in real-time.

## Supported Hardware

| Board | LED Type | Connectivity |
|-------|----------|-------------|
| ESP32-C3 Super Mini (Lolin C3 Mini) | GPIO 8 (active-low) | USB Serial + WiFi |
| Waveshare RP2040-Zero | WS2812B NeoPixel (pin 16) | USB Serial only |

## LED States

| State | Command | Blink Rate | RP2040 Color | Behavior |
|-------|---------|-----------|--------------|----------|
| RESPONSE | `RESPONSE` | Fast (200ms) | Red | Claude is streaming a response |
| WORKING | `WORKING` | Slow (800ms) | Blue | Claude is using tools / thinking |
| DONE | `DONE` | Solid on | Green | Claude finished |
| IDLE | `OFF` | Off | — | LED off |

On ESP32-C3 (single-color LED), RESPONSE and WORKING are distinguished by blink speed.

## Setup

### Prerequisites

- [PlatformIO](https://platformio.org/)

### Build & Flash

```bash
# ESP32-C3
pio run -e esp32c3 -t upload

# RP2040 (first flash: hold BOOT + plug USB)
pio run -e rp2040 -t upload
```

### ESP32-C3 WiFi Setup

On first boot (no saved credentials), the device creates an AP:
1. Connect to WiFi `ClauBlink` (password: `12345678`)
2. Open `http://192.168.4.1`
3. Enter your WiFi SSID and password
4. Device restarts and connects to your WiFi

Once connected: `http://claublink.local/DONE`

Reset credentials: `curl http://claublink.local/reset`

## Usage

### notify.sh

```bash
./scripts/notify.sh RESPONSE   # Claude is responding
./scripts/notify.sh WORKING    # Claude is thinking
./scripts/notify.sh DONE       # Claude finished
./scripts/notify.sh OFF        # Turn off
```

Auto-detects USB serial first, falls back to WiFi HTTP via mDNS.

### Claude Code Hooks

Add to `~/.claude/settings.json` (or project-level `.claude/settings.json`):

```json
{
  "hooks": {
    "PreToolUse": [
      {
        "matcher": "",
        "hooks": [{ "type": "command", "command": "/path/to/ClauBlink/scripts/notify.sh PreToolUse" }]
      }
    ],
    "PostToolUse": [
      {
        "matcher": "",
        "hooks": [{ "type": "command", "command": "/path/to/ClauBlink/scripts/notify.sh PostToolUse" }]
      }
    ],
    "Stop": [
      {
        "matcher": "",
        "hooks": [{ "type": "command", "command": "/path/to/ClauBlink/scripts/notify.sh Stop" }]
      }
    ]
  }
}
```

Replace `/path/to/ClauBlink` with your actual install path.

**Hook logic** (`notify.sh`):
- `PreToolUse` → LED blinks fast (RESPONSE) — Claude is streaming
- `PostToolUse` → LED blinks slow (WORKING) — Claude is thinking/using tools, sets a flag
- `Stop` → if flag exists: LED solid (DONE); otherwise: LED blinks fast (RESPONSE, no tools were used)
- `OFF` → LED off, clears flag

**Multi-session slots**: pass slot number as second argument (0-4):
```json
{ "type": "command", "command": "/path/to/ClauBlink/scripts/notify.sh PreToolUse 2" }
```

## Project Structure

```
├── src/main.cpp        # Firmware (dual-board, preprocessor guards)
├── platformio.ini      # PlatformIO config (esp32c3 + rp2040 envs)
└── scripts/notify.sh   # Host-side controller script
```
