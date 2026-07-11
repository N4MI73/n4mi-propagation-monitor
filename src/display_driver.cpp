// N4MI Desktop Instrument Series - Propagation Monitor
// display_driver.cpp
//
// CONFIRMED 2026-07-04: This unit uses the CO5300 display driver, NOT
// SH8601. The T-Encoder Pro shipped with two panel/touch revisions:
//   Original: SH8601 display + CHSC5816 touch (I2C 0x2E)
//   Newer:    CO5300 display + CST816 touch (I2C 0x15)
// SH8601 driver accepted all commands without error but produced a
// blank screen -- CO5300 driver confirmed working via live red-fill
// test and working text rendering. If a FUTURE T-Encoder Pro unit in
// this project turns out to have the original SH8601 panel instead,
// swap Arduino_CO5300 for Arduino_SH8601 below -- same constructor
// signature, drop-in swap.
//
// UPDATED 2026-07-10: all Serial output in this file is now guarded
// with `if (Serial)`. Confirmed by real-hardware testing that
// Serial.println() can stall for real time on this ESP32-S3 native
// USB-CDC setup when nothing is connected to read it -- since these
// calls run during boot, an unguarded print here could contribute to
// a slow-feeling startup on a device with no computer attached, which
// is the normal, expected way this instrument runs once deployed.

#include "display_driver.h"

// See main.cpp for the full explanation -- set to 1 to compile in
// diagnostic Serial output, 0 (default) to compile it out entirely.
#define DEBUG_VERBOSE 0

Arduino_GFX *gfx = nullptr;
static Arduino_OLED *oled = nullptr;
static Arduino_DataBus *bus = nullptr;

bool display_init() {
#if DEBUG_VERBOSE
    if (Serial) Serial.println("[display] display_init() starting");
#endif

    bus = new Arduino_ESP32QSPI(
        PIN_LCD_CS, PIN_LCD_SCLK,
        PIN_LCD_SDIO0, PIN_LCD_SDIO1, PIN_LCD_SDIO2, PIN_LCD_SDIO3);

    pinMode(PIN_LCD_VCI_EN, OUTPUT);
    digitalWrite(PIN_LCD_VCI_EN, HIGH);
    delay(20);

    // Arduino_CO5300 constructor signature confirmed by directly
    // inspecting Arduino_CO5300.h -- identical shape to Arduino_SH8601:
    // (bus, rst, r, w, h, col_offset1-4) -- no ips parameter.
    gfx = new Arduino_CO5300(bus, PIN_LCD_RST, 0 /* rotation */,
                              DISPLAY_WIDTH, DISPLAY_HEIGHT);
    oled = static_cast<Arduino_OLED *>(gfx);

    if (!gfx->begin()) {
#if DEBUG_VERBOSE
        if (Serial) Serial.println("[display] gfx->begin() FAILED");
#endif
        return false;
    }

    gfx->fillScreen(RGB565_BLACK);

    // Brightness ramp -- 0 to 200 (not full 255) to avoid a startling
    // full-brightness flash on a dark desk. Change the loop bound below
    // for a different max brightness.
    for (uint8_t i = 0; i < 200; i += 4) {
        oled->setBrightness(i);
        delay(2);
    }

#if DEBUG_VERBOSE
    if (Serial) Serial.println("[display] display_init() complete");
#endif
    return true;
}

void display_clear() {
    if (gfx) gfx->fillScreen(RGB565_BLACK);
}

void display_show_boot_message(const char *line1, const char *line2) {
    if (!gfx) return;
    display_clear();
    gfx->setTextColor(RGB565_WHITE);
    gfx->setTextSize(2);
    gfx->setCursor(40, 180);
    gfx->println(line1);
    if (line2) {
        gfx->setCursor(40, 210);
        gfx->println(line2);
    }
}
