#include "menu.h"
#include "PXX.h"
#include "CPPM.h"

// #include <avr/pgmspace.h>
#include <EEPROM.h>
//#include "U8glib.h"
#include <U8g2lib.h>
//#include <U8x8lib.h>

// Display selection; Note: after changing display may need to re-design menu;
//U8GLIB_SSD1306_64X48 u8g(U8G_I2C_OPT_NONE || U8G_I2C_OPT_NO_ACK || U8G_I2C_OPT_FAST);  // I2C / TWI 
#ifdef DEBUG // Allocate less display memory in DEBUG mode;
  U8G2_SSD1306_64X48_ER_1_HW_I2C u8g(U8G2_R0); // hardware // F - fastest, 1 - slowest, 2 - medium;
#else
  U8G2_SSD1306_64X48_ER_F_HW_I2C u8g(U8G2_R0); // hardware // F - fastest, 1 - slowest, 2 - medium;
#endif

uint8_t menu_current = 0;

boolean menuItemFlashing = false;

// Settings version;
const uint8_t CURRENT_VERSION = 0x10;

#define MODE_UNDEFINED 0
#define MODE_LOGO 1
#define MODE_SCREENSAVER 2
#define MODE_RADIO 3
#define MODE_MENU 4
#define MODE_MENU 5
#define MODE_CHANNELS 6
#define MODE_CHANNELS_RANGE 7
static byte menuMode = MODE_LOGO;

// Private variables;
#define KEY_NONE 0
#define KEY_NEXT 1
#define KEY_SELECT 2

#define CPPM_START 0
#define CPPM_LOST 1
#define CPPM_OBTAINED 2

static byte cppm_state = CPPM_START;
static byte old_cppm_state = CPPM_START;

bool cppmModeChanged(void);

#define RADIO_US_FCC  0 
#define RADIO_EU_LBT  1
#define RADIO_EU_FLEX 2
byte radioMode;

// PROGMEM enabled draw functions -----------------------
size_t getLength_F(const __FlashStringHelper *ifsh)
{
  PGM_P p = reinterpret_cast<PGM_P>(ifsh);
  size_t n = 0;
  while (1) {
    unsigned char c = pgm_read_byte(p++);
    if (c == 0) break;
    n++;
  }
  return n;
}

void drawStr_F(uint8_t x, uint8_t y, const __FlashStringHelper* s_P) {
  u8g.setCursor(x, y);
  u8g.print(s_P);
}

void drawStrRight_F(uint8_t x, uint8_t y, const __FlashStringHelper* s_P) {
  u8g.setCursor((uint8_t) u8g.getWidth()-x-getLength_F(s_P)*u8g.getStrWidth("W"), y);
  u8g.print(s_P);
}

void drawCentered_F(int y, const __FlashStringHelper* s_P) {
  u8g.setCursor((u8g.getWidth()-(getLength_F(s_P)+1)*u8g.getStrWidth("W"))/2, y);     
  u8g.print(s_P);
}

// Keyboard handling -----------------------------------
uint8_t peek(void) {
  if (digitalRead(PIN_KEY_NEXT) == LOW)
    return KEY_NEXT;
  else if (digitalRead(PIN_KEY_SELECT) == LOW)
    return KEY_SELECT;
  else 
    return KEY_NONE;  
}

uint8_t handleKeys(void) {
  static uint8_t key = KEY_NONE;
  static unsigned long pressMills=0;
  static uint8_t keyPrev = KEY_NONE;
  uint8_t c = peek();
  
  if (c != keyPrev) {
    keyPrev = c;
    pressMills = millis()+BOUNCE_TICK;  
  } else {
    if (millis() >= pressMills) {
      key = c;
    }    
  }
  return key;
}

#define MENU_LEFT 0
#define MENU_TOP 0

void drawMenuItem(uint8_t i, const __FlashStringHelper* s_P, const char * s1) {
    uint8_t y;
    uint8_t screen_width = (uint8_t) u8g.getWidth();
    uint8_t char_height = u8g.getFontAscent()-u8g.getFontDescent();
    y = char_height*i+MENU_TOP;
    u8g.setDrawColor(1);      
    if ( i == menu_current ) {
      u8g.drawBox(MENU_LEFT, y-1, screen_width-MENU_LEFT, char_height+1);
      u8g.setDrawColor(0);
    }
    if (s_P) {
      u8g.setCursor(MENU_LEFT+1, y-1);
      u8g.print(s_P);
    }
    if (s1) {
      u8g.drawStr(screen_width-u8g.getStrWidth(s1), y-1, s1);      
    }
}

const __FlashStringHelper* getPowerStr(void) {
  if (radioMode != RADIO_EU_LBT) { // RADIO_US_FCC || RADIO_EU_FLEX
    switch (PXX.getPower()) {
  case 0: return F("10 mW");
       break;
  case 1: return F("100 mW");
       break;
  case 2: return F("500 mW");
       break;
  case 3: return F("Auto < 1 W");
       break;
    }
  } else {
    switch (PXX.getPower()) {
  case 0: return F("25 mW 8ch");
       break;
  case 1: return F("25 mW");
       break;
  case 2: return F("200 mW");
       break;
  case 3: return F("500 mW");
       break;
    }    
  }
  return NULL;
}

const __FlashStringHelper* getBandStr(void) {
  switch(radioMode) {
    case RADIO_US_FCC: return F("US FCC 915MHz");
    case RADIO_EU_LBT: return F("EU LBT 868MHz");
    default:           return F("FLEX   868MHz"); // RADIO_EU_FLEX
  }
}

const __FlashStringHelper* getBandStrShort(void) {
  switch(radioMode) {
    case RADIO_US_FCC: return F("US 915");
    case RADIO_EU_LBT: return F("EU 868");
    default:           return F("FLEX 868"); // RADIO_EU_FLEX
  }
}

void prepareForDrawing(bool big_font) {
  u8g.setDrawColor(1);      
  u8g.setFont(big_font ? u8g_font_6x10 : u8g_font_5x8); 
  u8g.setFontRefHeightText();
  u8g.setFontPosTop();
}

void drawMenu(void) {
  prepareForDrawing(true);

#ifdef DEBUG
  Serial.print("drawMenu="); Serial.println(menu_current);
#endif  
  drawMenuItem(0, F("Radio Set."), NULL);
  drawMenuItem(1, F("RX Monitor"), NULL);
  drawMenuItem(2, F("RX Min/Max"), NULL);
}

static bool invalidValueFlashing = false;
static bool showChannels_Min = false;

int16_t getChannel_Raw_Min_Max(uint8_t channel) {
  int16_t* arr = menuMode == MODE_CHANNELS ? channels : showChannels_Min ? channelsMin : channelsMax;
  return arr[channel];
}

void drawChannelItems(uint8_t i, uint8_t channelLeft, uint8_t channelRight) {
  uint8_t screen_width = (uint8_t) u8g.getWidth();
  uint8_t char_height = u8g.getFontAscent()-u8g.getFontDescent()+1;
  uint8_t char_width = u8g.getStrWidth("W");
  char buf[7];
  int y = MENU_TOP+char_height*i;

  u8g.setDrawColor(1);      
  u8g.drawBox(MENU_LEFT, y-1, char_width+1, char_height+1);
  u8g.drawBox(MENU_LEFT+(screen_width+char_width)/2-1, y-1, char_width+2, char_height+1);

  u8g.setDrawColor(0);
  itoa(channelLeft+1, buf, 10);
  u8g.drawStr(MENU_LEFT, y, buf);

  itoa(channelRight+1, buf, 10);
  u8g.drawStr(MENU_LEFT+(screen_width+char_width)/2, y, buf); 

  u8g.setDrawColor(1);
  if (menuMode == MODE_CHANNELS || channelsMinMaxSet) {
    int16_t x = getChannel_Raw_Min_Max(channelLeft);
    if (menuMode == MODE_CHANNELS_RANGE || (channelValid(x) || invalidValueFlashing)) {
      itoa(x, buf, 10);
      u8g.drawStr(MENU_LEFT+char_width+2, y, buf); 
    }

    x = getChannel_Raw_Min_Max(channelRight);
    if (menuMode == MODE_CHANNELS_RANGE || (channelValid(x) || invalidValueFlashing)) {
      itoa(x, buf, 10);
      u8g.drawStr(screen_width-u8g.getStrWidth(buf), y, buf);
    }
  }
}

void drawChannels(void) {
  prepareForDrawing(true);

  if (timer1.isTriggered(TIMER_INVALID_FLASHING)) {
    invalidValueFlashing = !invalidValueFlashing;
  }
  
  if (timer1.isTriggered(TIMER_CHANNELS_MIN_MAX_FLIP)) {
    showChannels_Min = !showChannels_Min;
  }
  
  if (menuMode == MODE_CHANNELS && !cppmActive) {
    drawStr_F(MENU_LEFT, MENU_TOP, F("No CPPM")); 
    // Errs: NNN
    // Fail: 1
  } else {
    // CH 1 | CH 2
    // CH 3 | CH 4
    // CH 5 | CH 6
    // CH 7 | CH 8
    // Errs: NNN
    // Fail: 1
    drawChannelItems(0, 0, 1);
    drawChannelItems(1, 2, 3);
    drawChannelItems(2, 4, 5);
    drawChannelItems(3, 6, 7);
  }

  uint8_t screen_width = (uint8_t) u8g.getWidth();
  uint8_t char_height = u8g.getFontAscent()-u8g.getFontDescent()+1;
  uint8_t char_width = u8g.getStrWidth("W");

  int y = MENU_TOP+char_height*4;
  u8g.drawStr(MENU_LEFT, y, "ER"); 
  char buf[5];
  itoa(CPPM.getErrorCount(), buf, 10);
  u8g.drawStr(MENU_LEFT+char_width*2+4, y, buf);

  u8g.drawStr(MENU_LEFT+(screen_width+char_width)/2, y, 
    menuMode == MODE_CHANNELS ? "=" : showChannels_Min ? "<" : ">"); 

  itoa(CPPM.getFailReason(), buf, 10);
  int x = screen_width-u8g.getStrWidth(buf);
  u8g.drawStr(x, y, buf);      
  u8g.drawStr(x-char_width*2-3, y, "FL"); 
}

void drawMenuRadio(void) {
  prepareForDrawing(false);
  drawMenuItem(0, getBandStr(), NULL);

  char buf[5];

  itoa(PXX.getPower(), buf, 10);
  drawMenuItem(1, getPowerStr(), buf);
  
  itoa(PXX.getRxNum(), buf, 10);
  drawMenuItem(2, F("Receiver"), buf);
  
  drawMenuItem(3, F("TELEM "), PXX.getTelemetry() ? "Yes" : "No");
  drawMenuItem(4, F("S-Port "), PXX.getSPort() ? "Yes" : "No");
  drawMenuItem(5, !PXX.getModeRangeCheck() || menuItemFlashing ? F(">RANGE CHECK") : NULL, NULL);
  drawMenuItem(6, !PXX.getModeBind() || menuItemFlashing ? F(">BIND") : NULL, NULL);  
}

void drawScreenSaver() {
  prepareForDrawing(true);

  uint8_t screen_width = (uint8_t) u8g.getWidth();
  uint8_t char_height = u8g.getFontAscent()-u8g.getFontDescent()+1;
  int y = MENU_TOP;
  drawStr_F(MENU_LEFT, y, F("Batt:")); 
  char buf[5];
  dtostrf(battery1.getCurrVoltage(), 4, 1, buf);
  u8g.drawStr(screen_width-u8g.getStrWidth(buf), y, buf);
  y += char_height;

  drawStr_F(MENU_LEFT, y, F("Cells:")); 
  itoa(battery1.getNumCells(), buf, 10);
  u8g.drawStr(screen_width-u8g.getStrWidth(buf), y, buf);      
  y += char_height;

  const __FlashStringHelper* s;
  switch (cppm_state) {
  case CPPM_START: s = F("CPPM: Wait");
       break;
  case CPPM_OBTAINED: s = F("CPPM: OK");
       break;
  default: s = F("CPPM: Lost"); // CPPM_LOST
       break;
  }
  drawStr_F(MENU_LEFT, y, s); 
  if (CPPM.getFailReason()) {
    itoa(CPPM.getFailReason(), buf, 10);
    u8g.drawStr(screen_width-u8g.getStrWidth(buf), y, buf);          
  }
  y += char_height;
  
  drawStr_F(MENU_LEFT, y, getBandStrShort()); 
  y += char_height;

  drawStr_F(MENU_LEFT, y, getPowerStr()); 
}

void drawLogo(void) {
  prepareForDrawing(true);

  uint8_t char_height = u8g.getFontAscent()-u8g.getFontDescent();

  u8g.setDrawColor(1);      
  u8g.drawBox(MENU_LEFT, MENU_TOP, u8g.getWidth()-MENU_LEFT, char_height*2+1);

  u8g.setDrawColor(0);
  drawCentered_F(MENU_TOP+char_height*0, F(VERSION));   
  drawCentered_F(MENU_TOP+char_height*1, F("R9M relay"));

  u8g.setDrawColor(1);      
  uint8_t y = MENU_TOP+char_height*2+(MENU_TOP+char_height/2);
  drawCentered_F(y, F("(c) 2019"));   
  drawCentered_F(y+char_height*1, F("by Andrey"));
  drawCentered_F(y+char_height*2, F("Prikupets"));
}

void write_settings(void);
void adjust_settings(void);
void adjust_timer(void);

#define MENU_ITEMS_RADIO 7

void updateMenuRadio(uint8_t key) {  
  switch ( key ) {
    case KEY_NEXT:
      beeper1.play(&SEQ_KEY_NEXT);
      menu_current = menu_current >= MENU_ITEMS_RADIO-1 ? 0 : menu_current+1;
      PXX.setModeRangeCheck(false); // Turn off BIND and RANGECHECK if moved in menu;
      PXX.setModeBind(false);
      adjust_timer();
      break;
    case KEY_SELECT:
      beeper1.play(&SEQ_KEY_SELECT);
      switch(menu_current) {
        case 0: // radioMode = radioMode >= RADIO_EU_FLEX ? RADIO_US_FCC : radioMode++;
                radioMode = radioMode == RADIO_US_FCC ? RADIO_EU_LBT : radioMode == RADIO_EU_LBT ? RADIO_EU_FLEX : RADIO_US_FCC;
                adjust_settings(); break;
        case 1: PXX.setPower(PXX.getPower() >= 3 ? 0 : PXX.getPower()+1); 
                adjust_settings(); break;
        case 2: PXX.setRxNum(PXX.getRxNum() >= MAX_RX_NUM ? 0 : PXX.getRxNum()+1); break;
        case 3: PXX.setTelemetry(!PXX.getTelemetry()); break;
        case 4: PXX.setSPort(!PXX.getSPort()); break;
        case 5: PXX.setModeRangeCheck(!PXX.getModeRangeCheck()); 
                adjust_timer(); break;
        case 6: PXX.setModeBind(!PXX.getModeBind()); 
                adjust_timer(); break;
      }
      if (menu_current <= 4) {
        write_settings();
      }
      break;
  }
}

#define MENU_ITEMS 3

uint8_t updateMenu(uint8_t key) {  
  switch ( key ) {
    case KEY_NEXT:
      beeper1.play(&SEQ_KEY_NEXT);
      menu_current = menu_current >= MENU_ITEMS-1 ? 0 : menu_current+1;
#ifdef DEBUG
  Serial.print("updateMenu="); Serial.println(menu_current);
#endif  
      break;
    case KEY_SELECT:
      beeper1.play(&SEQ_KEY_SELECT);
      switch(menu_current) {
        case 0: return MODE_RADIO;
        case 1: return MODE_CHANNELS;
        case 2: return MODE_CHANNELS_RANGE;
      }
      break;
  }
  return MODE_UNDEFINED;
}

void adjust_timer(void) {
  if (PXX.getModeRangeCheck() || PXX.getModeBind()) {
    timer1.resume(TIMER_MENU_FLASHING);
    timer1.suspend(TIMER_SCREENSAVER);
  } else {
    timer1.suspend(TIMER_MENU_FLASHING);  
    timer1.resume(TIMER_SCREENSAVER);
  }  
}

void adjust_settings(void) {
  switch (radioMode) {
    case RADIO_US_FCC: PXX.setCountry(PXX_COUNTRY_US); PXX.setEUPlus(false); break;   
    case RADIO_EU_LBT: PXX.setCountry(PXX_COUNTRY_EU); PXX.setEUPlus(false); break;   
    default:           PXX.setCountry(PXX_COUNTRY_EU); PXX.setEUPlus(true); // RADIO_EU_FLEX
  }
  bool send8Ch = (radioMode == RADIO_EU_LBT) && (PXX.getPower() == 0); // 25mW 8Ch;
  PXX.setSend16Ch (!send8Ch);
}

bool update_menu_flash_timer(void) {
  if (timer1.isTriggered(TIMER_MENU_FLASHING)) {
    menuItemFlashing = !menuItemFlashing;  
    return true;
  }
  return false;
}

void read_settings(void) {
  uint8_t b;
  b = EEPROM.read(0);
  if (b != CURRENT_VERSION)
    return;

  radioMode = EEPROM.read(1);
  b = EEPROM.read(2);
  PXX.setPower(b > 3 ? 0 : b);
  adjust_settings();  

  b = EEPROM.read(3);
  PXX.setRxNum(b > MAX_RX_NUM ? 0 : b);

  PXX.setTelemetry(EEPROM.read(4) != 0);
  PXX.setSPort(EEPROM.read(5) != 0);
}

void init_settings(void) {
  PXX.setModeBind(false);
  PXX.setModeRangeCheck(false);

  PXX.setPower(0);
  radioMode = RADIO_US_FCC;
  adjust_settings(); // setCountry, setEUPlus;
  
  PXX.setRxNum(0);
  PXX.setTelemetry(true);
  PXX.setSPort(true);
  PXX.setProto(PXX_PROTO_X16);
}

void write_settings(void) {
  EEPROM.write(0, CURRENT_VERSION);
  EEPROM.write(1, radioMode);
  EEPROM.write(2, PXX.getPower());
  EEPROM.write(3, PXX.getRxNum());
  EEPROM.write(4, PXX.getTelemetry() ? 1 : 0);
  EEPROM.write(5, PXX.getSPort() ? 1 : 0);
}

void menuSetup(void)
{
  u8g.begin();
  pinMode(PIN_KEY_NEXT, INPUT_PULLUP);           // set pin to input with pullup
  pinMode(PIN_KEY_SELECT, INPUT_PULLUP);           // set pin to input with pullup
  init_settings();
  read_settings();
  adjust_timer();
}

void showLogo(void) {
    menuMode = MODE_LOGO;
    u8g.firstPage();
    do  {
      drawLogo();
    } while( u8g.nextPage() );
}

void showScreenSaver(void) {
    if (menuMode != MODE_SCREENSAVER) {
        timer1.suspend(TIMER_SCREENSAVER);
        menuMode = MODE_SCREENSAVER;
    }
    u8g.firstPage();
    do  {
      drawScreenSaver();
    } while( u8g.nextPage() );  
}

void showMenuRadio() {
    if (menuMode != MODE_RADIO) {
        timer1.resume(TIMER_SCREENSAVER);
        menuMode = MODE_RADIO;
    }
    u8g.firstPage();
    do  {
      drawMenuRadio();
    } while( u8g.nextPage() );  
}

void showMenu() {
    if (menuMode != MODE_MENU) {
        timer1.resume(TIMER_SCREENSAVER);
        menuMode = MODE_MENU;
    }
    u8g.firstPage();
    do  {
      drawMenu();
    } while( u8g.nextPage() );  
}

void showChannels(uint8_t mode) {
    if (menuMode != mode) {
        timer1.suspend(TIMER_SCREENSAVER);
        menuMode = mode;
    }
    u8g.firstPage();
    do  {
      drawChannels();
    } while( u8g.nextPage() );  
}

void handleBeeps(void) {
  if (timer1.isTriggered(TIMER_MODE_SOUND)) {
    if (PXX.getModeRangeCheck()) {
      beeper1.play(&SEQ_MODE_RANGE_CHECK);
    } else
    if (PXX.getModeBind()) {
      beeper1.play(&SEQ_MODE_BIND);
    }
  }
}

void menuLoop(void)
{
  byte refreshMenuMode = MODE_UNDEFINED;

  static uint8_t last_key_code = KEY_NONE;
  uint8_t key = handleKeys();
  if (key != KEY_NONE && last_key_code != key) {
    timer1.resetNotSuspended(TIMER_SCREENSAVER); // Reset non-suspended Screensaver timer if any key pressed;
    if (menuMode == MODE_MENU) { // If key is pressed in Main menu, get the new mode; if submenu selected, reset menu line;
        refreshMenuMode = updateMenu(key);
        if (refreshMenuMode != MODE_UNDEFINED) {
#ifdef DEBUG
  Serial.print("menuLoop1=0"); Serial.println(refreshMenuMode);
#endif  
          menu_current = 0; // Enter submenu and reset position to 1st menu item;
        } else {
          refreshMenuMode = MODE_MENU; // Refresh Main menu to update navigation;
        }
    } else
    if (menuMode == MODE_RADIO) { // If key is pressed in Radio menu, update it;
        updateMenuRadio(key);
        refreshMenuMode = MODE_RADIO; // Refresh Radio menu to update navigation;
    } else
    if (menuMode == MODE_CHANNELS || menuMode == MODE_CHANNELS_RANGE) { // Exit from Channels menu by a key;
        beeper1.play(&SEQ_KEY_SELECT);
        refreshMenuMode = MODE_MENU; // Show Main menu;
    } else
    if (menuMode == MODE_SCREENSAVER) { // If key is pressed in Screensaver mode, select Main menu, reset menu line;
        beeper1.play(&SEQ_KEY_SELECT);
        refreshMenuMode = MODE_MENU; // Show Main menu;
#ifdef DEBUG
  Serial.print("menuLoop2=0"); Serial.println(refreshMenuMode);
#endif  
        menu_current = 0; // Enter Main menu and reset position to 1st menu item;
    }
  } else {
    if (menuMode != MODE_SCREENSAVER && 
        timer1.isTriggered(TIMER_SCREENSAVER)) { // If in some menu Screensaver timeout is triggered, return to Screensaver mode;
        refreshMenuMode = MODE_SCREENSAVER;
    }
  }
  last_key_code = key;

  // Check for additional menu refreshes if something is flashing or displayed data updated; 
  switch(menuMode) {
    case MODE_RADIO:
      // Beeps if needed in Range Check or Bind modes;
      handleBeeps();
      // In Radio menu refresh in flashing in Range Check or Bind modes need it; 
      if (update_menu_flash_timer() && (PXX.getModeRangeCheck() || PXX.getModeBind())) {
        refreshMenuMode = menuMode;
      }
      break;
    case MODE_SCREENSAVER:
      // Refresh Screensaver if CPPM lost/obtained;
      if (cppmModeChanged()) {
          refreshMenuMode = menuMode;
      }
      // Refresh Screensaver if battery voltage changed (with the given update period);
      if (timer1.isTriggered(TIMER_BATTERY_SCREEN)) {
        if (battery1.isVoltageChanged()) {
          refreshMenuMode = menuMode;
        }
      }
      break;
    case MODE_CHANNELS:
    case MODE_CHANNELS_RANGE:
      // Refresh Channels with the given update period;
      if (timer1.isTriggered(TIMER_CHANNELS_SCREEN)) {
        refreshMenuMode = menuMode;
      }
      break;
  }

  // Redraw screens if resfresh required;
  switch(refreshMenuMode) {
    case MODE_RADIO: 
      showMenuRadio();
      break;
    case MODE_SCREENSAVER:
      showScreenSaver();
      break;
    case MODE_MENU:
      showMenu();
      break;
    case MODE_CHANNELS:
    case MODE_CHANNELS_RANGE:
      showChannels(refreshMenuMode);
      break;
  }
}

void setCPPM_Start(void) {
#ifdef DEBUG
  Serial.println("CPPM.Start");
#endif
    cppm_state = CPPM_START;
    timer1.suspend(TIMER_NO_CPPM);
}

void setCPPM_Obtained(void) {
    if (cppm_state == CPPM_LOST) {
#ifdef DEBUG
  Serial.println("CPPM.Lost->Got");
#endif
      beeper1.stop(&SEQ_MODE_NO_CPPM);
      beeper1.stop(&SEQ_MODE_CPPM_LOST);
      beeper1.play(&SEQ_MODE_GOT_CPPM);
      timer1.suspend(TIMER_NO_CPPM);
    }
#ifdef DEBUG
    if (cppm_state == CPPM_START) {
  Serial.println("CPPM.Start->Got");
    }
#endif
    cppm_state = CPPM_OBTAINED;  
}

void setCPPM_Lost(void) {
    if (cppm_state == CPPM_OBTAINED) {
#ifdef DEBUG
  Serial.println("CPPM.Got->Lost");
#endif
      beeper1.stop(&SEQ_MODE_GOT_CPPM);
      beeper1.play(&SEQ_MODE_CPPM_LOST);
      timer1.resume(TIMER_NO_CPPM);
      cppm_state = CPPM_LOST;  
    } else
    if (cppm_state == CPPM_LOST) {
      if (timer1.isTriggered(TIMER_NO_CPPM)) {
#ifdef DEBUG
  Serial.println("CPPM.Lost.Seq");
#endif
        beeper1.play(&SEQ_MODE_NO_CPPM);
      }
    }
}

bool cppmModeChanged(void) {
    if (old_cppm_state != cppm_state) {
      old_cppm_state = cppm_state;
      return true;
    }
    return false;
}
