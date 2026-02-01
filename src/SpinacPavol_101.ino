
// ------------------------------------------------------------------ //
//                    Timer switch with display                       //
// ------------------------------------------------------------------ //
// version: 1.1.001                                                    //
// author : (c) Martin Janček, mateusko.OAMDG@outlook.com              //
//                                                                    //
// The timer switches on the power supply for the set time.           //
// The times are stored in four presets and can be switched           //
// by briefly pressing the ON button.                                 //
// The time is set using the + and - buttons, and the MODE button     //
// is used to set the interval for adding and subtracting time.       //
// Press and hold the MODE button to save the time to memory.         //
// The active memory number is shown at the top right of the display. //
//                                                                    //
//                                                                    //
// The remaining time is shown on a 128x32 OLED display.              //
// When the time expires, the power relay switches off.               //
// Po vypršaní času sa relé napájania vypne                           //
// ------------------------------------------------------------------ //
// Control:                                                         //
//    ON/OFF - pressed LONG  =  ON or OFF                             //
//    PLUS   - pressed SHORT =  time + interval                       //
//    MINUS  - pressed SHORT =  time - interval                       //  
//    ON/OFF - pressed SHORT =  change preset                         //
//    MODE   - pressed SHORT =  change interval                       //
//    MODE   - pressed LONG  =  save current setting to EEPROM        //  
// ------------------------------------------------------------------ //

#include <avr/io.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <RealButton.h>
#include "classes.h"
#include "EEPROM.h"

#define PIN_BUTTON_ON_OFF   PIN_PA4  
#define PIN_BUTTON_PLUS     PIN_PA1
#define PIN_BUTTON_MINUS    PIN_PA2
#define PIN_BUTTON_MODE     PIN_PA3
#define PIN_SUPPLY          PIN_PA5
#define PIN_LED1            PIN_PA6

#define CLOCK_FREEZE_TIME 3 // [s]
#define SECURE_START_TIME 2000 // [ms]

// Dimesions of SettingsArray-s
#define ITEMS_OF_PRESETTIMES 4
#define ITEMS_OF_INTERVALS   2

// Preset TIMEs (see setup)
SettingsArray<int, ITEMS_OF_PRESETTIMES> presetTimes; // [s] objekt poľa s položkami typu unsigned long a size = ITEMS_OF_PRESETTIMES

// Preset intervals (see setup)
SettingsArray<int, ITEMS_OF_INTERVALS> intervals; // [s] objekt poľa s položkami typu int a size = 2

// Display Instantion
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C  u8g2(U8G2_R0, U8X8_PIN_NONE); // display SSD1306_128X32

// Hardware Buttons Instantions
RealButton buttonOn(PIN_BUTTON_ON_OFF);      // tlačidlo
RealButton buttonPlus(PIN_BUTTON_PLUS);         // tlačidlo
RealButton buttonMinus(PIN_BUTTON_MINUS);       // tlačidlo
RealButton buttonMode(PIN_BUTTON_MODE);         // tlačidlo

// LED Instantion
Led led1(PIN_LED1, HIGH, LOW);

// Main power supply relay Instantion
PowerTimer pt(PIN_SUPPLY);

// Display

// display callbacks
int getTimeCB() {
  return pt.getTime();
}

int getIndexCB() {
  return presetTimes.currentIndex();
}

int getIntervalCB() {
  return intervals.current();
}
 
Display display(&getTimeCB, &getIndexCB, &getIntervalCB);

// Auxiliary
unsigned long timer; // pomocný časovač
bool button_ready = false; // pomocný status tlačidla ON


// EEPROM CRC
unsigned long crcCompute(int length) {
  const unsigned long crc_table[16] = {
      0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
      0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
      0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
      0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    };

    unsigned long crc = ~0L;

    for (int index = 0 ; index < length; ++index) {
      crc = crc_table[(crc ^ EEPROM[index]) & 0x0f] ^ (crc >> 4);
      crc = crc_table[(crc ^ (EEPROM[index] >> 4)) & 0x0f] ^ (crc >> 4);
      crc = ~crc;
    }

    return crc;
}





/// @brief Do EEPROM sa uloží inštancia presetTimes a CRC
void saveToEEPROM() {
  presetTimes.setCurrent(pt.getTime());
  EEPROM.put(0, presetTimes);
  EEPROM.put(sizeof(presetTimes), crcCompute(sizeof(presetTimes))); //crc
}

bool loadFromEEPROM() {
  unsigned long saved_crc;
    
  EEPROM.get(0, presetTimes);
  EEPROM.get(sizeof(presetTimes), saved_crc);
  return saved_crc == crcCompute(sizeof(presetTimes)); //crc
}



/////// -------------------- S E T U P -------------------- ///////

void setup() {
  // nastav WDT periodu na ~1s (0x8) – protected write
  _PROTECTED_WRITE(WDT.CTRLA, 0x08);
  
  // SECURE OFF power supply relay 
  pinMode(PIN_SUPPLY, OUTPUT);
  digitalWrite(PIN_SUPPLY, LOW);
  
  timer = millis();

  u8g2.begin(); // inicializacia grafiky
  
  // Inicializacia presetTimes [s]
  if (!loadFromEEPROM()) {
    presetTimes[0] = 60ul; // 5min
    presetTimes[1] = 300ul; // 10min
    presetTimes[2] = 600ul; // 20 min
    presetTimes[3] = 1800ul; // 30 min
    presetTimes.setIndex(0);

    display.printText("Er EPR"); // EEPROM Read Error
    display.freeze(2000);
    display.doFreeze();
  }
  
  // Inicializacia intervals  [min] (time Plus/Minus interval)
  intervals[0] = 1; // 1 min
  intervals[1] = 5; // 5 min

  // inicializacia tlacidiel
  buttonOn.start();
  buttonPlus.start();
  buttonMinus.start();
  buttonMode.start();
  buttonMode.longTime = 2000ul; 

  led1.blink(200);

  if(millis() - timer < SECURE_START_TIME) delay_ds(1);

  // SECURE START
  pt.start(presetTimes.current()); 
  pt.freeze(5);
  // 
}


/////// -------------------- L O O P -------------------- ///////

void loop() {
  
  // Get and handle event

  if (!buttonOn.pressed() && !button_ready) {
      button_ready = true;
      buttonOn.start();
  }

// NEXT_PRESET 
  if (/*buttonOn.pressed() && buttonMode.onPress() || */ buttonOn.onClick()) {
    
    button_ready = false;
   
    presetTimes.next(true); // cyclic mode
    pt.start(presetTimes.current()); // start for current preset time
    pt.freeze(5);
    //display.freeze(2000);
    display.printTimeScreen();
    
  }

// RELAY OFF   
  else if (buttonOn.onLong() && button_ready) {
    
    display.printText("OFF");
    display.freeze(2000);
    pt.off();
  }

// TIME PLUS
  else if (buttonPlus.onPress()) {
    
    int interval = intervals.current() * 60;
    int t = pt.getTime();
    t = (t / interval + 1) * interval;

    if (t <  90 * 60) pt.start(t); 
    display.printTimeScreen(); 
    pt.freeze(5); 
  }

// TIME MINUS
  else if (buttonMinus.onPress()) {
    
     int interval = intervals.current() * 60;
    int t = pt.getTime();
    t = ((t - 1) / interval) * interval;
    if (t > 0) pt.start(t);
    display.printTimeScreen();
    pt.freeze(5);
  }

// CHANGE INTERVAL
  else if (buttonMode.onClick() && !buttonOn.pressed()) {
    
    intervals.next(true);
    display.freeze(2000);
    display.printInterval();
    pt.freeze(5);
  }

// SAVE TO EEPROM
  else if(buttonMode.onLong()) {
    
    saveToEEPROM();
    display.printText("SAVED");
    pt.freeze(5);
  }

  // Update Buttons 
  buttonOn.update();
  buttonPlus.update();
  buttonMinus.update();
  buttonMode.update();

  // LED update
  led1.update();

  // Update supply power relay
  pt.update();

  // Display update
  display.update();

  feedWatchdog();
}