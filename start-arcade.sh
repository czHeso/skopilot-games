#!/usr/bin/env bash
# Spustí ŠkoPilot Games v celoobrazovkovém kiosku (Chromium).
# Použij v autostartu Raspberry Pi.

set -e

# Cesta k repu (uprav, pokud máš jinde)
APP_DIR="$HOME/skopilot-games"
URL="file://$APP_DIR/index.html"

# ── Debug log ───────────────────────────────────────────────────────
# Každý start zapíše průběh do ~/skopilot-arcade.log — když kiosk
# nenaběhne přes celou obrazovku, podívej se sem: tail ~/skopilot-arcade.log
LOG="$HOME/skopilot-arcade.log"
log(){ echo "[$(date '+%H:%M:%S')] $*" >> "$LOG"; }

# ── Pojistka proti dvojímu spuštění ─────────────────────────────────
# Když je skript v autostartu na víc místech (XDG autostart, lxsession,
# labwc/wayfire…), spustí se dvakrát. Druhé Chromium se připojí k už
# běžící instanci a otevře NOVÉ malé okno bez kiosk/fullscreen flagů —
# přesně ten „pruh" přes část obrazovky. Zámek drží i běžící Chromium
# (zdědí otevřený deskriptor), takže opakovaný start nic nerozbije.
exec 9>"$HOME/.skopilot-arcade.lock"
if ! flock -n 9; then
  log "druhý start zablokován — kiosk už běží (zkontroluj duplicitní autostart)"
  exit 0
fi

: > "$LOG"
log "start; session=${XDG_SESSION_TYPE:-?} WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-} DISPLAY=${DISPLAY:-} pid=$$"

# ── Volba prohlížeče ────────────────────────────────────────────────
# Výchozí je Chromium. Když zlobí, přepneš na Firefox bez úprav skriptu:
#   sudo apt install -y firefox
#   SKOPILOT_BROWSER=firefox ~/skopilot-games/start-arcade.sh
# (v autostartu: Exec=env SKOPILOT_BROWSER=firefox /home/pi/skopilot-games/start-arcade.sh)
if [ -n "${SKOPILOT_BROWSER:-}" ]; then
  BROWSER="$SKOPILOT_BROWSER"
elif command -v chromium >/dev/null 2>&1; then
  BROWSER=chromium
elif command -v chromium-browser >/dev/null 2>&1; then
  BROWSER=chromium-browser
elif command -v firefox >/dev/null 2>&1; then
  BROWSER=firefox
else
  echo "Chromium není nainstalovaný. Spusť: sudo apt install -y chromium-browser" >&2
  exit 1
fi

# ── Počkej na grafické prostředí ────────────────────────────────────
# Autostart se často spustí dřív, než kompozitor nastaví rozlišení displeje.
# Chromium by pak naběhl v malém okně vlevo nahoře místo celé obrazovky.
WAITED=0
for _ in $(seq 1 30); do
  if [ -n "${WAYLAND_DISPLAY:-}" ] || [ -n "${DISPLAY:-}" ]; then break; fi
  sleep 1; WAITED=$((WAITED+1))
done
sleep 4   # dej kompozitoru čas dokončit nastavení výstupu
log "graphics ready (waited ${WAITED}s + 4s grace)"

# Zjisti LOGICKÉ rozlišení obrazovky (zahrnuje i otočení displeje).
# Pořadí: xrandr (X11/XWayland) → wlr-randr (Wayland) → DRM → 1920x1080.
RES=""
if [ -n "${DISPLAY:-}" ] && command -v xrandr >/dev/null 2>&1; then
  RES="$(xrandr --current 2>/dev/null | sed -n 's/.*current \([0-9]\{2,\}\) x \([0-9]\{2,\}\).*/\1x\2/p' | head -n 1 || true)"
  [ -n "$RES" ] && log "resolution via xrandr: $RES"
fi
if [ -z "$RES" ] && command -v wlr-randr >/dev/null 2>&1; then
  RES="$(wlr-randr 2>/dev/null | sed -n 's/^ *\([0-9]\{2,\}x[0-9]\{2,\}\) px.*current.*/\1/p' | head -n 1 || true)"
  [ -n "$RES" ] && log "resolution via wlr-randr: $RES"
fi
if [ -z "$RES" ]; then
  RES="$(cat /sys/class/drm/card*-*/modes 2>/dev/null | head -n 1 || true)"
  [ -n "$RES" ] && log "resolution via DRM (pozor: nezná otočení): $RES"
fi
case "$RES" in
  *x*) : ;;
  *)   RES="1920x1080"; log "resolution fallback: $RES" ;;
esac
SCR_W="${RES%%x*}"
SCR_H="${RES##*x}"
log "using window size ${SCR_W}x${SCR_H}"

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

# ── Hudba na pozadí ─────────────────────────────────────────────────
# Pustí assets/Skyward_Dash.mp3 ve smyčce na maximální hlasitost.
# Běží nezávisle na prohlížeči — po `exec` do Chromia/Firefoxu hraje dál.
MUSIC="$APP_DIR/assets/Skyward_Dash.mp3"

# Systémovou hlasitost vytoč na maximum. Názvy ovládacích prvků se mezi
# zvukovými kartami liší, tak projedeme ty nejběžnější a chyby ignorujeme.
if command -v amixer >/dev/null 2>&1; then
  for CTL in Master PCM Headphone Speaker Digital Lineout; do
    amixer -q sset "$CTL" 100% unmute 2>/dev/null || true
  done
  log "hlasitost nastavena na maximum (amixer)"
fi

if [ -f "$MUSIC" ]; then
  # Vyber dostupný přehrávač a pusť ho v nekonečné smyčce.
  play_music(){
    if command -v mpg123 >/dev/null 2>&1; then
      mpg123 --quiet --loop -1 "$MUSIC"
    elif command -v ffplay >/dev/null 2>&1; then
      ffplay -nodisp -autoexit -loop 0 -loglevel quiet "$MUSIC"
    elif command -v mpv >/dev/null 2>&1; then
      mpv --no-video --really-quiet --loop=inf --volume=100 "$MUSIC"
    elif command -v cvlc >/dev/null 2>&1; then
      cvlc --intf dummy --quiet --loop "$MUSIC"
    else
      log "žádný audio přehrávač nenalezen (nainstaluj: sudo apt install -y mpg123)"
      return 1
    fi
  }
  # Hlídač: kdyby přehrávač spadl, smyčku restartuj (return 1 = není čím hrát).
  ( while play_music; do sleep 1; done ) >/dev/null 2>&1 &
  log "hudba na pozadí spuštěna: $MUSIC (pid=$!)"
else
  log "hudba nenalezena: $MUSIC"
fi

# ── Firefox kiosk (alternativa, když Chromium zlobí) ────────────────
case "$BROWSER" in
  *firefox*)
    export MOZ_ENABLE_WAYLAND=1
    log "launching $BROWSER (kiosk)"
    exec "$BROWSER" --kiosk "$URL"
    ;;
esac

# ── Chromium kiosk ──────────────────────────────────────────────────
# Vlastní trvalý profil: highscory (localStorage) přežijí restart —
# dřívější --incognito je mazal při každém vypnutí automatu.
PROFILE="$HOME/.config/skopilot-kiosk"
mkdir -p "$PROFILE"

# Smaž případný „crash" dialog, aby kiosk vždy naběhl čistě
PREF="$PROFILE/Default/Preferences"
[ -f "$PREF" ] && sed -i 's/"exit_type":"Crashed"/"exit_type":"Normal"/' "$PREF" || true

# Vynuť X11 backend (XWayland): nativní Wayland backend Chromia má na
# otočených displejích chyby v geometrii fullscreen oken — okno se po
# startu srazí do pruhu. Přepsat jde přes SKOPILOT_OZONE=wayland.
OZONE="${SKOPILOT_OZONE:-x11}"

log "launching $BROWSER (ozone=$OZONE, profile=$PROFILE)"
exec "$BROWSER" \
  --kiosk \
  --start-fullscreen \
  --window-position=0,0 \
  --window-size="${SCR_W},${SCR_H}" \
  --ozone-platform="$OZONE" \
  --user-data-dir="$PROFILE" \
  --no-first-run \
  --no-default-browser-check \
  --noerrordialogs \
  --disable-infobars \
  --disable-session-crashed-bubble \
  --disable-features=Translate,TranslateUI \
  --disable-translate \
  --overscroll-history-navigation=0 \
  --password-store=basic \
  --check-for-update-interval=31536000 \
  "$URL"
