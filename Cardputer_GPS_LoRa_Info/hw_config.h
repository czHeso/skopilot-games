/*  hw_config.h
 *
 *  Hardware pin map for:
 *    Host   : M5Stack Cardputer-Adv (StampS3A / ESP32-S3FN8)
 *    Module : M5Stack Cap LoRa-1262 (SX1262 868-923 MHz + AT6668/ATGM336H GNSS)
 *
 *  The Cap does NOT use the Grove HY2.0-4P port for its own data lines. It sits
 *  on the 2x7 "Cap-Bus" header on the back of the Cardputer-Adv, which carries:
 *
 *      G3, G4, G6, G40, G14, G39, G5, 5V, GND, G8, G9, OUT, SDA, SCL, G13, G15
 *
 *  The HY2.0-4P connector on the Cap is a *pass-through* for the user, not the
 *  link to the GPS/LoRa chips. Wiring the GPS to G1/G2 (the Grove port) - which
 *  is what the original sketch defaulted to - therefore never produces data.
 *
 *  IMPORTANT: this Cap is Cardputer-Adv only. On the older Cardputer 1.0/1.1 the
 *  GPIOs used below (3, 4, 5, 6, 13, 15) are the keyboard scan matrix. The Adv
 *  moved the keyboard to an I2C TCA8418 controller, which is what frees them.
 */

#pragma once

/* ------------------------------------------------------------------ *
 *  GNSS - AT6668 / ATGM336H, UART
 * ------------------------------------------------------------------ */

/* Cardputer RX  <-  GPS TX */
#define GPS_RX_PIN            15
/* Cardputer TX  ->  GPS RX */
#define GPS_TX_PIN            13

/* Factory default of the AT6668 on this Cap is 115200 baud.
 * Older M5Stack GPS units (NEO-M8N, GPS Unit v1) used 9600 - do not copy
 * those settings over. */
#define GPS_BAUD_DEFAULT      115200

/* UART number. UART0 is the USB-CDC console, so use UART1. */
#define GPS_UART_NUM          1

/* At 115200 baud a full GNSS epoch (GGA+RMC+GSA*4+GSV*12+VTG) is ~1.5 kB and
 * arrives as one burst. The Arduino default RX buffer is 256 B, which silently
 * drops sentences the moment a redraw or an SD write stalls the loop.
 * Must be set BEFORE begin(). */
#define GPS_RX_BUFFER_SIZE    2048

/* ------------------------------------------------------------------ *
 *  LoRa - Semtech SX1262, SPI
 * ------------------------------------------------------------------ */

/* NOTE: SCK/MISO/MOSI are the *same* bus the on-board microSD uses.
 * Both devices must be driven through one SPIClass instance with their own
 * chip selects - see loraInit() / sdInit(). */
#define SPI_SCK_PIN           40
#define SPI_MISO_PIN          39
#define SPI_MOSI_PIN          14

#define LORA_NSS_PIN           5   /* chip select        */
#define LORA_DIO1_PIN          4   /* IRQ                */
#define LORA_RST_PIN           3   /* reset              */
#define LORA_BUSY_PIN          6   /* busy               */

#define SD_CS_PIN             12   /* Cardputer on-board microSD */

/* Radio defaults. 868.0 MHz is the EU ISM band and matches the antenna the
 * Cap ships with. Legal band edges: 868-923 MHz for this hardware. */
#define LORA_FREQ_DEFAULT     868.0f   /* MHz   */
#define LORA_BW_DEFAULT       125.0f   /* kHz   */
#define LORA_SF_DEFAULT       9         /* 5..12 */
#define LORA_CR_DEFAULT       7         /* 4/x   */
#define LORA_SYNCWORD         0x12      /* 0x12 private, 0x34 LoRaWAN */
#define LORA_POWER_DEFAULT    14        /* dBm - 14 dBm is the EU868 ERP limit */
#define LORA_PREAMBLE_LEN     8
#define LORA_CURRENT_LIMIT    140.0f    /* mA OCP */

/* DIO3 supplies the TCXO on M5Stack's SX1262 designs. If a board revision ever
 * ships with a plain crystal instead, radio.begin() answers -706/-707; the
 * driver below automatically retries with 0.0 V (XTAL) so both revisions work.
 */
#define LORA_TCXO_VOLTAGE     1.8f
#define LORA_USE_LDO          false     /* false = internal DC-DC (efficient) */

/* ------------------------------------------------------------------ *
 *  I2C - PI4IOE5V6408 port expander on the Cap
 * ------------------------------------------------------------------ */

/* Internal I2C bus of the Cardputer-Adv, also exposed on the Cap-Bus header. */
#define I2C_SDA_PIN            8
#define I2C_SCL_PIN            9
#define I2C_FREQ              400000

/* The Cap routes the RF path through an antenna switch driven by P0 of a
 * PI4IOE5V6408. Without pulling P0 high the SX1262 initialises fine over SPI
 * but transmits and receives into a dead end - the classic "LoRa says OK but
 * range is 2 metres" symptom. */
#define PI4IOE_ADDR           0x43
#define PI4IOE_PIN_ANT_SW     0        /* P0 */

/* PI4IOE5V6408 register map */
#define PI4IOE_REG_DIRECTION  0x03
#define PI4IOE_REG_OUTPUT     0x05
#define PI4IOE_REG_HIGH_Z     0x07     /* 1 = high impedance, 0 = driven */
#define PI4IOE_REG_PULL_EN    0x0B

/* ------------------------------------------------------------------ *
 *  Display - Cardputer / Cardputer-Adv, ST7789V2
 * ------------------------------------------------------------------ */
#define SCREEN_W              240
#define SCREEN_H              135
