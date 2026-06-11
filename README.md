# 🕹️ ŠkoPilot Family — Arcade

Sbírka mini‑her s maskotem **ŠkoPilot**, připravená pro arkádový automat
**Arcade1Up Pac‑Man 40th Anniversary** poháněný **Raspberry Pi 4**.

Vše je čisté HTML/JS/Canvas — žádný build, žádné závislosti. Stačí otevřít
`index.html` v prohlížeči.

> **Zajímavost:** Celý projekt běží offline a bez jakéhokoli frameworku.
> Jediné externě načítané assety jsou Google Fonty — ale i ty jsou přibalené
> lokálně, takže kiosk funguje i na místech bez internetu (továrna, výstava, sklad).

Launcher (v angličtině, v barvách loga **ŠkoPilot Family**) je rozdělený do kategorií:

- **ŠkoPilot** — *Flappy ŠkoPilot*
- **ŠkoPilot Clip** — *ŠkoPilot Clip* (Tetris na čas) — hratelné
- **ŠkoPilot Pro** — *Agent Builder* (plošinovka) — hratelné

---

## Co je uvnitř

| Cesta | Hra | Žánr | Ovládání |
|-------|-----|------|----------|
| `index.html` | **Arcade launcher** | retro výběr her | šipky + Enter / myš |
| `flappy/` | **Flappy ŠkoPilot** | letová obratnost | Mezerník / klik = skok |
| `clip/` | **ŠkoPilot Clip** | Tetris na čas | šipky = posun, **↑/X** = rotace, Mezerník = hard drop |
| `pro/` | **ŠkoPilot Pro** | plošinovka, stavba agenta | šipky/joystick + skok, **X** = výstřel |

---

### Flappy ŠkoPilot

Klasická Flappy Bird mechanika v kabátě ŠkoPilota. Pípnutí mezerníku (nebo klik)
drží maskota ve vzduchu — bez toho padá. Mezery mezi překážkami se náhodně generují,
takže každá hra je jiná. Skóre se uloží do `localStorage` a zobrazí se na úvodní
obrazovce hry.

> **Tip:** Na arkádovém automatu je nejpříjemnější hrát s tlačítkem 1 (Mezerník).
> Rychlé krátké tapnutí funguje lépe než držení.

---

### ŠkoPilot Clip — Tetris na čas

Dole je předem napsané slovo **CLIP** pixel grafikou — každá buňka jiný
**odstín žluté**. Cílem je nápis **co nejrychleji zničit**: padají tetromina a
když spojíš celý řádek, zmizí (klasicky jako Tetris) a s ním i kousek nápisu.

Občas (**fakt malá šance**) spadne místo dílu **bomba 💣**, která vybuchne a zničí
**3×3** kolem dopadu. V bočním panelu vidíš:

- **Náhled** dalšího padajícího dílu
- Aktuální **čas** (čím méně, tím líp)
- **Síň slávy nejrychlejších časů**

Ovládání je nahoře v souboru přehledně v `keydown` — snadno upravíš pro USB enkodér.

> **Zajímavost:** Pixel font nápisu CLIP je ručně nakódovaný jako 2D pole jedniček
> a nul přímo v JS — žádný sprite sheet, žádný obrázek. Každý pixel je jeden blok herního pole.

---

### ŠkoPilot Pro — Agent Builder

V mladoboleslavském cloudu spadly moduly. ŠkoPilot probíhá **3 levely** (IT&Dev →
Výroba → HR), sbírá ztracené **tooly** a na konci levelu z nich u „šasi"
sestaví **AI Agenta**.

Herní mechaniky:
- **Časový limit** — Server Timeout odpočítává, spěchej!
- **Bugy** — nepřátelé, kterým se musíš vyhnout nebo je zničit výstřelem
- **Power‑upy** — ☕ zrychlení pohybu, 🛡 firewall (dočasná nezranitelnost)
- **Síň slávy** — arkádové zadávání jména (až 5 znaků: A–Z, 1–9, `-`, `/`, mezera)

Technické detaily:
- Rozlišení **4:3 (800×600)**, automaticky škáluje na celou obrazovku
- Mapování kláves je nahoře v souboru v `KEYMAP` (Input Manager) — snadno upravíš pro USB enkodér
- Kolize jsou řešeny AABB (Axis‑Aligned Bounding Box) — rychlé, spolehlivé, bez závislostí

> **Zajímavost:** Celý herní engine (fyzika, render loop, správa entit, input) je
> napsaný v ~600 řádcích vanilla JS. Žádný Phaser, žádný PixiJS.

---

### Společné vlastnosti všech her

- Jdou **přes celou obrazovku** (automaticky se přizpůsobí poměru displeje)
- **ESC** kdykoliv vrátí do menu, vlevo nahoře je i tlačítko **◀ MENU**
- Ukládají nejlepší skóre do prohlížeče (`localStorage`)
- Fungují v **Chromiu bez internetu** — vhodné pro kiosky a výstavní stánky

#### Síň slávy (Hall of Fame)

- **Zadání jména:** až **5 znaků** (A–Z, číslice 1–9, pomlčka `-`, lomítko `/`
  a prázdná pozice). Funguje arkádově
  joystickem (↑/↓ mění znak, ←/→ posouvá pozici, Start/Enter potvrdí) **i psaním
  na klávesnici** (Backspace maže). Kratší jméno necháš tak, že zbylé pozice
  necháš prázdné.
- **Vstup do síně slávy:** na úvodní obrazovce každé hry zatlač **joystick
  nahoru (↑)**.
- **Mazání rekordů:** dole v síni slávy je položka **MANAGE RECORDS** —
  otevřeš ji tlačítkem **X** (nebo kliknutím myší). Uvnitř vybereš šipkami
  ↑/↓ (nebo najetím myší) **jednotlivý záznam** a smažeš ho přes X/Enter/klik
  s potvrzením **YES/NO**. K dispozici je i **DELETE ALL** (smaže celou
  tabulku včetně uloženého BEST skóre) a **BACK** pro návrat.

#### Obrázky s ovládáním (controls)

Každá hra si na úvodní obrazovce zobrazuje vlastní nápovědu ovládání:

| Hra | Soubor |
|-----|--------|
| Flappy ŠkoPilot | `assets/controls-1.png` |
| ŠkoPilot Clip | `assets/controls-2.png` |
| ŠkoPilot Pro | `assets/controls-3.png` |

Když soubor pro danou hru chybí, použije se společný `assets/controls.png`;
když chybí i ten, nápověda se prostě nezobrazí (nic nespadne).

---

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

> **Proč Chromium?** Chromium v kiosk módu (`--kiosk`) zakáže adresní lištu,
> kontextové menu i klávesové zkratky prohlížeče. Uživatel vidí jen hru —
> přesně jako u skutečného arkádového automatu.

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
| Tlačítko 3 | X | rotace (Clip) / výstřel (Pro) |

> **Jak funguje USB enkodér?** Raspberry Pi ho vidí jako standardní HID klávesnici.
> Není potřeba žádný ovladač — zapojíš, přiřadíš klávesy v enkodéru a hraješ.
> Zero Delay enkodéry mají odezvu pod 1 ms, takže latence je zcela zanedbatelná.

> Enkodéry se obvykle dají přemapovat ve svém configu. Když máš joystick na
> jiných klávesách, hry rozumí i `WASD`.

---

## 🛠️ Tipy a řešení potíží

- **Černá obrazovka / menu nenaskočí:** zkontroluj cestu v `Exec=` v souboru
  `~/.config/autostart/skopilot.desktop` (musí sedět tvoje uživatelské jméno).
- **Hra po startu naběhne v malém okně / pruhu místo celé obrazovky:**
  autostart spustil Chromium dřív, než kompozitor nastavil rozlišení displeje,
  nebo velikost okna neodpovídala logickému rozlišení (typicky u otočené
  obrazovky). `start-arcade.sh` proto počká na grafické prostředí (+4 s navíc)
  a zjistí **logické** rozlišení přes `xrandr`/`wlr-randr` (s fallbackem na
  DRM). Jak to debugnout:
  1. Po startu se podívej do logu: `tail ~/skopilot-arcade.log` — je tam typ
     session (X11/Wayland), jak dlouho skript čekal a jaké rozlišení zjistil.
  2. Porovnej s realitou: `DISPLAY=:0 xrandr --current | grep current`
     (přes SSH). Když nesedí, je obrazovka otočená/škálovaná v
     **Screen Configuration** — zjištěné rozlišení musí odpovídat tomu
     logickému.
  3. Spusť skript ručně z plochy (`~/skopilot-games/start-arcade.sh`) — když
     ručně funguje a po bootu ne, je to časování: zvyš ve skriptu `sleep 4`
     na `sleep 8`.
  4. Když je okno malé, zmáčkni `F11` — pokud se tím roztáhne, jde o geometrii
     okna (pošli obsah logu), pokud ne, problém je v kompozitoru.
- **V logu je každý řádek dvakrát / objeví se druhé malé okno:** skript se
  spouští **dvakrát** — bývá v autostartu na více místech najednou. Druhé
  Chromium se připojí k běžící instanci a otevře malé okno bez kiosku.
  Skript má od teď pojistku (`flock`), která druhý start zablokuje (v logu
  uvidíš „druhý start zablokován"). Duplicitní záznam najdeš a smažeš takto:
  ```bash
  grep -Rl skopilot ~/.config/autostart ~/.config/lxsession \
       ~/.config/labwc ~/.config/wayfire.ini 2>/dev/null
  ```
  Nech jen jeden (doporučeně `~/.config/autostart/skopilot.desktop`).
- **Okno pořád skáče do pruhu — co zkusit po krocích:**
  1. **Výchozí stav** (po aktualizaci): Chromium nyní běží vynuceně přes
     **X11 backend (XWayland)** — nativní Wayland backend Chromia má na
     otočených displejích chyby v geometrii fullscreenu. Restartuj a sleduj
     `tail ~/skopilot-arcade.log` (uvidíš `ozone=x11`).
  2. **Jiný prohlížeč — Firefox:**
     ```bash
     sudo apt install -y firefox
     SKOPILOT_BROWSER=firefox ~/skopilot-games/start-arcade.sh   # ruční test
     ```
     Když funguje, uprav autostart:
     `Exec=env SKOPILOT_BROWSER=firefox /home/pi/skopilot-games/start-arcade.sh`
  3. **Přepnout celý systém na X11** (nejjistější pro kiosky):
     `sudo raspi-config` → **Advanced Options** → **Wayland** → **X11**,
     restart. Pro arkádový automat to nemá žádné nevýhody.
  4. Návrat na Wayland backend Chromia (kdyby bylo potřeba):
     `SKOPILOT_OZONE=wayland ~/skopilot-games/start-arcade.sh`.
- **Highscory se po restartu mazaly:** kiosk dříve běžel s `--incognito`,
  takže `localStorage` (síň slávy, BEST) po vypnutí zmizel. Nyní má kiosk
  vlastní trvalý profil `~/.config/skopilot-kiosk` a rekordy přežijí restart.
- **Obraz je menší než displej:** hry se škálují podle okna — v kiosku Chromia
  to sedí samo. Pokud testuješ v okně, roztáhni ho na celou plochu.
- **Obrazovka po chvíli zhasne:** dokonči krok A (Screen Blanking → No).
- **Aktualizace her:** `cd ~/skopilot-games && git pull`, pak restart.
- **Hra sekne nebo padá:** zkus `chromium-browser --disable-gpu index.html` —
  na některých Pi 4 se GPU acceleration chová nestabilně pod Waylandem.
- **Kurzor myši je pořád vidět:** `unclutter` funguje jen pod X11 — na novějším
  Raspberry Pi OS (Bookworm, Wayland/labwc) kurzor neskryje. `start-arcade.sh`
  to řeší sám: vygeneruje průhledný kurzorový motiv `~/.icons/blank` a nastaví
  ho Chromiu (`XCURSOR_THEME=blank`), takže kurzor v kiosku není vidět vůbec.
  Pokud by i přesto byl vidět (např. kurzor kompozitoru ještě před startem
  Chromia), přidej řádek `XCURSOR_THEME=blank` do `~/.config/labwc/environment`
  a restartuj.

---

## 📐 Technická architektura (pro vývojáře)

```
skopilot-games/
├── index.html          # Arcade launcher (výběr her)
├── start-arcade.sh     # Spouštěcí skript pro Raspberry Pi (kiosk mód)
├── assets/
│   ├── fonts/          # Press Start 2P (offline pixel font)
│   ├── skopilot.png    # Maskot (celé tělo)
│   └── skopilotHead.png# Maskot (hlava, fallback)
├── flappy/
│   └── index.html      # Celá hra v jednom souboru (~300 řádků JS)
├── clip/
│   └── index.html      # Tetris na čas (~500 řádků JS)
└── pro/
    └── index.html      # Plošinovka Agent Builder (~600 řádků JS)
```

Každá hra je **jeden samostatný HTML soubor** — žádné importy, žádné moduly,
žádný bundler. To zaručuje, že hra poběží kdekoliv: v Chromiu, Firefoxu,
i na starém tabletu.

---

Made with 💚 pro ŠkoPilot Edition.
