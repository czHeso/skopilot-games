#!/usr/bin/env bash
# Spustí ŠkoPilot Games v celoobrazovkovém kiosku (Chromium).
# Použij v autostartu Raspberry Pi.

set -e

# Cesta k repu (uprav, pokud máš jinde)
APP_DIR="$HOME/skopilot-games"
URL="file://$APP_DIR/index.html"

# Najdi binárku Chromia (na Bookworm bývá 'chromium', na starších 'chromium-browser')
if command -v chromium >/dev/null 2>&1; then
  BROWSER=chromium
elif command -v chromium-browser >/dev/null 2>&1; then
  BROWSER=chromium-browser
else
  echo "Chromium není nainstalovaný. Spusť: sudo apt install -y chromium-browser" >&2
  exit 1
fi

# Vypni šetřič / blank obrazovky (pod X11; pod Waylandem řeš v raspi-config)
xset s off      2>/dev/null || true
xset -dpms      2>/dev/null || true
xset s noblank  2>/dev/null || true

# Skryj kurzor po 0.5 s nečinnosti (pokud je unclutter nainstalovaný)
command -v unclutter >/dev/null 2>&1 && unclutter -idle 0.5 &

# Smaž případný „crash" dialog, aby kiosk vždy naběhl čistě
PREF="$HOME/.config/chromium/Default/Preferences"
[ -f "$PREF" ] && sed -i 's/"exit_type":"Crashed"/"exit_type":"Normal"/' "$PREF" || true

exec "$BROWSER" \
  --kiosk \
  --start-fullscreen \
  --noerrordialogs \
  --disable-infobars \
  --disable-session-crashed-bubble \
  --disable-features=Translate \
  --overscroll-history-navigation=0 \
  --incognito \
  --password-store=basic \
  --check-for-update-interval=31536000 \
  "$URL"
