#!/bin/bash
# ClauBlink notify script
# Usage: ./notify.sh <HOOK_EVENT> [SESSION_SLOT]
# Hook events: PreToolUse, PostToolUse, Stop, OFF
# Session slot: 0-4 (default: 0)

EVENT="$1"
SLOT="${2:-0}"

if [ -z "$EVENT" ]; then
  exit 0
fi

FLAG_DIR="/tmp/claublink"
mkdir -p "$FLAG_DIR"
FLAG_FILE="$FLAG_DIR/tools_used_${SLOT}"

# Determine LED state based on hook event
case "$EVENT" in
  PreToolUse)
    STATE="RESPONSE:${SLOT}"
    ;;
  PostToolUse)
    touch "$FLAG_FILE"
    STATE="WORKING:${SLOT}"
    ;;
  Stop)
    if [ -f "$FLAG_FILE" ]; then
      STATE="DONE:${SLOT}"
      rm -f "$FLAG_FILE"
    else
      STATE="RESPONSE:${SLOT}"
    fi
    ;;
  OFF)
    STATE="OFF:${SLOT}"
    rm -f "$FLAG_FILE"
    ;;
  *)
    STATE="${EVENT}:${SLOT}"
    ;;
esac

# Try USB serial first
PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)

if [ -n "$PORT" ]; then
  stty -f "$PORT" 115200 2>/dev/null
  printf "%s\n" "$STATE" > "$PORT"
  exit 0
fi

# Fallback: WiFi HTTP (try mDNS, then cached IP)
CMD=$(echo "$STATE" | cut -d: -f1)
IP_CACHE="/tmp/claublink/ip"

# Try mDNS first
if curl -s --connect-timeout 2 --max-time 3 "http://claublink.local/${CMD}?slot=${SLOT}" > /dev/null 2>&1; then
  exit 0
fi

# Fallback: cached IP
if [ -f "$IP_CACHE" ]; then
  IP=$(cat "$IP_CACHE")
  curl -s --connect-timeout 1 --max-time 2 "http://${IP}/${CMD}?slot=${SLOT}" > /dev/null 2>&1
fi
