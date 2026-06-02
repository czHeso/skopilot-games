# 🕹️ ŠkoPilot Family — Arcade

Sbírka mini‑her s maskotem **ŠkoPilot**, připravená pro arkádový automat
**Arcade1Up Pac‑Man 40th Anniversary** poháněný **Raspberry Pi 4**.

Vše je čisté HTML/JS/Canvas — žádný build, žádné závislosti. Stačí otevřít
`index.html` v prohlížeči.

Launcher (v angličtině, v barvách loga **ŠkoPilot Family**) je rozdělený do kategorií:

- **ŠkoPilot** — *Flappy ŠkoPilot*
- **ŠkoPilot Clip** — *ŠkoPilot Clip* (Tetris na čas) — hratelné
- **ŠkoPilot Pro** — *Agent Builder* (plošinovka) — hratelné

## Co je uvnitř

| Cesta | Hra | Žánr | Ovládání |
|-------|-----|------|----------|
| `index.html` | **Arcade launcher** | retro výběr her | šipky + Enter / myš |
| `flappy/` | **Flappy ŠkoPilot** | letová obratnost | Mezerník / klik = skok |
| `clip/` | **ŠkoPilot Clip** | Tetris na čas | šipky = posun, **↑/X** = rotace, Mezerník = hard drop |
| `pro/` | **ŠkoPilot Pro** | plošinovka, stavba agenta | šipky/joystick + skok, **X** = výstřel |

### ŠkoPilot Clip — Tetris na čas
Dole je předem napsané slovo **CLIP** pixel grafikou — každá buňka jiný
**odstín žluté**. Cílem je nápis **co nejrychleji zničit**: padají tetromina a
když spojíš celý řádek, zmizí (klasicky jako Tetris) a s ním i kousek nápisu.
Občas (**fakt malá šance**) spadne místo dílu **bomba**, která vybuchne a zničí
**2×2** kolem dopadu. V bočním panelu vidíš **další padající díl**, aktuální
**čas** a **síň slávy nejrychlejších časů** (čím méně, tím líp). Ovládání je
nahoře v souboru přehledně v `keydown` — snadno upravíš pro USB enkodér.

### ŠkoPilot Pro — Agent Builder
V mladoboleslavském cloudu spadly moduly. ŠkoPilot probíhá 3 levely (IT&Dev →
Výroba → HR), sbírá ztracené **tooly** a na konci levelu z nich u „šasi"
sestaví **AI Agenta**. Má časový limit (**Server Timeout**), bugy (nepřátele),
power‑upy (☕ zrychlení, 🛡 firewall) a arkádovou **síň slávy** se zadáváním
3 písmen jména. Rozlišení 4:3 (800×600), škáluje se na celou obrazovku.
Mapování kláves je nahoře v souboru v `KEYMAP` (Input Manager) — snadno upravíš
pro svůj USB enkodér.

Všechny hry:
- jdou **přes celou obrazovku** (automaticky se přizpůsobí poměru displeje),
- **ESC** kdykoliv vrátí do menu, vlevo nahoře je i tlačítko **◀ MENU**,
- ukládají nejlepší skóre do prohlížeče (`localStorage`).

## Avatar maskota

Úvodní obrazovka a hry používají obrázek maskota. Ulož celou postavičku jako:

```
assets/skopilot.png      (nejlépe PNG s průhledným pozadím, na výšku ~500–800 px)
```

Když soubor chybí, vše automaticky použije hlavu `assets/skopilotHead.png`,
takže nic nespadne.

> Úvodní obrazovka je v **8-bit retro** stylu (hvězdné pole, CRT scanlines,
> pixel font *Press Start 2P*). Font je přibalený v `assets/fonts/`, takže
> kiosk funguje i **bez internetu**.

---

## 🚀 Jak to dostat na Raspberry Pi 4

> Příkazy spouštěj v terminálu na Raspberry Pi (přes screen‑sharing okno
> v Raspberry Pi Connect nebo přes SSH).

### 1) Stáhni repo

```bash
cd ~
git clone https://github.com/czheso/skopilot-games.git
cd skopilot-games
```

> Dokud změny nejsou v `main`, přepni na vývojovou větev:
> ```bash
> git checkout claude/dazzling-ptolemy-cOulb
> ```

Aktualizace později:
```bash
cd ~/skopilot-games && git pull
```

### 2) Nainstaluj prohlížeč (pokud chybí)

```bash
sudo apt update
sudo apt install -y chromium-browser unclutter
```
> Na novějším Raspberry Pi OS (Bookworm) se balíček může jmenovat `chromium`.
> Spouštěcí skript si poradí s oběma názvy.

### 3) Vyzkoušej ručně

```bash
~/skopilot-games/start-arcade.sh
```

Mělo by se otevřít menu na celou obrazovku. Kiosk ukončíš `Alt+F4`
(nebo `Ctrl+W`).

---

## ⏯️ Autostart úvodní obrazovky po zapnutí

Cíl: po startu Pi naskočí rovnou arkádové menu na celou obrazovku.

### A) Vypni zhasínání obrazovky

```bash
sudo raspi-config
```
→ **Display Options** → **Screen Blanking** → **No**. Pak restart.

### B) Přidej autostart (XDG `.desktop`)

Funguje na desktopovém prostředí Raspberry Pi OS (labwc / wayfire / LXDE):

```bash
mkdir -p ~/.config/autostart
cat > ~/.config/autostart/skopilot.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=SkoPilot Arcade
Exec=/home/pi/skopilot-games/start-arcade.sh
X-GNOME-Autostart-enabled=true
EOF
```

> ⚠️ Pokud tvůj uživatel **není** `pi`, uprav cestu v `Exec=`
> (např. `/home/ondra/skopilot-games/start-arcade.sh`). Zjistíš ho přes `whoami`.

Restartuj a hotovo:
```bash
sudo reboot
```

### B‑alternativa) LXDE autostart (starší Raspberry Pi OS)

Pokud verze A nefunguje, přidej řádek do LXDE autostartu:

```bash
echo "@/home/pi/skopilot-games/start-arcade.sh" >> ~/.config/lxsession/LXDE-pi/autostart
```

---

## 🎮 Propojení s ovládáním automatu Arcade1Up

Tlačítka a joystick automatu se k Raspberry Pi připojují přes **USB enkodér**
(např. Zero Delay / I‑PAC), který je hardwarově mapuje na klávesy. Hry počítají
s tímto výchozím rozložením:

| Prvek automatu | Klávesa | Funkce ve hrách |
|----------------|---------|------------------|
| Joystick ←/→/↑/↓ | šipky | pohyb / výběr v menu |
| Tlačítko 1 | Mezerník | skok / palba / start |
| Tlačítko 2 | Enter | potvrzení v menu |

> Enkodéry se obvykle dají přemapovat ve svém configu. Když máš joystick na
> jiných klávesách, hry rozumí i `WASD`.

---

## 🛠️ Tipy a řešení potíží

- **Černá obrazovka / menu nenaskočí:** zkontroluj cestu v `Exec=` v souboru
  `~/.config/autostart/skopilot.desktop` (musí sedět tvoje uživatelské jméno).
- **Obraz je menší než displej:** hry se škálují podle okna — v kiosku Chromia
  to sedí samo. Pokud testuješ v okně, roztáhni ho na celou plochu.
- **Obrazovka po chvíli zhasne:** dokonči krok A (Screen Blanking → No).
- **Aktualizace her:** `cd ~/skopilot-games && git pull`, pak restart.

---

Made with 💚 pro ŠkoPilot Edition.
