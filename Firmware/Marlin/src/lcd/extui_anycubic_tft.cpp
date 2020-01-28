
/**
 * extui_anycubic_tft.cpp
 *
 * Supports anycubic TFT touchscreen
 * 
 * Copyright (c) 2020 Robert Stein (StoneColdCrazy)
 */

#include "../inc/MarlinConfigPre.h"

#if ENABLED(ANYCUBIC_TFT_MODEL)

#include "extensible_ui/ui_api.h"

#include "ultralcd.h"
#include "../sd/cardreader.h"
#include "../module/temperature.h"
#include "../module/stepper.h"
#include "../module/motion.h"
#include "../libs/duration_t.h"
#include "../module/printcounter.h"
#include "../gcode/queue.h"

#define DEBUG_OUT ENABLED(ANYCUBIC_TFT_DEBUG)
#include "../core/debug_out.h"

#ifdef ANYCUBIC_TFT_SERIAL
  #define LCD_SERIAL ANYCUBIC_TFT_SERIAL
#else
  #define LCD_SERIAL Serial1
#endif
#define MAX_ANY_COMMAND (32 + LONG_FILENAME_LENGTH) * 2

/**
 * Write direct to Anycubic LCD
 */
void write_to_lcd_PGM(PGM_P const message) {
  LCD_SERIAL.Print::write(message, sizeof(message));
}

/**
 * Write line break to Anycubic LCD
 */
void write_to_lcd_ENTER() {
  write_to_lcd_PGM(PSTR("\r\n"));
}

/**
 * Write space to Anycubic LCD
 */
void write_to_lcd_SPACE() {
  write_to_lcd_PGM(PSTR(" "));
}

/**
 * Write to Anycubic LCD with line breaks
 */
void write_to_lcd_NPGM(PGM_P const message) {
  write_to_lcd_PGM(message);
  write_to_lcd_ENTER();
}

/**
 * Parse incomming LCD bytes
 */
void parse_lcd_byte(byte b) {
}

/**
 * Implementation in ExtUI
 */
namespace ExtUI {
  /**
   * Initialize Display
   */
  void onStartup() {
    LCD_SERIAL.begin(115200);
    write_to_lcd_NPGM(PSTR("J17"));      // J17 Main board reset
    delay(10);
    write_to_lcd_NPGM(PSTR("J12"));      // J12 Ready
  }

  /**
   * When idle, parse incomming lcd messages
   */
  void onIdle() {
    while (LCD_SERIAL.available()) {
      parse_lcd_byte((byte)LCD_SERIAL.read() & 0x7F);
    }
  }

  // UNUSED for LCD
  void onPrinterKilled(PGM_P error, PGM_P component) {}
  void onPrintTimerStarted() {}
  void onPrintTimerPaused() {}
  void onPrintTimerStopped() {} 
  void onStatusChanged(const char * const) {}
  void onMediaInserted() {};
  void onMediaError() {};
  void onMediaRemoved() {};
  void onPlayTone(const uint16_t, const uint16_t) {}
  void onFilamentRunout(const extruder_t extruder) {}
  void onUserConfirmRequired(const char * const) {}
  void onFactoryReset() {}
  void onStoreSettings(char*) {}
  void onLoadSettings(const char*) {}
  void onConfigurationStoreWritten(bool) {}
  void onConfigurationStoreRead(bool) {}
}

#endif // ANYCUBIC_TFT_MODEL
