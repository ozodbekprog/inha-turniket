#!/bin/bash
# INHA Turniket — avtonom ko'prik (bridge)
# Vazifasi: tunnel URL o'zgarsa, ESP ga yangi manzilni USB orqali avtomatik yuboradi.
# Har 30 soniyada tekshiradi. systemd xizmat sifatida ishlaydi.
DIR="$(cd "$(dirname "$0")" && pwd)"
LOG="$DIR/logs/cloudflared.log"
URLFILE="$DIR/tunnel_url.txt"

last=""
while true; do
  cur=$(grep -oE "https://[a-z0-9-]+\.trycloudflare\.com" "$LOG" 2>/dev/null | tail -1)
  cur="${cur#https://}"
  if [ -n "$cur" ] && [ "$cur" != "$last" ]; then
    last="$cur"
    echo "$cur" > "$URLFILE"
    echo "$(date '+%F %T') tunnel URL: $cur" >> "$DIR/logs/bridge.log"
    if [ -c /dev/ttyACM0 ]; then
      printf "SETHOST=%s\n" "$cur" > /dev/ttyACM0 2>/dev/null \
        && echo "$(date '+%F %T') ESP ga yuborildi" >> "$DIR/logs/bridge.log"
    fi
  fi
  sleep 30
done
