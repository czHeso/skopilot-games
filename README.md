# 🕹️ ŠkoPilot Family — Arcade

Sbírka mini‑her s maskotem **ŠkoPilot**, připravená pro arkádový automat
**Arcade1Up Pac‑Man 40th Anniversary** poháněný **Raspberry Pi 4**.

Vše je čisté HTML/JS/Canvas — žádný build, žádné závislosti. Stačí otevřít
`index.html` v prohlížeči.

Launcher (v angličtině, v barvách loga **ŠkoPilot Family**) je rozdělený do kategorií:

- **ŠkoPilot** — hratelné hry (níže v tabulce)
- **ŠkoPilot Clip** — *ClipTris* (Tetris) — zatím jen náhled „SOON"
- **ŠkoPilot Pro** — zatím „SOON"

## Co je uvnitř

| Cesta | Hra | Žánr | Ovládání |
|-------|-----|------|----------|
| `index.html` | **Arcade launcher** | retro výběr her | šipky + Enter / myš |
| `flappy/` | **Flappy ŠkoPilot** | letová obratnost | Mezerník / klik = skok |
| `invaders/` | **ŠkoInvaders** | vesmírná střílečka | ← → + Mezerník |
| `snake/` | **ŠkoSnake** | sbírání mincí | šipky / WASD |
| `breakout/` | **ŠkoBreakout** | arkanoid s pádlem | ← → + Mezerník |
| `skoman/` | **ŠkoMan** | bludiště à la Pac-Man | šipky / WASD |

Všechny hry:
- jdou **přes celou obrazovku** (automaticky se přizpůsobí poměru displeje),
- mají barvy maskota (smaragdová `#2fc56f`, tmavě zelené tělo `#0d4631`, bílá hlava),
- **ESC** kdykoliv vrátí do menu, vlevo nahoře je i tlačítko **◀ MENU**,
- ukládají nejlepší skóre do prohlížeče (`localStorage`).

## Avatar maskota

Úvodní obrazovka a hry používají obrázek maskota. Ulož celou postavičku jako:

```
assets/skopilot.png      (nejlépe PNG s průhledným pozadím, na výšku ~500–800 px)
```

Když soubor chybí, vše automaticky použije hlavu `invaders/skopilotHead.png`,
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
