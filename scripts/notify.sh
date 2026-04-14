#!/bin/bash
# Usage: ./notify.sh RESPONSE|WORKING|DONE|OFF
# Mode 1: USB serial (auto-detect)
# Mode 2: WiFi HTTP via mDNS

STATE="$1"
if [ -z "$STATE" ]; then
  exit 0
fi

# Try USB serial first
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

if [ -n "$PORT" ]; then
  stty -f "$PORT" 115200 2>/dev/null
  printf "%s\n" "$STATE" > "$PORT"
  exit 0
fi

# Fallback: WiFi HTTP via mDNS
curl -s --connect-timeout 1 "http://claublink.local/$STATE" > /dev/null 2>&1
