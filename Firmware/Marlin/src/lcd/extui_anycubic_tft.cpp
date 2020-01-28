
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
#include "../libs/numtostr.h"
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

// Track incoming command bytes from the LCD
int inbound_count;

/**
 * Write direct unsigned int to Anycubic LCD
 */
void write_to_lcd_PROTOCOL(unsigned int value) {
  LCD_SERIAL.write(value);
}

/**
 * Write direct to Anycubic LCD
 */
void write_to_lcd_PGM(PGM_P const message) {
  LCD_SERIAL.write((char*)message, strlen(message));
}

/**
 * Write line break to Anycubic LCD
 */
void write_to_lcd_ENTER() {
  write_to_lcd_PGM("\r\n");
}

/**
 * Write space to Anycubic LCD
 */
void write_to_lcd_SPACE() {
  write_to_lcd_PGM(" ");
}

/**
 * Write to Anycubic LCD with line breaks
 */
void write_to_lcd_NPGM(PGM_P const message) {
  write_to_lcd_PGM(message);
  write_to_lcd_ENTER();
}

// M111 S7


/**
 * Receive a curly brace command and translate to G-code.
 * Currently {E:0} is not handled. Its function is unknown,
 * but it occurs during the temp window after a sys build.
 */
void process_lcd_command(const char* command) {
  
  const char *current = command;
  byte command_code = *current++;
  int commandId = -1;

  if (command_code == 'A') {
    char* pRest;
    commandId = (int)(strtod(current, &pRest));
  
    switch (commandId) {
      // A0 GET HOTEND TEMP
      case 0:
        write_to_lcd_PGM("A0V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degHotend(0) + 0.5)));
        write_to_lcd_ENTER();
        break;

      // A1 GET HOTEND TARGET TEMP
      case 1:
        write_to_lcd_PGM("A1V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degTargetHotend(0) + 0.5)));
        write_to_lcd_ENTER();
        break;

      // A2 GET HOTBED TEMP
      case 2:
        write_to_lcd_PGM("A2V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degBed() + 0.5)));
        write_to_lcd_ENTER();

      // A3 GET HOTBED TARGET TEMP
      case 3:
        write_to_lcd_PGM("A3V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degTargetBed() + 0.5)));
        write_to_lcd_ENTER();
        break;

      // A4 GET FAN SPEED
      case 4:
        unsigned int temp;
        temp = constrain(thermalManager.fanPercent(thermalManager.fan_speed[0]),0,100);
        write_to_lcd_PGM("A4V ");
        write_to_lcd_PROTOCOL(temp);
        write_to_lcd_ENTER();
        break;

      // Command code UNKOWN
      default:
        DEBUG_ECHOLNPAIR("UNKNOWN COMMAND A", commandId);
        break;
    }
  } else {
    DEBUG_ECHOLNPAIR("UNKNOWN COMMAND FORMAT ", command);
  }
}

/**
 * Parse incomming LCD bytes
 */
void parse_lcd_byte(byte b) {
  static char inbound_buffer[MAX_ANY_COMMAND];

  // A line-ending
  if (b == '\n' || b == '\r') {
    if (inbound_count > 0) {
      inbound_buffer[inbound_count] = 0;
      process_lcd_command(inbound_buffer);
    }
    inbound_count = 0;

  // Buffer only if we have enougth space
  } else if (inbound_count < MAX_ANY_COMMAND - 2) {
    inbound_buffer[inbound_count++] = b;
  }
}

/**
 * Implementation in ExtUI
 */
namespace ExtUI {
  /**
   * Initialize Display
   */
  void onStartup() {
    inbound_count = 0;
    LCD_SERIAL.begin(115200);
    write_to_lcd_NPGM("J17");      // J17 Main board reset
    delay(10);
    write_to_lcd_NPGM("J12");      // J12 Ready
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


  #if HAS_LEVELING
    //bool getLevelingActive();
    //void setLevelingActive(const bool);
    ////bool getMeshValid();
    #if HAS_MESH
      //bed_mesh_t& getMeshArray();
      //float getMeshPoint(const xy_uint8_t &pos);
      //void setMeshPoint(const xy_uint8_t &pos, const float zval);
      void onMeshUpdate(const int8_t xpos, const int8_t ypos, const float zval) {}
      //inline void onMeshUpdate(const xy_int8_t &pos, const float zval) { onMeshUpdate(pos.x, pos.y, zval); }
    #endif
  #endif

  #if HAS_PID_HEATING
    void OnPidTuning(const result_t rst) {}
  #endif
}

#endif // ANYCUBIC_TFT_MODEL
