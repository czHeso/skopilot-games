/*  Cardputer GPS + LoRa Info
 *
 *  Target : M5Stack Cardputer-Adv  +  M5Stack Cap LoRa-1262
 *           (SX1262 868-923 MHz, AT6668 / ATGM336H GNSS)
 *
 *  Based on "Cardputer GPS Info" v1.1.0 by alcor55, reworked for the Cap
 *  LoRa-1262 hardware and hardened for 115200 baud GNSS.
 *
 *  See hw_config.h for the pin map and README_HW.md for the change log.
 */

#include <M5Cardputer.h>
#include <TinyGPSPlus.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <FS.h>

#include <vector>
#include <algorithm>

#include "hw_config.h"

#define APP_VERSION  "2.0.0"
#define CONFIG_FILE  "/cpGpsLora.conf"

/* ================================================================== *
 *  Forward declarations
 *
 *  The Arduino IDE synthesises these automatically, PlatformIO and any
 *  plain C++ build do not. The original sketch relied on the former and
 *  would not compile under the latter.
 * ================================================================== */
struct SatData;
void  nmeaDispatcher(const String &line);
void  parseGSA(const String &line);
void  parseGSV(const String &line);
void  storeSatellite(const SatData &sat);
void  render();
void  saveConfig();
void  loadConfig();
bool  loraInit();
void  loraServiceIrq();

/* ================================================================== *
 *  Satellite bookkeeping
 * ================================================================== */

struct SatData {
  String   system   = "?";   /* "GPS", "GLONASS", "Galileo", "BeiDou", "QZSS" */
  int      id       = 0;
  int      elevation = 0;    /* 0..90 deg  */
  int      azimuth   = 0;    /* 0..359 deg */
  int      snr       = 0;    /* 0..99 dBHz */
  bool     used      = false;  /* used in the current fix */
  bool     visible   = false;  /* reported by the last complete GSV cycle */
  uint32_t lastSeen  = 0;      /* millis() of the last GSV mention */
};

/* Bound the list. 4 constellations x ~32 tracked channels is a realistic
 * worst case; the original vector grew without limit for the whole runtime. */
static const size_t   SAT_LIST_MAX    = 96;
static const uint32_t SAT_STALE_MS    = 300000UL;   /* forget after 5 min */

std::vector<SatData> satellites;

struct GSVSequenceState {
  String system;
  int    totalMsgs   = 0;
  int    lastMsgNum  = 0;
  std::vector<int> currentVisible;
};
static const int  GSV_STATE_MAX = 6;
GSVSequenceState  gsvStates[GSV_STATE_MAX];
int               gsvCount = 0;

/* ================================================================== *
 *  Global state
 * ================================================================== */

HardwareSerial GPS_Serial(GPS_UART_NUM);
TinyGPSPlus    gps;

/* One shared SPI bus: microSD (CS 12) + SX1262 (CS 5). */
SX1262 radio = new Module(LORA_NSS_PIN, LORA_DIO1_PIN, LORA_RST_PIN,
                          LORA_BUSY_PIN, SPI);

enum UiPage { PAGE_GPS, PAGE_LORA, PAGE_HELP, PAGE_INFO, PAGE_CONFIG };
UiPage uiPage = PAGE_GPS;

enum GPSState { GPS_OFF, GPS_ON, GPS_ERR };
GPSState gpsSerialState = GPS_OFF;

enum LoRaState { LORA_OFF, LORA_LISTEN, LORA_SENDING, LORA_ERROR };
LoRaState loraState = LORA_OFF;

bool sdAvailable = false;
bool sdErr       = false;

bool gpsSerial     = false;
bool nmeaSerial    = false;
bool satListSerial = false;
bool showPlotId    = false;
bool showPlotSystem = false;

/* Runtime configuration (persisted to microSD). */
int   gpsRxPin  = GPS_RX_PIN;
int   gpsTxPin  = GPS_TX_PIN;
int   gpsBaud   = GPS_BAUD_DEFAULT;
float loraFreq  = LORA_FREQ_DEFAULT;
int   loraSf    = LORA_SF_DEFAULT;
int   loraPower = LORA_POWER_DEFAULT;

/* GPS link supervision. */
static const uint32_t GPS_TIMEOUT_MS = 3000;   /* one epoch is 1 s; 3 s is a
                                                * real fault, not a hiccup */
uint32_t lastValidGpsMillis = 0;

/* Automatic baud probing - lets the sketch recover if the receiver was left
 * configured for 9600 by an earlier project. */
static const int GPS_BAUD_CANDIDATES[] = {115200, 9600, 38400, 57600};
static const int GPS_BAUD_CANDIDATE_N =
    sizeof(GPS_BAUD_CANDIDATES) / sizeof(GPS_BAUD_CANDIDATES[0]);
bool     gpsAutoBaud       = true;
bool     gpsBaudLocked     = false;
int      gpsBaudProbeIdx   = 0;
uint32_t gpsBaudProbeStart = 0;
static const uint32_t GPS_BAUD_PROBE_MS = 2500;

/* LoRa runtime. */
volatile bool loraIrqFlag = false;
int      loraLastState    = RADIOLIB_ERR_NONE;
String   loraLastRxText   = "";
float    loraLastRssi     = 0;
float    loraLastSnr      = 0;
uint32_t loraRxCount      = 0;
uint32_t loraTxCount      = 0;
uint32_t loraLastRxMillis = 0;
bool     loraTcxoFallback = false;

/* Config editor. */
int    configSel = 0;
String configTmp[6] = {"", "", "", "", "", ""};
static const int CONFIG_FIELDS = 6;

/* Off-screen frame buffer - removes the flicker of the original, which
 * fillRect()-ed and re-printed every cell straight onto the panel. */
M5Canvas   canvas(&M5Cardputer.Display);
LovyanGFX *gfx = nullptr;

/* ================================================================== *
 *  Small helpers
 * ================================================================== */

static bool isValidGpio(int pin) {
  /* ESP32-S3: 0..48, minus the SPI flash/PSRAM block 26..32. */
  if (pin < 0 || pin > 48) return false;
  if (pin >= 26 && pin <= 32) return false;
  return true;
}

/* NMEA PRN ranges, used to attribute satellites reported by the combined
 * $GNGSA/$GNGSV talkers to the right constellation. */
static String systemFromPrn(int prn) {
  if (prn >= 1   && prn <= 32)  return "GPS";
  if (prn >= 33  && prn <= 64)  return "SBAS";
  if (prn >= 65  && prn <= 96)  return "GLONASS";
  if (prn >= 193 && prn <= 199) return "QZSS";
  if (prn >= 201 && prn <= 237) return "BeiDou";
  if (prn >= 301 && prn <= 336) return "Galileo";
  return "?";
}

/* NMEA 4.10 GSA system ID field. */
static String systemFromNmeaId(int id) {
  switch (id) {
    case 1: return "GPS";
    case 2: return "GLONASS";
    case 3: return "Galileo";
    case 4: return "BeiDou";
    case 5: return "QZSS";
    default: return "";
  }
}

static void splitNmea(const String &line, std::vector<String> &out) {
  out.clear();
  int last = 0;
  for (int i = 0; i <= (int)line.length(); i++) {
    if (i == (int)line.length() || line[i] == ',' || line[i] == '*') {
      out.push_back(line.substring(last, i));
      last = i + 1;
    }
  }
}

/* ================================================================== *
 *  GNSS serial
 * ================================================================== */

void initGPSSerial(bool on) {
  if (on) {
    GPS_Serial.end();
    /* Must precede begin() or it is ignored. */
    GPS_Serial.setRxBufferSize(GPS_RX_BUFFER_SIZE);
    GPS_Serial.begin(gpsBaud, SERIAL_8N1, gpsRxPin, gpsTxPin);
    lastValidGpsMillis = millis();
    gpsBaudProbeStart  = millis();
    gpsSerialState     = GPS_ON;
  } else {
    GPS_Serial.end();
    gpsSerialState = GPS_OFF;
  }
}

/* Step to the next candidate baud rate when nothing parses. */
static void gpsProbeNextBaud() {
  gpsBaudProbeIdx = (gpsBaudProbeIdx + 1) % GPS_BAUD_CANDIDATE_N;
  gpsBaud = GPS_BAUD_CANDIDATES[gpsBaudProbeIdx];
  initGPSSerial(true);
}

void serialGPSRead() {
  static String nmeaLine;
  bool gotData = false;

  /* Drain the whole FIFO every pass. */
  while (GPS_Serial.available()) {
    char c = (char)GPS_Serial.read();
    gps.encode(c);
    gotData = true;

    if (c == '\n') {
      nmeaDispatcher(nmeaLine);
      nmeaLine = "";
    } else if (c != '\r') {
      /* Guard against a runaway line if the baud rate is wrong and we are
       * decoding noise - the original appended without any bound. */
      if (nmeaLine.length() < 120) nmeaLine += c;
      else nmeaLine = "";
    }
  }

  if (gotData) lastValidGpsMillis = millis();

  /* A sentence that passes its checksum is the only proof the baud rate is
   * right; raw bytes can be framing noise. */
  if (!gpsBaudLocked && gps.passedChecksum() > 0) {
    gpsBaudLocked = true;
    gpsAutoBaud   = false;
  }

  if (gpsSerialState != GPS_OFF) {
    if (millis() - lastValidGpsMillis > GPS_TIMEOUT_MS) {
      gpsSerialState = GPS_ERR;
    } else if (gotData) {
      gpsSerialState = GPS_ON;
    }
  }

  if (gpsAutoBaud && !gpsBaudLocked && gpsSerialState != GPS_OFF &&
      millis() - gpsBaudProbeStart > GPS_BAUD_PROBE_MS) {
    gpsProbeNextBaud();
  }
}

/* ================================================================== *
 *  NMEA parsing
 * ================================================================== */

void nmeaDispatcher(const String &rawLine) {
  String line = rawLine;
  line.trim();
  if (line.length() < 6 || line[0] != '$') return;

  if (nmeaSerial) Serial.println(line);

  /* Compare the sentence type only (chars 3..5), so every talker ID -
   * GP/GL/GA/GB/BD/GN/GQ - is handled without an entry per combination.
   * The original table missed $GBGSV (BeiDou per NMEA 4.10) and $GQGSV. */
  String type = line.substring(3, 6);
  if      (type == "GSV") parseGSV(line);
  else if (type == "GSA") parseGSA(line);
}

/*  GSA - DOP and active satellites.
 *
 *  Two real bugs from the original are fixed here:
 *    1. "used" was never cleared, so the count only ever grew.
 *    2. IDs were matched across every constellation, so GPS PRN 5 and
 *       BeiDou PRN 5 marked each other as used.
 */
void parseGSA(const String &line) {
  static uint32_t lastGsaBurst = 0;

  /* GSA sentences arrive as one burst per epoch, one per constellation.
   * A gap means a new epoch: drop the previous fix membership. */
  if (millis() - lastGsaBurst > 500) {
    for (auto &s : satellites) s.used = false;
  }
  lastGsaBurst = millis();

  std::vector<String> f;
  splitNmea(line, f);
  if (f.size() < 18) return;

  /* Talker-derived constellation, e.g. $GPGSA -> GPS. */
  String talker = line.substring(1, 3);
  String sys    = "";
  if      (talker == "GP") sys = "GPS";
  else if (talker == "GL") sys = "GLONASS";
  else if (talker == "GA") sys = "Galileo";
  else if (talker == "GB" || talker == "BD") sys = "BeiDou";
  else if (talker == "GQ") sys = "QZSS";

  /* NMEA 4.10 appends a system ID after VDOP; prefer it when present. */
  if (sys.isEmpty() && f.size() >= 20) sys = systemFromNmeaId(f[18].toInt());

  for (int i = 3; i <= 14; i++) {
    if (f[i].length() == 0) continue;
    int id = f[i].toInt();
    if (id == 0) continue;

    String want = sys;
    if (want.isEmpty()) want = systemFromPrn(id);   /* $GNGSA fallback */

    for (auto &sat : satellites) {
      if (sat.id != id) continue;
      if (!want.isEmpty() && want != "?" && sat.system != want) continue;
      sat.used = true;
    }
  }
}

/*  GSV - satellites in view. */
void parseGSV(const String &line) {
  String talker = line.substring(1, 3);
  String system;
  if      (talker == "GP") system = "GPS";
  else if (talker == "GL") system = "GLONASS";
  else if (talker == "GA") system = "Galileo";
  else if (talker == "GB" || talker == "BD") system = "BeiDou";
  else if (talker == "GQ") system = "QZSS";
  else if (talker == "GN") system = "";   /* resolved per satellite by PRN */
  else return;

  std::vector<String> f;
  splitNmea(line, f);
  if (f.size() < 4) return;

  int totalMsgs = f[1].toInt();
  int msgNum    = f[2].toInt();
  if (totalMsgs <= 0 || msgNum <= 0) return;

  /* One sequence state per talker; $GNGSV shares a single "GN" slot. */
  String stateKey = system.isEmpty() ? String("GN") : system;
  GSVSequenceState *state = nullptr;
  for (int i = 0; i < gsvCount; i++) {
    if (gsvStates[i].system == stateKey) { state = &gsvStates[i]; break; }
  }
  if (!state && gsvCount < GSV_STATE_MAX) {
    gsvStates[gsvCount].system = stateKey;
    state = &gsvStates[gsvCount++];
  }
  if (!state) return;

  if (msgNum == 1 || state->totalMsgs != totalMsgs) {
    state->currentVisible.clear();
    state->totalMsgs = totalMsgs;
  }

  for (size_t i = 4; i + 3 < f.size(); i += 4) {
    if (f[i].length() == 0) continue;

    SatData sat;                       /* members are default-initialised;
                                        * the original left `visible`
                                        * uninitialised before push_back */
    sat.id = f[i].toInt();
    if (sat.id == 0) continue;

    sat.system = system.isEmpty() ? systemFromPrn(sat.id) : system;

    /* The original swapped elevation/azimuth for BeiDou. That was a
     * workaround for one non-conforming module; the AT6668 emits standard
     * field order and the swap mirrors the sky plot. Removed. */
    sat.elevation = f[i + 1].toInt();
    sat.azimuth   = f[i + 2].toInt();
    sat.snr       = f[i + 3].toInt();
    sat.used      = false;
    sat.lastSeen  = millis();

    storeSatellite(sat);
    state->currentVisible.push_back(sat.id);
  }

  state->lastMsgNum = msgNum;

  if (msgNum == totalMsgs) {
    for (auto &s : satellites) {
      bool belongs = system.isEmpty() ? true : (s.system == system);
      if (!belongs) continue;
      s.visible = std::find(state->currentVisible.begin(),
                            state->currentVisible.end(),
                            s.id) != state->currentVisible.end();
    }
  }
}

void storeSatellite(const SatData &sat) {
  for (auto &s : satellites) {
    if (s.system == sat.system && s.id == sat.id) {
      s.elevation = sat.elevation;
      s.azimuth   = sat.azimuth;
      s.snr       = sat.snr;
      s.lastSeen  = sat.lastSeen;
      return;
    }
  }
  if (satellites.size() >= SAT_LIST_MAX) return;
  satellites.push_back(sat);
}

/* Drop satellites that have not been mentioned for a long time, so the sky
 * plot does not accumulate red ghosts at stale positions forever. */
void pruneSatellites() {
  uint32_t now = millis();
  satellites.erase(
      std::remove_if(satellites.begin(), satellites.end(),
                     [now](const SatData &s) {
                       return (now - s.lastSeen) > SAT_STALE_MS;
                     }),
      satellites.end());
}

/* ================================================================== *
 *  USB serial console
 * ================================================================== */

/* USB-CDC is opened once in setup() and simply gated by flags afterwards.
 * The original did Serial.begin()/Serial.end() per keypress and spun in
 * `while (!Serial) {}` - which hangs the device forever when no terminal
 * is attached to the ESP32-S3 native USB port. */
void initDebugSerial() {
  Serial.begin(115200);
  Serial.setTimeout(2000);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 1500) delay(10);   /* bounded wait */
  Serial.println("\nCardputer GPS+LoRa Info " APP_VERSION);
}

void serialConsoleSatsList() {
  if (!satListSerial) return;
  static uint32_t lastPrint = 0;
  if (millis() - lastPrint < 5000) return;
  lastPrint = millis();
  if (satellites.empty()) return;

  std::sort(satellites.begin(), satellites.end(),
            [](const SatData &a, const SatData &b) {
              if (a.system != b.system) return a.system < b.system;
              return a.id < b.id;
            });

  Serial.println("------------------------------------");
  Serial.println("System    ID  Ele  Azi  SNR  Usd Vis");
  Serial.println("------------------------------------");
  for (auto &s : satellites) {
    Serial.printf("%-8s %3d %4d %4d %4d   %c   %c\n", s.system.c_str(), s.id,
                  s.elevation, s.azimuth, s.snr, s.used ? 'Y' : 'N',
                  s.visible ? 'V' : 'X');
  }
  Serial.println("------------------------------------");
}

/* ================================================================== *
 *  PI4IOE5V6408 port expander (RF antenna switch on the Cap)
 * ================================================================== */

/*  M5Unified already owns the internal I2C bus on the Cardputer-Adv (the
 *  keyboard is a TCA8418 on that same bus). Re-running Wire.begin()
 *  unconditionally can reset it under the keyboard driver, so probe first and
 *  only initialise the bus ourselves if nothing answers. */
static bool i2cEnsure() {
  static bool begun = false;
  Wire.beginTransmission(PI4IOE_ADDR);
  if (Wire.endTransmission() == 0) return true;
  if (begun) return false;
  begun = true;
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
  Wire.beginTransmission(PI4IOE_ADDR);
  return Wire.endTransmission() == 0;
}

static bool pi4ioeRead(uint8_t reg, uint8_t &val) {
  Wire.beginTransmission(PI4IOE_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)PI4IOE_ADDR, (int)1) != 1) return false;
  val = (uint8_t)Wire.read();
  return true;
}

static bool pi4ioeWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(PI4IOE_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

/* Read-modify-write a single bit. Writing whole bytes here would drive every
 * other expander output, which on this Cap is not harmless. */
static bool pi4ioeSetBit(uint8_t reg, uint8_t bit, bool level) {
  uint8_t v;
  if (!pi4ioeRead(reg, v)) return false;
  uint8_t nv = level ? (uint8_t)(v | (1 << bit)) : (uint8_t)(v & ~(1 << bit));
  if (nv == v) return true;
  return pi4ioeWrite(reg, nv);
}

bool antennaSwitchEnable() {
  if (!i2cEnsure()) return false;
  bool ok = true;
  ok &= pi4ioeSetBit(PI4IOE_REG_DIRECTION, PI4IOE_PIN_ANT_SW, true);  /* out */
  ok &= pi4ioeSetBit(PI4IOE_REG_HIGH_Z,    PI4IOE_PIN_ANT_SW, false); /* drive */
  ok &= pi4ioeSetBit(PI4IOE_REG_PULL_EN,   PI4IOE_PIN_ANT_SW, false);
  ok &= pi4ioeSetBit(PI4IOE_REG_OUTPUT,    PI4IOE_PIN_ANT_SW, true);  /* high */
  return ok;
}

/* ================================================================== *
 *  LoRa - SX1262 via RadioLib
 * ================================================================== */

void IRAM_ATTR onLoraDio1() { loraIrqFlag = true; }

bool loraInit() {
  /* Both SPI devices idle-high before anything touches the bus. */
  pinMode(LORA_NSS_PIN, OUTPUT); digitalWrite(LORA_NSS_PIN, HIGH);
  pinMode(SD_CS_PIN,    OUTPUT); digitalWrite(SD_CS_PIN,    HIGH);

  antennaSwitchEnable();

  loraLastState = radio.begin(loraFreq, LORA_BW_DEFAULT, (uint8_t)loraSf,
                              LORA_CR_DEFAULT, LORA_SYNCWORD,
                              (int8_t)loraPower, LORA_PREAMBLE_LEN,
                              LORA_TCXO_VOLTAGE, LORA_USE_LDO);

  /* -706/-707 mean the chip never left standby: almost always a TCXO vs XTAL
   * mismatch. Retry once assuming a plain crystal. */
  if (loraLastState == RADIOLIB_ERR_SPI_CMD_TIMEOUT ||
      loraLastState == RADIOLIB_ERR_SPI_CMD_FAILED) {
    loraTcxoFallback = true;
    loraLastState = radio.begin(loraFreq, LORA_BW_DEFAULT, (uint8_t)loraSf,
                                LORA_CR_DEFAULT, LORA_SYNCWORD,
                                (int8_t)loraPower, LORA_PREAMBLE_LEN,
                                0.0f, LORA_USE_LDO);
  }

  if (loraLastState != RADIOLIB_ERR_NONE) {
    loraState = LORA_ERROR;
    return false;
  }

  radio.setDio2AsRfSwitch(true);
  radio.setCurrentLimit(LORA_CURRENT_LIMIT);
  radio.setDio1Action(onLoraDio1);

  loraLastState = radio.startReceive();
  loraState = (loraLastState == RADIOLIB_ERR_NONE) ? LORA_LISTEN : LORA_ERROR;
  return loraState == LORA_LISTEN;
}

void loraStop() {
  radio.clearDio1Action();
  radio.sleep();
  loraState = LORA_OFF;
}

/* Non-blocking transmit: startTransmit() returns immediately and the DIO1
 * interrupt tells us when the packet is out. radio.transmit() would block for
 * the whole air time (up to ~1 s at SF12) and overflow the GNSS RX buffer. */
bool loraSend(const String &msg) {
  if (loraState != LORA_LISTEN) return false;
  /* c_str(): RadioLib's String overload takes a non-const reference and will
   * not bind to a temporary. */
  loraLastState = radio.startTransmit(msg.c_str());
  if (loraLastState != RADIOLIB_ERR_NONE) return false;
  loraState = LORA_SENDING;
  return true;
}

String loraBuildBeacon() {
  char buf[64];
  if (gps.location.isValid()) {
    snprintf(buf, sizeof(buf), "CP,%.5f,%.5f,%d,%d", gps.location.lat(),
             gps.location.lng(),
             gps.altitude.isValid() ? (int)gps.altitude.meters() : 0,
             gps.satellites.isValid() ? (int)gps.satellites.value() : 0);
  } else {
    snprintf(buf, sizeof(buf), "CP,nofix");
  }
  return String(buf);
}

void loraServiceIrq() {
  if (!loraIrqFlag) return;
  loraIrqFlag = false;

  if (loraState == LORA_SENDING) {
    radio.finishTransmit();
    loraTxCount++;
    loraLastState = radio.startReceive();
    loraState = (loraLastState == RADIOLIB_ERR_NONE) ? LORA_LISTEN : LORA_ERROR;
    return;
  }

  if (loraState == LORA_LISTEN) {
    String data;
    int st = radio.readData(data);
    if (st == RADIOLIB_ERR_NONE) {
      loraLastRxText   = data;
      loraLastRssi     = radio.getRSSI();
      loraLastSnr      = radio.getSNR();
      loraLastRxMillis = millis();
      loraRxCount++;
      if (nmeaSerial || satListSerial) {
        Serial.printf("[LoRa RX %.1f dBm] %s\n", loraLastRssi, data.c_str());
      }
    }
    radio.startReceive();
  }
}

/* ================================================================== *
 *  microSD configuration
 * ================================================================== */

/*  The original called a bare SD.begin(), which uses the ESP32 default SPI
 *  pins - not the Cardputer's SCK 40 / MISO 39 / MOSI 14 / CS 12. The config
 *  file therefore never loaded on real hardware. */
bool sdInit() {
  if (!SD.begin(SD_CS_PIN, SPI, 20000000)) return false;
  return true;
}

void saveConfig() {
  if (!sdAvailable) return;
  File f = SD.open(CONFIG_FILE, FILE_WRITE);
  if (!f) { sdErr = true; return; }
  sdErr = false;
  f.printf("gpsRxPin=%d\n",  gpsRxPin);
  f.printf("gpsTxPin=%d\n",  gpsTxPin);
  f.printf("gpsBaud=%d\n",   gpsBaud);
  f.printf("loraFreq=%.3f\n", loraFreq);
  f.printf("loraSf=%d\n",    loraSf);
  f.printf("loraPower=%d\n", loraPower);
  f.close();
}

void loadConfig() {
  if (!sdAvailable) return;
  if (!SD.exists(CONFIG_FILE)) { saveConfig(); return; }
  File f = SD.open(CONFIG_FILE);
  if (!f) { sdErr = true; return; }
  sdErr = false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String k = line.substring(0, eq);
    String v = line.substring(eq + 1);

    if      (k == "gpsRxPin"  && isValidGpio(v.toInt())) gpsRxPin  = v.toInt();
    else if (k == "gpsTxPin"  && isValidGpio(v.toInt())) gpsTxPin  = v.toInt();
    else if (k == "gpsBaud"   && v.toInt() >= 4800)      gpsBaud   = v.toInt();
    else if (k == "loraFreq") {
      float fq = v.toFloat();
      if (fq >= 150.0f && fq <= 960.0f) loraFreq = fq;
    } else if (k == "loraSf") {
      int sf = v.toInt();
      if (sf >= 5 && sf <= 12) loraSf = sf;
    } else if (k == "loraPower") {
      int p = v.toInt();
      if (p >= -9 && p <= 22) loraPower = p;
    }
  }
  f.close();

  /* A saved baud rate is an explicit choice - do not probe over it. */
  gpsAutoBaud = false;
}

/* ================================================================== *
 *  Rendering
 * ================================================================== */

static const char *gpsStateStr() {
  switch (gpsSerialState) {
    case GPS_ON:  return "On";
    case GPS_ERR: return "Err";
    default:      return "Off";
  }
}

static const char *loraStateStr() {
  switch (loraState) {
    case LORA_LISTEN:  return "RX";
    case LORA_SENDING: return "TX";
    case LORA_ERROR:   return "Err";
    default:           return "Off";
  }
}

void drawHeader() {
  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  gfx->fillRect(0, 0, SCREEN_W, 13, TFT_GREEN);
  gfx->setTextColor(TFT_BLACK, TFT_GREEN);
  gfx->setCursor(4, 3);
  gfx->print("  -= Cardputer GPS + LoRa Info =-");

  gfx->drawRect(0, 12, SCREEN_W, 13, TFT_GREEN);
  gfx->setTextColor(TFT_GREEN, TFT_BLACK);
  gfx->setCursor(4, 16);
  gfx->print("[s]GPS [t]LoRa [b]Beacon [h]Help");
}

void drawStatus() {
  char line[80];
  snprintf(line, sizeof(line), "GP:%s %d%s | LoRa:%s %lu/%lu | SD:%s",
           gpsStateStr(), gpsBaud, (gpsAutoBaud && !gpsBaudLocked) ? "?" : "",
           loraStateStr(), (unsigned long)loraTxCount,
           (unsigned long)loraRxCount,
           sdAvailable ? (sdErr ? "E" : "Y") : "N");

  gfx->fillRect(0, SCREEN_H - 13, SCREEN_W, 13, TFT_DARKGREY);
  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  gfx->setTextColor(TFT_WHITE, TFT_DARKGREY);
  gfx->setCursor(4, SCREEN_H - 10);
  gfx->print(line);
}

void drawSatelliteDataTab() {
  const int y0 = 26;

  const char *c1labels[] = {"Lat", "Lng", "Alt", "Spd",
                            "Crs", "Date", "Time", "HDOP"};
  char c1[8][20];

  /* Every value is validity-checked. The original printed speed, course and
   * HDOP unconditionally, showing 0.0 as if it were a measurement. */
  if (gps.location.isValid()) {
    snprintf(c1[0], sizeof(c1[0]), "%.6f", gps.location.lat());
    snprintf(c1[1], sizeof(c1[1]), "%.6f", gps.location.lng());
  } else {
    snprintf(c1[0], sizeof(c1[0]), "NoFix");
    snprintf(c1[1], sizeof(c1[1]), "NoFix");
  }
  if (gps.altitude.isValid()) snprintf(c1[2], sizeof(c1[2]), "%.1f", gps.altitude.meters());
  else                        snprintf(c1[2], sizeof(c1[2]), "--");
  if (gps.speed.isValid())    snprintf(c1[3], sizeof(c1[3]), "%.1f", gps.speed.kmph());
  else                        snprintf(c1[3], sizeof(c1[3]), "--");
  if (gps.course.isValid())   snprintf(c1[4], sizeof(c1[4]), "%.1f", gps.course.deg());
  else                        snprintf(c1[4], sizeof(c1[4]), "--");
  if (gps.date.isValid())     snprintf(c1[5], sizeof(c1[5]), "%02d/%02d/%02d",
                                       gps.date.day(), gps.date.month(),
                                       gps.date.year() % 100);
  else                        snprintf(c1[5], sizeof(c1[5]), "--");
  if (gps.time.isValid())     snprintf(c1[6], sizeof(c1[6]), "%02d:%02d:%02d",
                                       gps.time.hour(), gps.time.minute(),
                                       gps.time.second());
  else                        snprintf(c1[6], sizeof(c1[6]), "--");
  if (gps.hdop.isValid())     snprintf(c1[7], sizeof(c1[7]), "%.1f", gps.hdop.hdop());
  else                        snprintf(c1[7], sizeof(c1[7]), "--");

  int seen = (int)satellites.size(), used = 0, visible = 0;
  int nGps = 0, nGln = 0, nGal = 0, nBds = 0;
  for (auto &s : satellites) {
    if (s.used) used++;
    if (!s.visible) continue;
    visible++;
    if      (s.system == "GPS")     nGps++;
    else if (s.system == "GLONASS") nGln++;
    else if (s.system == "Galileo") nGal++;
    else if (s.system == "BeiDou")  nBds++;
  }

  const char *c2labels[] = {"Seen", "Visb", "Used", "InFx",
                            "GPS", "Gln", "Gal", "BDo"};
  char c2[8][12];
  snprintf(c2[0], sizeof(c2[0]), "%d", seen);
  snprintf(c2[1], sizeof(c2[1]), "%d", visible);
  snprintf(c2[2], sizeof(c2[2]), "%d", used);
  snprintf(c2[3], sizeof(c2[3]), "%d",
           gps.satellites.isValid() ? (int)gps.satellites.value() : 0);
  snprintf(c2[4], sizeof(c2[4]), "%d", nGps);
  snprintf(c2[5], sizeof(c2[5]), "%d", nGln);
  snprintf(c2[6], sizeof(c2[6]), "%d", nGal);
  snprintf(c2[7], sizeof(c2[7]), "%d", nBds);

  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);

  int y = y0;
  for (int i = 0; i < 8; i++) {
    gfx->drawRect(1, y, 90, 13, TFT_DARKGREY);
    gfx->setTextColor(TFT_WHITE, TFT_BLACK);
    gfx->setCursor(5, y + 3);
    gfx->printf("%s: %s", c1labels[i], c1[i]);
    y += 12;
  }

  y = y0;
  for (int i = 0; i < 8; i++) {
    gfx->drawRect(90, y, 53, 13, TFT_DARKGREY);
    gfx->setTextColor(TFT_WHITE, TFT_BLACK);
    gfx->setCursor(94, y + 3);
    gfx->printf("%s: %s", c2labels[i], c2[i]);
    y += 12;
  }
}

void drawSkyPlot() {
  const int x = 143, y = 27, w = 96, h = 95;
  const int r  = h / 2;
  const int cx = x + r, cy = y + r;

  gfx->drawRect(x - 1, y - 1, w + 2, h + 2, TFT_DARKGREY);
  gfx->drawCircle(cx, cy, r, TFT_WHITE);
  gfx->drawCircle(cx, cy, (int)(r * 0.66f), TFT_DARKGREY);
  gfx->drawCircle(cx, cy, (int)(r * 0.33f), TFT_DARKGREY);
  gfx->drawLine(cx - r, cy, cx + r, cy, TFT_DARKGREY);
  gfx->drawLine(cx, cy - r, cx, cy + r, TFT_DARKGREY);

  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  gfx->setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  gfx->setCursor(cx - 3, cy - r + 3);  gfx->print("N");
  gfx->setCursor(cx - 3, cy + r - 10); gfx->print("S");
  gfx->setCursor(cx + r - 9, cy - 3);  gfx->print("E");
  gfx->setCursor(cx - r + 3, cy - 3);  gfx->print("W");

  for (auto &s : satellites) {
    if (!s.visible && !s.used) continue;   /* no ghosts of lost satellites */

    float elev  = constrain((float)s.elevation, 0.0f, 90.0f);
    float az    = fmodf((float)s.azimuth + 360.0f, 360.0f);
    float rad   = (90.0f - elev) / 90.0f * (float)r;
    float radAz = radians(az);
    int   sx    = cx + (int)(rad * sinf(radAz));
    int   sy    = cy - (int)(rad * cosf(radAz));

    uint16_t color = TFT_RED;
    if (s.used)         color = TFT_GREEN;
    else if (s.visible) color = (s.snr >= 25) ? TFT_YELLOW : TFT_ORANGE;

    gfx->fillCircle(sx, sy, 2, color);

    if (showPlotId || showPlotSystem) {
      char label[10];
      if (showPlotId && showPlotSystem) {
        const char *sys = "?";
        if      (s.system == "GPS")     sys = "G";
        else if (s.system == "GLONASS") sys = "R";
        else if (s.system == "Galileo") sys = "E";
        else if (s.system == "BeiDou")  sys = "B";
        else if (s.system == "QZSS")    sys = "Q";
        snprintf(label, sizeof(label), "%s%d", sys, s.id);
      } else if (showPlotId) {
        snprintf(label, sizeof(label), "%d", s.id);
      } else {
        const char *sys = "?";
        if      (s.system == "GPS")     sys = "Gp";
        else if (s.system == "GLONASS") sys = "Gl";
        else if (s.system == "Galileo") sys = "Ga";
        else if (s.system == "BeiDou")  sys = "Bd";
        else if (s.system == "QZSS")    sys = "Qz";
        snprintf(label, sizeof(label), "%s", sys);
      }
      /* TomThumb is a 3x5 font. The original used setTextSize(0), which is
       * not a valid scale factor. */
      gfx->setFont(&fonts::TomThumb);
      gfx->setTextColor(color, TFT_BLACK);
      gfx->setCursor(sx + 4, sy - 2);
      gfx->print(label);
      gfx->setFont(&fonts::Font0);
    }
  }
}

void drawLoraPage() {
  gfx->fillRect(8, 26, SCREEN_W - 16, SCREEN_H - 42, TFT_BLACK);
  gfx->drawRect(8, 26, SCREEN_W - 16, SCREEN_H - 42, TFT_GREEN);
  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  gfx->setTextColor(TFT_WHITE, TFT_BLACK);

  int y = 31;
  gfx->setCursor(14, y); gfx->printf("SX1262  %s%s", loraStateStr(),
                                     loraTcxoFallback ? " (XTAL)" : "");
  y += 10;
  gfx->setCursor(14, y); gfx->printf("%.3f MHz  SF%d  %d dBm",
                                     loraFreq, loraSf, loraPower);
  y += 10;
  if (loraState == LORA_ERROR) {
    gfx->setTextColor(TFT_RED, TFT_BLACK);
    gfx->setCursor(14, y); gfx->printf("RadioLib error %d", loraLastState);
    gfx->setTextColor(TFT_WHITE, TFT_BLACK);
    y += 10;
  }
  gfx->setCursor(14, y); gfx->printf("TX:%lu  RX:%lu", loraTxCount, loraRxCount);
  y += 12;

  gfx->setTextColor(TFT_GREEN, TFT_BLACK);
  gfx->setCursor(14, y); gfx->print("Last RX:");
  y += 10;
  gfx->setTextColor(TFT_WHITE, TFT_BLACK);
  if (loraRxCount == 0) {
    gfx->setCursor(14, y); gfx->print("(nothing yet)");
  } else {
    gfx->setCursor(14, y);
    gfx->print(loraLastRxText.substring(0, 34));
    y += 10;
    gfx->setCursor(14, y);
    gfx->printf("RSSI %.1f dBm  SNR %.1f dB  %lus ago", loraLastRssi,
                loraLastSnr, (millis() - loraLastRxMillis) / 1000UL);
  }
}

void drawHelpPage() {
  const char *txt[] = {
    "[s] GPS serial start/stop",
    "[t] LoRa radio on/off",
    "[b] Broadcast position beacon",
    "[r] LoRa status page",
    "[c] Config   [i] Info   [h] Help",
    "[p] Sat ID on sky plot",
    "[o] Sat system on sky plot",
    "[l] Sat list -> USB serial",
    "[n] NMEA -> USB serial (115200)",
  };
  gfx->fillRect(6, 16, SCREEN_W - 12, SCREEN_H - 22, TFT_BLACK);
  gfx->drawRect(6, 16, SCREEN_W - 12, SCREEN_H - 22, TFT_GREEN);
  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  int y = 21;
  for (auto t : txt) {
    String line = t;
    int end = line.indexOf(']') + 1;
    gfx->setCursor(12, y);
    if (end > 0) {
      gfx->setTextColor(TFT_GREEN, TFT_BLACK);
      gfx->print(line.substring(0, end));
      gfx->setTextColor(TFT_WHITE, TFT_BLACK);
      gfx->print(line.substring(end));
    } else {
      gfx->setTextColor(TFT_WHITE, TFT_BLACK);
      gfx->print(line);
    }
    y += 12;
  }
}

void drawInfoPage() {
  gfx->fillRect(14, 22, SCREEN_W - 28, SCREEN_H - 34, TFT_BLACK);
  gfx->drawRect(14, 22, SCREEN_W - 28, SCREEN_H - 34, TFT_GREEN);
  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  gfx->setTextColor(TFT_WHITE, TFT_BLACK);
  int y = 28;
  gfx->setCursor(20, y); gfx->print("Cardputer GPS + LoRa Info " APP_VERSION); y += 11;
  gfx->setCursor(20, y); gfx->print("Cardputer-Adv + Cap LoRa-1262");      y += 11;
  gfx->setCursor(20, y); gfx->printf("GPS  RX:%d TX:%d @%d", gpsRxPin, gpsTxPin, gpsBaud); y += 11;
  gfx->setCursor(20, y); gfx->printf("LoRa NSS:%d BUSY:%d DIO1:%d",
                                     LORA_NSS_PIN, LORA_BUSY_PIN, LORA_DIO1_PIN); y += 11;
  gfx->setCursor(20, y); gfx->printf("NMEA ok:%lu bad:%lu chars:%lu",
                                     (unsigned long)gps.passedChecksum(),
                                     (unsigned long)gps.failedChecksum(),
                                     (unsigned long)gps.charsProcessed());     y += 11;
  gfx->setCursor(20, y); gfx->printf("Free heap: %u B", (unsigned)ESP.getFreeHeap());
}

void drawConfigPage() {
  const char *labels[CONFIG_FIELDS] = {"GPS RX pin", "GPS TX pin", "GPS baud",
                                       "LoRa MHz/kHz", "LoRa SF",  "LoRa dBm"};
  char actual[CONFIG_FIELDS][12];
  snprintf(actual[0], 12, "%d",   gpsRxPin);
  snprintf(actual[1], 12, "%d",   gpsTxPin);
  snprintf(actual[2], 12, "%d",   gpsBaud);
  snprintf(actual[3], 12, "%.1f", loraFreq);
  snprintf(actual[4], 12, "%d",   loraSf);
  snprintf(actual[5], 12, "%d",   loraPower);

  gfx->fillRect(4, 14, SCREEN_W - 8, SCREEN_H - 20, TFT_BLACK);
  gfx->drawRect(4, 14, SCREEN_W - 8, SCREEN_H - 20, TFT_GREEN);
  gfx->setFont(&fonts::Font0);
  gfx->setTextSize(1);
  gfx->setTextColor(TFT_WHITE, TFT_BLACK);
  gfx->setCursor(10, 18);
  gfx->printf("Config (SD:%s%s)  [;/.]nav [c]exit",
              sdAvailable ? "Y" : "N", sdErr ? "/ERR" : "");
  gfx->setCursor(10, 28);
  gfx->print("type digits, [del] erase, [ok] save");

  int y = 42;
  for (int i = 0; i < CONFIG_FIELDS; i++) {
    bool sel = (i == configSel);
    gfx->setTextColor(sel ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
    gfx->setCursor(10, y);
    gfx->printf("%c %-11s now:%-7s new:%s", sel ? '>' : ' ', labels[i],
                actual[i], configTmp[i].c_str());
    y += 12;
  }
}

void render() {
  gfx->fillScreen(TFT_BLACK);

  switch (uiPage) {
    case PAGE_CONFIG:
      drawConfigPage();
      break;
    case PAGE_HELP:
      drawHeader(); drawHelpPage(); drawStatus();
      break;
    case PAGE_INFO:
      drawHeader(); drawInfoPage(); drawStatus();
      break;
    case PAGE_LORA:
      drawHeader(); drawLoraPage(); drawStatus();
      break;
    case PAGE_GPS:
    default:
      drawHeader(); drawSatelliteDataTab(); drawSkyPlot(); drawStatus();
      break;
  }

  if (gfx == &canvas) canvas.pushSprite(0, 0);
}

/* ================================================================== *
 *  Input
 *
 *  Edge-triggered: isChange() is true only on a state transition, so no
 *  blocking delay() debounce is needed. The original spent 300 ms inside
 *  delay() per keypress, which at 115200 baud drops ~3 kB of NMEA.
 * ================================================================== */

void handleConfigKeys(const Keyboard_Class::KeysState &st) {
  for (auto c : st.word) {
    /* ';' is the up arrow and '.' the down arrow on the Cardputer keyboard -
     * which is why the frequency field takes an integer rather than a decimal
     * point (see below). The original only ever moved down. */
    if (c == ';')      configSel = (configSel + CONFIG_FIELDS - 1) % CONFIG_FIELDS;
    else if (c == '.') configSel = (configSel + 1) % CONFIG_FIELDS;
    else if (c >= '0' && c <= '9') {
      if (configTmp[configSel].length() < 9) configTmp[configSel] += c;
    } else if (c == 'c') {
      uiPage = PAGE_GPS;
      return;
    }
  }

  if (st.del && configTmp[configSel].length() > 0) {
    configTmp[configSel].remove(configTmp[configSel].length() - 1);
  }

  if (!st.enter) return;

  bool gpsChanged = false, loraChanged = false;

  if (configTmp[0].length() && isValidGpio(configTmp[0].toInt())) {
    gpsRxPin = configTmp[0].toInt(); gpsChanged = true;
  }
  if (configTmp[1].length() && isValidGpio(configTmp[1].toInt())) {
    gpsTxPin = configTmp[1].toInt(); gpsChanged = true;
  }
  if (configTmp[2].length() && configTmp[2].toInt() >= 4800) {
    gpsBaud = configTmp[2].toInt(); gpsChanged = true;
    gpsAutoBaud = false;                  /* explicit choice wins */
  }
  if (configTmp[3].length()) {
    /* Entered as an integer: < 1000 is MHz (868, 915, 923), >= 1000 is kHz
     * (868500 -> 868.5 MHz). Avoids needing a decimal-point key. */
    long raw = configTmp[3].toInt();
    float f = (raw >= 1000) ? (float)raw / 1000.0f : (float)raw;
    if (f >= 150.0f && f <= 960.0f) { loraFreq = f; loraChanged = true; }
  }
  if (configTmp[4].length()) {
    int sf = configTmp[4].toInt();
    if (sf >= 5 && sf <= 12) { loraSf = sf; loraChanged = true; }
  }
  if (configTmp[5].length()) {
    int p = configTmp[5].toInt();
    if (p >= -9 && p <= 22) { loraPower = p; loraChanged = true; }
  }

  for (int i = 0; i < CONFIG_FIELDS; i++) configTmp[i] = "";
  saveConfig();

  if (gpsChanged && gpsSerial) initGPSSerial(true);
  if (loraChanged && loraState != LORA_OFF) loraInit();

  uiPage = PAGE_GPS;
}

void handleGlobalKeys(const Keyboard_Class::KeysState &st) {
  for (auto c : st.word) {
    switch (c) {
      case 's':
        gpsSerial = !gpsSerial;
        initGPSSerial(gpsSerial);
        break;
      case 't':
        if (loraState == LORA_OFF || loraState == LORA_ERROR) loraInit();
        else loraStop();
        break;
      case 'b':
        loraSend(loraBuildBeacon());
        break;
      case 'r':
        uiPage = (uiPage == PAGE_LORA) ? PAGE_GPS : PAGE_LORA;
        break;
      case 'h':
        uiPage = (uiPage == PAGE_HELP) ? PAGE_GPS : PAGE_HELP;
        break;
      case 'i':
        uiPage = (uiPage == PAGE_INFO) ? PAGE_GPS : PAGE_INFO;
        break;
      case 'c':
        uiPage = PAGE_CONFIG;
        break;
      case 'p':
        showPlotId = !showPlotId;
        break;
      case 'o':
        showPlotSystem = !showPlotSystem;
        break;
      case 'l':
        satListSerial = !satListSerial;
        if (satListSerial) nmeaSerial = false;
        break;
      case 'n':
        nmeaSerial = !nmeaSerial;
        if (nmeaSerial) satListSerial = false;
        break;
      default:
        break;
    }
  }
}

void handleKeyboard() {
  if (!M5Cardputer.Keyboard.isChange()) return;
  if (!M5Cardputer.Keyboard.isPressed()) return;

  Keyboard_Class::KeysState st = M5Cardputer.Keyboard.keysState();

  if (uiPage == PAGE_CONFIG) handleConfigKeys(st);
  else                       handleGlobalKeys(st);

  render();   /* immediate feedback, no delay() */
}

/* ================================================================== *
 *  setup / loop
 * ================================================================== */

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(64);
  M5Cardputer.Display.fillScreen(TFT_BLACK);

  initDebugSerial();

  /* Off-screen frame buffer. The StampS3A is an ESP32-S3FN8 - 8 MB flash and
   * no guaranteed PSRAM - so a full-screen 16 bpp sprite (240*135*2 = 64.8 kB)
   * has to come out of internal SRAM. Degrade to 8 bpp (32.4 kB) and finally
   * to direct drawing rather than failing outright. */
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  if (canvas.createSprite(SCREEN_W, SCREEN_H)) {
    gfx = &canvas;
  } else {
    canvas.setColorDepth(8);
    if (canvas.createSprite(SCREEN_W, SCREEN_H)) {
      gfx = &canvas;
      Serial.println("Canvas: 8bpp fallback");
    } else {
      gfx = &M5Cardputer.Display;
      Serial.println("Canvas alloc failed - drawing direct (flicker expected)");
    }
  }

  /* I2C for the Cap's PI4IOE5V6408 (shares the Adv internal bus). */
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  /* One SPI bus for microSD and SX1262. -1 = do not let the driver own a CS;
   * each device drives its own. */
  pinMode(LORA_NSS_PIN, OUTPUT); digitalWrite(LORA_NSS_PIN, HIGH);
  pinMode(SD_CS_PIN,    OUTPUT); digitalWrite(SD_CS_PIN,    HIGH);
  SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, -1);

  sdAvailable = sdInit();
  if (sdAvailable) loadConfig();

  satellites.reserve(SAT_LIST_MAX);

  /* Both radios up straight away - this is a monitoring tool, not a
   * power-budgeted node. */
  loraInit();
  gpsSerial = true;
  initGPSSerial(true);

  render();
}

void loop() {
  M5Cardputer.update();

  handleKeyboard();

  if (gpsSerial) serialGPSRead();
  loraServiceIrq();
  serialConsoleSatsList();

  static uint32_t lastPrune = 0;
  if (millis() - lastPrune > 30000) {
    lastPrune = millis();
    pruneSatellites();
  }

  static uint32_t lastRender = 0;
  if (millis() - lastRender > 250) {
    lastRender = millis();
    render();
  }

  /* No delay() here. The GNSS FIFO must be drained continuously at 115200
   * baud; yielding to the RTOS is enough to keep the idle task fed. */
  delay(1);
}
