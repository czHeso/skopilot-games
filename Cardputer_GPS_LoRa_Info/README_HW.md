# Cardputer GPS + LoRa Info — verze pro Cap LoRa-1262

Přepracovaná verze projektu *Cardputer GPS Info v1.1.0* (alcor55) pro sestavu:

| | |
|---|---|
| Host | **M5Stack Cardputer-Adv** (StampS3A / ESP32-S3FN8) |
| Modul | **M5Stack Cap LoRa-1262** |
| LoRa | Semtech **SX1262**, 868–923 MHz, SPI |
| GNSS | **AT6668 / ATGM336H**, UART, **115200 Bd** |
| Expandér | **PI4IOE5V6408** @ I²C 0x43 (RF anténní přepínač) |

> **Pozor:** Cap je určen **výhradně pro Cardputer-Adv**. Na starším Cardputeru
> 1.0/1.1 jsou GPIO 3, 4, 5, 6, 13 a 15 obsazené maticí klávesnice. Teprve ADV
> přesunul klávesnici na I²C řadič TCA8418, čímž se tyto piny uvolnily.

---

## 1. Mapování pinů

Modul **nekomunikuje přes Grove HY2.0-4P**, ale přes zadní **2×7 Cap-Bus**
konektor Cardputeru-Adv, který vede:

```
G3, G4, G6, G40, G14, G39, G5, 5V, GND, G8, G9, OUT, SDA, SCL, G13, G15
```

Grove konektor na Capu je průchozí pro uživatele — není to cesta k GPS ani
k LoRa čipu. Původní výchozí hodnoty `RX = G1`, `TX = G2` proto nikdy nemohly
přinést data.

### GNSS (AT6668)

| Signál | GPIO | Poznámka |
|---|---|---|
| Cardputer RX ← GPS TX | **15** | |
| Cardputer TX → GPS RX | **13** | |
| Baud rate | **115200** | tovární nastavení tohoto Capu |
| UART | UART1 | UART0 je USB-CDC konzole |

### LoRa (SX1262)

| Signál | GPIO |
|---|---|
| SCK | **40** |
| MISO | **39** |
| MOSI | **14** |
| NSS (CS) | **5** |
| DIO1 (IRQ) | **4** |
| RST | **3** |
| BUSY | **6** |

### Sdílená SPI sběrnice

SCK 40 / MISO 39 / MOSI 14 jsou **tytéž piny, které používá vestavěná microSD**
(CS = **12**). Obě zařízení proto musí jet přes jednu instanci `SPIClass`
s vlastními CS. `SPI.begin(40, 39, 14, -1)` se volá jednou v `setup()`, oba CS
se předtím nastaví na HIGH.

### I²C

`SDA = G8`, `SCL = G9` — vnitřní sběrnice ADV, vyvedená i na Cap-Bus.

---

## 2. Knihovny

### PlatformIO

Vše je v přiloženém `platformio.ini`:

```ini
lib_deps =
    m5stack/M5Cardputer@^1.0.3     ; táhne M5Unified + M5GFX
    mikalhart/TinyGPSPlus@^1.0.3
    jgromes/RadioLib@^6.6.0
platform = espressif32@^6.9.0
board    = m5stack-stamps3
```

### Arduino IDE

Board manager URL: `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
→ deska **M5Cardputer** (nebo *M5Stack-StampS3*), USB CDC On Boot = **Enabled**.

| Knihovna | Verze | Zdroj | K čemu |
|---|---|---|---|
| **M5Cardputer** | ≥ 1.0.3 | Library Manager (`M5Cardputer`) | displej, klávesnice, `M5Canvas` |
| **M5Unified** | ≥ 0.1.16 | závislost M5Cardputer | HAL |
| **M5GFX** | ≥ 0.1.16 | závislost M5Cardputer | LovyanGFX |
| **TinyGPSPlus** | 1.0.3 | Library Manager (`TinyGPSPlus` – Mikal Hart) | parsování NMEA |
| **RadioLib** | 6.6.0 (7.x je též OK) | Library Manager (`RadioLib` – Jan Gromeš) | driver SX1262 |
| `SD`, `SPI`, `Wire`, `FS` | — | součást ESP32 core | — |

Verze 6.6.0 RadioLib a 1.0.3 TinyGPSPlus jsou ověřená kombinace pro tento
hardware. Použité API (`begin()` s 9 parametry, `setDio1Action`,
`startTransmit(const char*)`) je stejné i v RadioLib 7.x.

---

## 3. Co se změnilo a proč

### Hardware

1. **GPS piny G1/G2 → G15/G13** a **9600 → 115200 Bd.** Bez toho program mlčí.
2. **Zvětšený RX buffer UARTu na 2048 B** (`setRxBufferSize()` **před**
   `begin()`). Při 115200 Bd přijde celá epocha (~1,5 kB) jako jeden shluk;
   výchozích 256 B přeteče, jakmile smyčku na chvíli zdrží překreslení nebo
   zápis na SD, a věty se tiše ztrácejí.
3. **Odstraněny všechny `delay(300)` z obsluhy kláves.** 300 ms při 115200 Bd
   znamená ~3 kB zahozených dat na jeden stisk. Nahrazeno hranovou detekcí přes
   `Keyboard.isChange()`, která debounce nepotřebuje. Také `delay(10)` ve
   `loop()` je zkrácený na `delay(1)`.
4. **Přidán driver SX1262** (RadioLib) — inicializace, příjem na přerušení,
   nebklokující vysílání, stránka se stavem rádia.
5. **Inicializace PI4IOE5V6408** — P0 se nastaví jako výstup, vypne se
   vysoká impedance a přepne na HIGH. Bez toho se SX1262 přes SPI ohlásí
   normálně, ale RF cesta vede naslepo — klasické „LoRa funguje na dva metry“.
   Bity se nastavují read-modify-write, aby se nerozsvítily ostatní výstupy.
6. **Automatický fallback TCXO → XTAL.** Když `radio.begin()` vrátí −706/−707,
   zkusí se ještě jednou s `tcxoVoltage = 0.0`, takže projdou obě varianty
   osazení.
7. **Sdílená SPI sběrnice s microSD** ošetřena jednou `SPI.begin()`
   a explicitními CS.

### Opravené chyby v původním kódu

| # | Chyba | Důsledek |
|---|---|---|
| 1 | `SD.begin()` bez parametrů | Použije výchozí SPI piny ESP32, ne Cardputeru (SCK 40 / MISO 39 / MOSI 14 / CS 12) → konfigurační soubor se **nikdy** nenačetl. Opraveno na `SD.begin(12, SPI, 20 MHz)`. |
| 2 | `SatData sat;` v `parseGSV()` nenastavovala `sat.visible` | Čtení neinicializovaného `bool` (UB) — nové družice měly náhodný stav viditelnosti. Členy struktury mají teď implicitní inicializaci. |
| 3 | `parseGSA()` porovnávala ID přes **všechny** konstelace | GPS PRN 5 a BeiDou PRN 5 se navzájem označily jako „used“. Nově se konstelace určí z talkeru (`$GPGSA`/`$BDGSA`…), z pole system ID (NMEA 4.10) nebo z rozsahu PRN. |
| 4 | Příznak `used` se nikdy nenuloval | Počet „Used“ jen monotónně rostl a přestal cokoli znamenat. Nuluje se na začátku každé epochy GSA. |
| 5 | `while (!Serial) {}` v `initDebugSerial()` | Na ESP32-S3 s nativním USB se zařízení po stisku `l`/`n` **natvrdo zaseklo**, dokud se nepřipojil terminál. Nahrazeno čekáním s timeoutem; USB-CDC se otevírá jednou v `setup()`. |
| 6 | Vektor `satellites` rostl bez omezení | Trvalý únik paměti a stále přibývající „duchové“ ve skyplotu. Přidán limit 96 položek a mazání po 5 minutách bez zmínky. |
| 7 | Funkce použité před deklarací | Fungovalo jen díky autoprototypům Arduino IDE; v PlatformIO se to nepřeložilo. Doplněny forward deklarace. |
| 8 | `setTextSize(0)` u popisků ve skyplotu | Nula není platný násobitel velikosti. Nahrazeno fontem `TomThumb` (3×5). |
| 9 | Nezávislé boolean příznaky `helpMenu` / `infoMenu` / `configsMenu` | Menu se dala otevřít přes sebe a stavy se rozešly; `configsMenu` navíc nenastavovalo `openMenu`, takže se do něj překreslovalo. Nahrazeno jedním `enum UiPage`. |
| 10 | V konfiguraci šipka nahoru i dolů dělaly `+1` | Šlo jen dopředu. `;` = nahoru, `.` = dolů. |
| 11 | Bez validace zadaných pinů | Šlo uložit GPIO 99 a poslat ho do `Serial.begin()`. Přidána kontrola rozsahu (0–48 mimo 26–32). |
| 12 | `speed`, `course`, `hdop` se tiskly bez `isValid()` | Bez fixu se zobrazovaly nuly, jako by to bylo měření. |
| 13 | Prohozená elevace/azimut pro BeiDou | Workaround pro jiný modul; AT6668 posílá standardní pořadí, takže to skyplot zrcadlilo. Odstraněno. |
| 14 | Tabulka NMEA handlerů vyjmenovávala kombinace talkerů | Chyběly `$GBGSV` (BeiDou dle NMEA 4.10) a `$GQGSV` (QZSS). Nově se porovnává jen typ věty (znaky 3–5). |
| 15 | `nmeaLine` rostla bez limitu | Při špatné baud rate se dekóduje šum a řetězec roste neomezeně. Limit 120 znaků. |
| 16 | `drawStatus()` volané ze `serialGPSRead()` | Kreslilo přes otevřené popupy. Vykreslování je teď na jednom místě. |

### Efektivita a struktura

- **Kreslení do `M5Canvas`** místo přímo na panel. Původní kód volal na každou
  buňku `fillRect()` + `print()` přímo do displeje, což viditelně blikalo.
  Nyní se skládá celý snímek do off-screen bufferu a jednou se pushne.
  Fallback: 16 bpp → 8 bpp → přímé kreslení.
- **Jedna funkce `render()`** řízená stavem `uiPage` místo kreslicích volání
  roztroušených po celém kódu.
- **Automatická detekce baud rate** (115200 → 9600 → 38400 → 57600). Zamkne se,
  jakmile dorazí věta se správným kontrolním součtem. Ruční nastavení baudu
  nebo hodnota z konfiguračního souboru ji vypne.
- Rozdělení NMEA vět do jedné pomocné funkce `splitNmea()`, pinout do
  `hw_config.h`.

---

## 4. Ovládání

| Klávesa | Funkce |
|---|---|
| `s` | zapnout/vypnout GPS UART |
| `t` | zapnout/vypnout LoRa rádio |
| `b` | odvysílat pozici (beacon) |
| `r` | stránka stavu LoRa |
| `c` | konfigurace |
| `h` / `i` | nápověda / info |
| `p` / `o` | ID / systém družice ve skyplotu |
| `l` | výpis družic na USB sériovou linku |
| `n` | výpis NMEA vět na USB sériovou linku (115200) |

V konfiguraci: `;` / `.` navigace, číslice zadání, `del` mazání, `ok` uložení
(na microSD do `/cpGpsLora.conf`), `c` odchod.

Frekvence LoRa se zadává celým číslem: `868` = 868 MHz, `868500` = 868,5 MHz
(desetinná tečka není k dispozici — `.` je na Cardputeru šipka dolů).

---

## 5. Co je potřeba ověřit na reálném kusu

Pinout, adresa expandéru i baud rate jsou z dokumentace M5Stack a z veřejného
referenčního projektu pro tuto sestavu, ne z měření na mém stole:

- **Napětí TCXO** (`LORA_TCXO_VOLTAGE` v `hw_config.h`, výchozí 1,8 V) — pokud
  by byla osazena jiná hodnota, kód si sám sáhne na variantu s krystalem, ale
  na stránce `[r]` se pak ukáže „(XTAL)“. To je signál hodnotu upravit.
- **I²C piny G8/G9** — pokud by expandér na tyto adrese neodpověděl, anténní
  přepínač se nezapne a `[r]` ukáže LoRa jako funkční, ale dosah bude mizivý.

Obojí je v `hw_config.h` jako jediné `#define`.
