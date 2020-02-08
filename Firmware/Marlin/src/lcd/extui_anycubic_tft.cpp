
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
  #define LCD_SERIAL                  ANYCUBIC_TFT_SERIAL
#else
  #define LCD_SERIAL                  Serial1
#endif
#define MSG_ANYCUBIC_VERSION          "V202"
#define MAX_ANY_COMMAND               (32 + LONG_FILENAME_LENGTH) * 2
#define ANYCUBIC_STATE_IDLE           0
#define ANYCUBIC_STATE_SDPRINT        1
#define ANYCUBIC_STATE_SDPAUSE        2
#define ANYCUBIC_STATE_SDPAUSE_REQ    3
#define ANYCUBIC_STATE_SDPAUSE_OOF    4
#define ANYCUBIC_STATE_SDSTOP_REQ     5
#define ANYCUBIC_STATE_SDOUTAGE       99

char AnycubicState = ANYCUBIC_STATE_IDLE;
char AnycubicSelectedDirectory[30] = { 0 };
uint16_t AnycubicFilenumber = 0;
uint8_t AnycubicSpecialMenu = false;
bool AnycubicIsParked = false;

/**
* Anycubic TFT pause states:
*
* 0 - printing / stopped
* 1 - regular pause
* 2 - M600 pause
* 3 - filament runout pause
* 4 - nozzle timeout on M600
* 5 - nozzle timeout on filament runout
*/
uint8_t AnycubicPauseState = 0;

// Track incoming command bytes from the LCD
int inbound_count;

/**
 * Write direct uint32_t to Anycubic LCD
 */
void write_to_lcd_PROTOCOL(uint32_t value) {
  LCD_SERIAL.print(value);
}

/**
 * Write direct uint16_t to Anycubic LCD
 */
void write_to_lcd_PROTOCOL(uint16_t value) {
  LCD_SERIAL.print(value);
}

/**
 * Write direct uint8_t to Anycubic LCD
 */
void write_to_lcd_PROTOCOL(uint8_t value) {
  LCD_SERIAL.print(value);
}

/**
 * Write direct float int to Anycubic LCD
 */
void write_to_lcd_PROTOCOL(float value) {
  LCD_SERIAL.print(value);
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

/**
 * Write to Anycubic LCD with line breaks twice (used for special menu)
 */
void write_to_lcd_NPGMTwice(PGM_P const message) {
  write_to_lcd_PGM(message);
  write_to_lcd_ENTER();
  write_to_lcd_PGM(message);
  write_to_lcd_ENTER();
}

/**
 * Check if code was received and returns position
 * after code or NULL
 */
char *isCodeReceived(char *message, char code) {
  char *pch = strchr(message, code);
  return (pch != NULL) ? &(pch[1]) : NULL; 
}


/**
 * Handle start printing function
 */
void HandleAnycubicStartPrint() {
}

/**
 * Handle pause printing function
 */
void HandleAnycubicPausePrint() {
}

/**
 * Handle stop printing function
 */
void HandleAnycubicStopPrint() {
}

/**
 * Handle filament change pause function
 */
void HandleAnycubicFilamentChangePause() {
}

/**
 * Handle filament change resume function
 */
void HandleAnycubicFilamentChangeResume() {
}

/**
 * Handle reheat nozzle function
 */
void HandleAnycubicReheatNozzle() {
}

/**
 * Handle park after stop function
 */
void HandleAnycubicParkAfterStop() {
}

/**
 * Handle file listing on sd card function
 */
void HandleAnycubicSdCardFileListing() {
  if (AnycubicSpecialMenu) {
    switch (AnycubicFilenumber) {
      case 0: // First Page
        write_to_lcd_NPGMTwice("<Z Up 0.1>");
        write_to_lcd_NPGMTwice("<Z Up 0.02>");
        write_to_lcd_NPGMTwice("<Z Down 0.02>");
        write_to_lcd_NPGMTwice("<Z Down 0.1>");
        break;

      case 4: // Second Page
        write_to_lcd_NPGMTwice("<Preheat bed>");
        write_to_lcd_NPGMTwice("<Start Mesh Leveling>");
        write_to_lcd_NPGMTwice("<Next Mesh Point>");
        write_to_lcd_NPGMTwice("<Save EEPROM>");
        break;

      case 8: // Third Page
        write_to_lcd_NPGMTwice("<Exit>");
        write_to_lcd_NPGMTwice("<Auto Tune Hotend PID>");
        write_to_lcd_NPGMTwice("<Auto Tune Hotbed PID>");
        write_to_lcd_NPGMTwice("<Load FW Defaults>");
        break;

      case 12: // Fourth Page
        write_to_lcd_NPGMTwice("<FilamentChange Pause>");
        write_to_lcd_NPGMTwice("<FilamentChange Resume>");
        break;

      default:
        break;
    }
  
  #ifdef SDSUPPORT
    } else if(card.isMounted()) {

  #endif
  } else {
    write_to_lcd_NPGM("<Special_Menu>");
    write_to_lcd_NPGM("<Special_Menu>");
  }
}


/**
 * Special menu for bed leveling
 */
void HandleAnycubicSpecialMenu() {
  if(strcmp(AnycubicSelectedDirectory, "<special menu>") == 0) {
    AnycubicSpecialMenu=true;
  } else if (strcmp(AnycubicSelectedDirectory, "<auto tune hotend pid>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Auto Tune Hotend PID");
    queue.enqueue_now_P(PSTR("M106 S204\nM303 E0 S210 C15 U1"));
  } else if (strcmp(AnycubicSelectedDirectory, "<auto tune hotbed pid>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Auto Tune Hotbed Pid");
    queue.enqueue_now_P(PSTR("M303 E-1 S60 C6 U1"));
  } else if (strcmp(AnycubicSelectedDirectory, "<save eeprom>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Save EEPROM");
    queue.enqueue_now_P(PSTR("M500"));
    //buzzer.tone(105, 1108);
    //buzzer.tone(210, 1661);
  } else if (strcmp(AnycubicSelectedDirectory, "<load fw defaults>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Load FW Defaults");
    queue.enqueue_now_P(PSTR("M502"));
    //buzzer.tone(105, 1661);
    //buzzer.tone(210, 1108);
  } else if (strcmp(AnycubicSelectedDirectory, "<preheat bed>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Preheat Bed");
    queue.enqueue_now_P(PSTR("M140 S60"));
  } else if (strcmp(AnycubicSelectedDirectory, "<start mesh leveling>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Start Mesh Leveling");
    queue.enqueue_now_P(PSTR("G29 S1"));
  } else if (strcmp(AnycubicSelectedDirectory, "<next mesh point>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Next Mesh Point");
    queue.enqueue_now_P(PSTR("G29 S2"));
  } else if (strcmp(AnycubicSelectedDirectory, "<z up 0.1>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Z Up 0.1");
    queue.enqueue_now_P(PSTR("G91\nG1 Z+0.1\nG90"));
  } else if (strcmp(AnycubicSelectedDirectory, "<z up 0.02>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Z Up 0.02");
    queue.enqueue_now_P(PSTR("G91\nG1 Z+0.02\nG90"));
  } else if (strcmp(AnycubicSelectedDirectory, "<z down 0.02>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Z Down 0.02");
    queue.enqueue_now_P(PSTR("G91\nG1 Z-0.02\nG90"));
  } else if (strcmp(AnycubicSelectedDirectory, "<z down 0.1>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: Z Down 0.1");
    queue.enqueue_now_P(PSTR("G91\nG1 Z-0.1\nG90"));
  } else if (strcmp(AnycubicSelectedDirectory, "<filamentchange pause>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: FilamentChange Pause");
    HandleAnycubicFilamentChangePause();
  } else if (strcmp(AnycubicSelectedDirectory, "<filamentchange resume>") == 0) {
    SERIAL_ECHOLNPGM("Special Menu: FilamentChange Resume");
    HandleAnycubicFilamentChangeResume();
  } else if (strcmp(AnycubicSelectedDirectory, "<exit>") == 0) {
    AnycubicSpecialMenu=false;
  }
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
      {
        write_to_lcd_PGM("A0V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degHotend(0) + 0.5)));
        write_to_lcd_ENTER();
        break;
      }

      // A1 GET HOTEND TARGET TEMP
      case 1:
      {
        write_to_lcd_PGM("A1V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degTargetHotend(0) + 0.5)));
        write_to_lcd_ENTER();
        break;
      }

      // A2 GET HOTBED TEMP
      case 2:
      {
        write_to_lcd_PGM("A2V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degBed() + 0.5)));
        write_to_lcd_ENTER();
      }

      // A3 GET HOTBED TARGET TEMP
      case 3:
      {
        write_to_lcd_PGM("A3V ");
        write_to_lcd_PGM(ui8tostr3(uint8_t(thermalManager.degTargetBed() + 0.5)));
        write_to_lcd_ENTER();
        break;
      }

      // A4 GET FAN SPEED
      case 4:
      {
        uint8_t temp;
        temp = thermalManager.fanPercent(thermalManager.fan_speed[0]);
        write_to_lcd_PGM("A4V ");
        write_to_lcd_PROTOCOL(temp);
        write_to_lcd_ENTER();
        break;
      }

      // A5 GET CURRENT COORDINATE
      case 5:
      {
        write_to_lcd_PGM("A5V");
        write_to_lcd_PGM(" X: ");
        write_to_lcd_PROTOCOL(current_position[X_AXIS]);
        write_to_lcd_PGM(" Y: ");
        write_to_lcd_PROTOCOL(current_position[Y_AXIS]);
        write_to_lcd_PGM(" Z: ");
        write_to_lcd_PROTOCOL(current_position[Z_AXIS]);
        write_to_lcd_SPACE();
        write_to_lcd_ENTER();
        break;
      }

      // A6 GET SD CARD PRINTING STATUS
      case 6:
      {
        #ifdef SDSUPPORT
          if(card.isPrinting()) {
            write_to_lcd_PGM("A6V ");
            if(card.isMounted()) {
              write_to_lcd_PGM(ui8tostr3(uint8_t(card.percentDone() + 0.5)));
            } else {
              write_to_lcd_PGM("J02");
            }
          } else {
            write_to_lcd_PGM("A6V ---");
          }
          write_to_lcd_ENTER();
        #endif
        break;
      }

      // A7 GET PRINTING TIME
      case 7:
      {
        write_to_lcd_PGM("A7V ");
        if(print_job_timer.isRunning()) {
          uint16_t time = millis() / 60000 - print_job_timer.duration() / 60000;
          write_to_lcd_PGM(ui8tostr3(time / 60));
          write_to_lcd_SPACE();
          write_to_lcd_PGM("H");
          write_to_lcd_SPACE();
          write_to_lcd_PGM(ui8tostr3(time % 60));
          write_to_lcd_SPACE();
          write_to_lcd_PGM("M");
        } else {
          write_to_lcd_SPACE();
          write_to_lcd_PGM("999:999");
        }
        write_to_lcd_ENTER();
        break;
      }

      // A8 GET SD LIST
      case 8:
      {
        #ifdef SDSUPPORT
          AnycubicSelectedDirectory[0] = 0;
          if(!IS_SD_INSERTED()) {
            write_to_lcd_NPGM("J02");
          } else {
            char *commandDataS = isCodeReceived(pRest, 'S');
            if(commandDataS != NULL) {
              AnycubicFilenumber = (uint16_t)strtod(commandDataS, NULL);
            }   

            write_to_lcd_NPGM("FN "); // Filelist start
            HandleAnycubicSdCardFileListing();
            write_to_lcd_NPGM("END"); // Filelist stop
          }
        #endif
        break;
      }

      // A9 pause sd print
      case 9:
      {
        #ifdef SDSUPPORT
          if(card.isPrinting()) {
            HandleAnycubicPausePrint();
          } else {
            AnycubicPauseState = 0;
            DEBUG_ECHOPAIR(" DEBUG: AI3M Pause State: ", AnycubicPauseState);
            DEBUG_EOL();
            HandleAnycubicStopPrint();
          }
        #endif
        break;
      }

      // A10 resume sd print
      case 10:
      {
        #ifdef SDSUPPORT
          if((AnycubicState == ANYCUBIC_STATE_SDPAUSE) || (AnycubicState == ANYCUBIC_STATE_SDOUTAGE)) {
            HandleAnycubicStartPrint();
            write_to_lcd_NPGM("J04");// J04 printing form sd card now
            DEBUG_ECHOLNPGM("TFT Serial Debug: SD print started... J04");
          }
          if (AnycubicPauseState > 3) {
            HandleAnycubicReheatNozzle();
          }
        #endif
        break;
      }
      
      // A11 STOP SD PRINT
      case 11:
      {
        #ifdef SDSUPPORT
          if(card.isPrinting() || (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
            HandleAnycubicStopPrint();
          } else {
            write_to_lcd_NPGM("J16");
            AnycubicState = ANYCUBIC_STATE_IDLE;
            AnycubicPauseState = 0;
            DEBUG_ECHOPAIR(" DEBUG: AI3M Pause State: ", AnycubicPauseState);
            DEBUG_EOL();
          }
        #endif
        break;
      }

      // A12 kill
      case 12:
      {
        kill(PSTR(MSG_ERR_KILLED));
        break;
      }

      // A13 SELECTION FILE
      case 13:
      {
        #ifdef SDSUPPORT
          //if(AnycubicState != ANYCUBIC_STATE_SDOUTAGE) {
            char *starpos = (strchr(pRest,'*'));          
            if (pRest[1] == '/') {
              strcpy(AnycubicSelectedDirectory, pRest+2);
            } else if (pRest[1] == '<') {
              strcpy(AnycubicSelectedDirectory, pRest+2);
            } else {
              AnycubicSelectedDirectory[0]=0;
              if(starpos != NULL) {
                *(starpos-1)='\0';
              }
              card.openFileRead(pRest + 2,true);
              if (card.isFileOpen()) {
                write_to_lcd_NPGM("J20"); // J20 Open successful
                DEBUG_ECHOLNPGM("TFT Serial Debug: File open successful... J20");
              } else {
                write_to_lcd_NPGM("J21"); // J21 Open failed
                DEBUG_ECHOLNPGM("TFT Serial Debug: File open failed... J21");
              }
            }
            write_to_lcd_ENTER();
          //}
        #endif
        break;
      }

      // A14 START PRINTING
      case 14:
      {
        #ifdef SDSUPPORT
          if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE) && card.isFileOpen()) {
            AnycubicPauseState = 0;
            DEBUG_ECHOPAIR(" DEBUG: AI3M Pause State: ", AnycubicPauseState);
            DEBUG_EOL();
            HandleAnycubicStartPrint();
            AnycubicIsParked = false;
            write_to_lcd_NPGM("J04"); // J04 Starting Print
            DEBUG_ECHOLNPGM("TFT Serial Debug: Starting SD Print... J04");
          }
        #endif
        break;
      }

      // A15 RESUMING FROM OUTAGE
      case 15:
      {
        // NO SUPPORT!
        break;
      }

      // A16 set hotend temp
      case 16:
      {
        unsigned int temp;
        char *commandDataS= isCodeReceived(pRest, 'S');
        char *commandDataC = isCodeReceived(pRest, 'C');
        if(commandDataS != NULL) {
          temp = constrain(strtod(commandDataS, NULL), 0, 275);
          thermalManager.setTargetHotend(temp, 0);
        } else if((commandDataC != NULL) && (!planner.movesplanned())) {
          if ((current_position[Z_AXIS] < 10)) {
            queue.enqueue_now_P(PSTR("G1 Z10")); //RASE Z AXIS
          }
          temp = constrain(strtod(commandDataC, NULL), 0, 275);
          thermalManager.setTargetHotend(temp, 0);
        }
        break;
      }

      // A17 set heated bed temp
      case 17:
      {
        unsigned int temp;
        char *commandDataS = isCodeReceived(pRest, 'S');
        if(commandDataS != NULL) {
          temp = constrain(strtod(commandDataS, NULL), 0, 150);
          thermalManager.setTargetBed(temp);
        }
        break;
      }

      // A18 set fan speed
      case 18:
      {
        unsigned int temp;
        char *commandDataS = isCodeReceived(pRest, 'S');
        if(commandDataS != NULL) {
          temp = (strtod(commandDataS, NULL) * 255 / 100);
          temp = constrain(temp, 0, 255);
          thermalManager.set_fan_speed(0, temp);
        } else {
          thermalManager.set_fan_speed(0, 255);
        }
        write_to_lcd_ENTER();
        break;
      }

      // A19 stop stepper drivers
      case 19:
      {
        if((!planner.movesplanned())
            #ifdef SDSUPPORT
            &&(!card.isPrinting())
            #endif
        ) {
          quickstop_stepper();
          //disable_X();
          //disable_Y();
          //disable_Z();
          //disable_E0();
        }
        write_to_lcd_ENTER();
        break;
      }

      // A20 read printing speed
      case 20:
      {
        char *commandDataS = isCodeReceived(pRest, 'S');
        if(commandDataS != NULL) {
          feedrate_percentage = constrain(strtod(commandDataS, NULL), 40, 999);
        } else {
          write_to_lcd_PGM("A20V ");
          write_to_lcd_PROTOCOL((uint8_t)feedrate_percentage);
          write_to_lcd_ENTER();
        }
        break;
      }

      // A21 all home
      case 21:
      {
        if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
          // Parse for commands
          char *commandDataX = isCodeReceived(pRest, 'X');
          char *commandDataY = isCodeReceived(pRest, 'Y');
          char *commandDataZ = isCodeReceived(pRest, 'Z');
          char *commandDataC = isCodeReceived(pRest, 'C');
          
          if ((commandDataX != NULL) || (commandDataY != NULL) || (commandDataZ != NULL)) {
            if (commandDataX != NULL) {
              queue.enqueue_now_P(PSTR("G28 X"));
            }
            if (commandDataY != NULL) {
              queue.enqueue_now_P(PSTR("G28 Y"));
            }
            if (commandDataZ != NULL) {
              queue.enqueue_now_P(PSTR("G28 Z"));
            }
          } else if(commandDataC != NULL) {
            queue.enqueue_now_P(PSTR("G28"));
          }
        }
        break;
      }

      // A22 move X/Y/Z or extrude
      case 22:
      {
        if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
          unsigned int movespeed = 0;
          float coorvalue;
          char moveAxis = 0;
          char value[30];

          // Parse for commands
          char *commandDataF = isCodeReceived(pRest, 'F');
          char *commandDataX = isCodeReceived(pRest, 'X');
          char *commandDataY = isCodeReceived(pRest, 'Y');
          char *commandDataZ = isCodeReceived(pRest, 'Z');
          char *commandDataE = isCodeReceived(pRest, 'E');

          // Set feedrate
          if(commandDataF != NULL) {
            movespeed = (unsigned int)strtod(commandDataF, NULL);
          }

          queue.enqueue_now_P(PSTR("G91")); // relative coordinates

          // Move in X direction
          if(commandDataX != NULL) {
            coorvalue = strtod(commandDataX, NULL);
            moveAxis = 'X';

          // Move in Y direction
          } else if(commandDataY != NULL) {
            coorvalue = strtod(commandDataY, NULL);
            moveAxis = 'Y';
            
          // Move in Z direction
          } else if(commandDataZ != NULL) {
            coorvalue = strtod(commandDataZ, NULL);
            moveAxis = 'Z';

          // Exrude
          } else if(commandDataE != NULL) {
            coorvalue = strtod(commandDataE, NULL);
            moveAxis = 'E';
          }

          // Set value
          if (moveAxis != 0) {
             if((coorvalue <= 0.2) && (coorvalue > 0)) {
              sprintf_P(value, PSTR("G1 %c0.1F%i"), moveAxis, movespeed);
            } else if((coorvalue <= -0.1) && (coorvalue>-1)) {
              sprintf_P(value, PSTR("G1 %c-0.1F%i"), moveAxis, movespeed);
            } else {
              if (moveAxis == 'E') {
                movespeed = 500;
              }
              sprintf_P(value, PSTR("G1 %c%iF%i"), moveAxis, int(coorvalue), movespeed);
            }
            queue.enqueue_one_now(value);
          }
          queue.enqueue_now_P(PSTR("G90")); // absolute coordinates
        }
        write_to_lcd_ENTER();
        break;
      }

      // A23 preheat pla
      case 23:
      {
        if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
          if((current_position[Z_AXIS] < 10)) {
            queue.enqueue_now_P(PSTR("G1 Z10")); // RAISE Z AXIS
          }
          thermalManager.setTargetBed(PREHEAT_1_TEMP_BED);
          thermalManager.setTargetHotend(PREHEAT_1_TEMP_HOTEND, 0);
          write_to_lcd_NPGM("OK");
        }
        break;
      }

      // A24 preheat abs
      case 24:
      {
        if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
          if((current_position[Z_AXIS] < 10)) {
            queue.enqueue_now_P(PSTR("G1 Z10")); // RAISE Z AXIS
          }
          thermalManager.setTargetBed(PREHEAT_2_TEMP_BED);
          thermalManager.setTargetHotend(PREHEAT_2_TEMP_HOTEND, 0);
          write_to_lcd_NPGM("OK");
        }
        break;
      }

      // A25 cool down
      case 25:
      {
        if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
          thermalManager.setTargetHotend(0,0);
          thermalManager.setTargetBed(0);
          write_to_lcd_NPGM("J12"); // J12 cool down
        }
        break;
      }

      // A26 refresh SD
      case 26:
      {
        #ifdef SDSUPPORT
          if (AnycubicSelectedDirectory[0] == 0) {
            card.mount();
          } else {
            if ((AnycubicSelectedDirectory[0] == '.') && (AnycubicSelectedDirectory[1] == '.')) {
              card.cdup();
            } else {
              if (AnycubicSelectedDirectory[0] == '<') {
                HandleAnycubicSpecialMenu();
              } else {
                card.cd(AnycubicSelectedDirectory);
              }
            }
          }

          AnycubicSelectedDirectory[0] = 0;
          if(!IS_SD_INSERTED())
          {
            write_to_lcd_NPGM("J02"); // J02 SD Card initilized
            DEBUG_ECHOLNPGM("TFT Serial Debug: SD card initialized... J02");
          }
        #endif
        break;
      }

      // A27 servos angles  adjust
      #ifdef SERVO_ENDSTOPS
      case 27:
      {
        break;
      }
      #endif

      // A28 filament test
      case 28:
      {
        write_to_lcd_ENTER();
        break;
      }

      #ifndef BLTOUCH
        // A29 Z PROBE OFFESET SET
        case 29:
        {
          break;
        }

        // A30 assist leveling, the original function was canceled
        case 30:
        {
          // Parse for commands
          char *commandDataS = isCodeReceived(pRest, 'S');
          char *commandDataO = isCodeReceived(pRest, 'O');
          char *commandDataT = isCodeReceived(pRest, 'T');
          char *commandDataC = isCodeReceived(pRest, 'C');
          char *commandDataQ = isCodeReceived(pRest, 'Q');
          char *commandDataH = isCodeReceived(pRest, 'H');
          char *commandDataL = isCodeReceived(pRest, 'L');

          if(commandDataS != NULL) {
            DEBUG_ECHOLNPGM("TFT Entering level menue...");
          } else if(commandDataO != NULL) {
            DEBUG_ECHOLNPGM("TFT Leveling started and movint to front left...");
            queue.enqueue_now_P(PSTR("G91\nG1 Z10 F240\nG90\nG28\nG29\nG1 X20 Y20 F6000\nG1 Z0 F240"));
          } else if(commandDataT != NULL) {
            DEBUG_ECHOLNPGM("TFT Level checkpoint front right...");
            queue.enqueue_now_P(PSTR("G1 Z5 F240\nG1 X190 Y20 F6000\nG1 Z0 F240"));
          } else if(commandDataC != NULL) {
            DEBUG_ECHOLNPGM("TFT Level checkpoint back right...");
            queue.enqueue_now_P(PSTR("G1 Z5 F240\nG1 X190 Y190 F6000\nG1 Z0 F240"));
          } else if(commandDataQ != NULL) {
            DEBUG_ECHOLNPGM("TFT Level checkpoint back right...");
            queue.enqueue_now_P(PSTR("G1 Z5 F240\nG1 X190 Y20 F6000\nG1 Z0 F240"));
          } else if(commandDataH != NULL) {
            DEBUG_ECHOLNPGM("TFT Level check no heating...");
            //queue.enqueue_now_P(PSTR("... TBD ..."));
            write_to_lcd_NPGM("J22"); // J22 Test print done
            DEBUG_ECHOLNPGM("TFT Serial Debug: Leveling print test done... J22");
          } else if(commandDataL != NULL) {
            DEBUG_ECHOLNPGM("TFT Level check heating...");
            //queue.enqueue_now_P(PSTR("... TBD ..."));
            write_to_lcd_NPGM("J22"); // J22 Test print done
            DEBUG_ECHOLNPGM("TFT Serial Debug: Leveling print test with heating done... J22");
          }
          write_to_lcd_NPGM("OK");
          break;
        }

        // A31 zoffset
        case 31:
        {
          if((!planner.movesplanned()) && (AnycubicState != ANYCUBIC_STATE_SDPAUSE) && (AnycubicState != ANYCUBIC_STATE_SDOUTAGE)) {
            #if HAS_BED_PROBE
              char value[30];
              char *s_zoffset;

              // Parse for commands
              char *commandDataS = isCodeReceived(pRest, 'S');
              char *commandDataD = isCodeReceived(pRest, 'D');

              //if(commandDataS != NULL) {
                //write_to_lcd_PGM("A9V ");
                //write_to_lcd_PROTOCOL(ui8tostr3(uint8_t(NOZZLE_TO_PROBE_OFFSET[2] * 100.00 + 05)));
                //write_to_lcd_ENTER();
                //DEBUG_ECHOPGM("TFT sending current z-probe offset data... <");
                //DEBUG_ECHOPGM("A9V ");
                //DEBUG_ECHO(ui8tostr3(uint8_t(NOZZLE_TO_PROBE_OFFSET[2] * 100.00 + 0.5)));
                //DEBUG_ECHOLNPGM(">");
              //}
              if(commandDataD != NULL) {
                s_zoffset = (char*)ftostr3(float(strtod(commandDataD, NULL)) / 100.0);
                sprintf_P(value, PSTR("M851 Z"));
                strcat(value, s_zoffset);
                queue.enqueue_now_P(value); // Apply Z-Probe offset
                queue.enqueue_now_P(PSTR("M500")); // Save to EEPROM
              }
            #endif
          }
          write_to_lcd_ENTER();
          break;
        }

        // A32 clean leveling beep flag
        case 32:
        {
          // Parse for commands
          char *commandDataS = isCodeReceived(pRest, 'S');
          if(commandDataS != NULL) {
            DEBUG_ECHOLNPGM("TFT Level saving data...");
            queue.enqueue_now_P(PSTR("M500\nM420 S1\nG1 Z10 F240\nG1 X0 Y0 F6000"));
            write_to_lcd_NPGM("OK");
          }
          break;
        }
      #endif

      // A33 get version info
      case 33:
      {
        write_to_lcd_PGM("J33 ");
        write_to_lcd_NPGM(MSG_ANYCUBIC_VERSION);
        break;
      }

      // Command code UNKOWN
      default:
        DEBUG_ECHOLNPAIR("UNKNOWN COMMAND A", commandId);
        break;
    }

    if (commandId > 5) {
      DEBUG_ECHOLNPAIR("CALLED COMMAND A", commandId);
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
