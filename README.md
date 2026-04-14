# ClauBlink

Physical LED indicator that shows Claude AI activity status in real-time.

## Supported Hardware

| Board | LED Type | Connectivity |
|-------|----------|-------------|
| ESP32-C3 Super Mini (Lolin C3 Mini) | GPIO 8 (active-low) | USB Serial + WiFi |
| Waveshare RP2040-Zero | WS2812B NeoPixel (pin 16) | USB Serial only |

## LED States

| State | Command | ESP32-C3 | RP2040 NeoPixel | Behavior |
|-------|---------|----------|-----------------|----------|
| RESPONSE | `RESPONSE` | Fast blink | Magenta solid | Stays on until next command |
| WORKING | `WORKING` | Slow blink | Blue blink | 200ms toggle |
| DONE | `DONE` | Solid on | Green solid | Stays on until next command |
| IDLE | `OFF` | Off | Off | LED off |

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

Add to `~/.claude/settings.json`:

```json
{
  "hooks": {
    "PreToolUse": [{ "command": "./scripts/notify.sh WORKING" }],
    "PostToolUse": [{ "command": "./scripts/notify.sh RESPONSE" }],
    "Stop": [{ "command": "./scripts/notify.sh DONE" }]
  }
}
```

## Project Structure

```
├── src/main.cpp        # Firmware (dual-board, preprocessor guards)
├── platformio.ini      # PlatformIO config (esp32c3 + rp2040 envs)
└── scripts/notify.sh   # Host-side controller script
```
