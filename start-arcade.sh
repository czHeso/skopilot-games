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

# ── Skrytí kurzoru myši ─────────────────────────────────────────────
# 1) X11: unclutter skryje kurzor po 0.5 s nečinnosti
command -v unclutter >/dev/null 2>&1 && unclutter -idle 0.5 &

# 2) Wayland (labwc/wayfire na Bookworm) i X11: unclutter tam nefunguje,
#    proto si vygenerujeme plně průhledný kurzorový motiv „blank" a řekneme
#    Chromiu, ať ho používá — kurzor je pak neviditelný v celém kiosku.
BLANK_THEME="$HOME/.icons/blank"
if [ ! -f "$BLANK_THEME/cursors/left_ptr" ] && command -v python3 >/dev/null 2>&1; then
  mkdir -p "$BLANK_THEME/cursors"
  python3 - "$BLANK_THEME/cursors/left_ptr" <<'PY'
import struct, sys
# Xcursor soubor s jediným 1x1 plně průhledným obrázkem (nominal size 24)
header = struct.pack('<4sIII', b'Xcur', 16, 0x10000, 1)
toc    = struct.pack('<III', 0xFFFD0002, 24, 28)
chunk  = struct.pack('<IIIIIIIII', 36, 0xFFFD0002, 24, 1, 1, 1, 0, 0, 50)
open(sys.argv[1], 'wb').write(header + toc + chunk + struct.pack('<I', 0))
PY
  printf '[Icon Theme]\nName=blank\n' > "$BLANK_THEME/index.theme"
  for n in default arrow pointer hand1 hand2 text xterm crosshair watch left_ptr_watch progress wait; do
    ln -sf left_ptr "$BLANK_THEME/cursors/$n"
  done
fi
if [ -f "$BLANK_THEME/cursors/left_ptr" ]; then
  export XCURSOR_THEME=blank
  export XCURSOR_SIZE=24
  export XCURSOR_PATH="$HOME/.icons:/usr/share/icons"
fi

# Smaž případný „crash" dialog, aby kiosk vždy naběhl čistě
PREF="$HOME/.config/chromium/Default/Preferences"
[ -f "$PREF" ] && sed -i 's/"exit_type":"Crashed"/"exit_type":"Normal"/' "$PREF" || true

exec "$BROWSER" \
  --kiosk \
  --start-fullscreen \
  --noerrordialogs \
  --disable-infobars \
  --disable-session-crashed-bubble \
  --disable-features=Translate,TranslateUI \
  --disable-translate \
  --overscroll-history-navigation=0 \
  --incognito \
  --password-store=basic \
  --check-for-update-interval=31536000 \
  "$URL"
