/*****************
 * Basis control for the SKAARHOJ C100 models
 * This example is programmed for ATEM 1M/E versions
 *
 * This example also uses a number of custom libraries which you must install first. 
 * Search for "#include" in this file to find the libraries. Then download the libraries from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
 * Works with Ethernet enabled arduino devices (Arduino Mega with Ethernet shield preferred)
 * Make sure to configure IP and addresses! Look for "<= SETUP" in the code below!
 * 
 * - kasper
 */
 
 /* General TODO:
 - Encoders for sure... (should use interrupt pins) -> more responsive.
 - Better handling of menu generation, comparison of items. Understand that code!!
 - Strings, PROGMEM etc. -> Save memory, what's the memory usage anyway?
 */
 
 
 
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>

#include <MenuBackend.h>  // Library for menu navigation. Must (for some reason) be included early! Otherwise compilation chokes.


// MAC address and IP address for this *particular* Ethernet Shield!
// MAC address is printed on the shield
// IP address is an available address you choose on your subnet where the switcher is also present:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x21, 0x16 };      // <= SETUP!
IPAddress ip(192, 168, 0, 20);              // <= SETUP!






// Include ATEM library and make an instance:
#include <ATEM.h>
ATEM AtemSwitcher;

// All related to library "SkaarhojBI8", which controls the buttons:
#include "Wire.h"
#include "MCP23017.h"
#include "PCA9685.h"
#include "SkaarhojBI8.h"
SkaarhojBI8 inputSelect;
SkaarhojBI8 cmdSelect;

// All related to library "SkaarhojUtils". Used for rotary encoder and "T-bar" potentiometer:
#include "SkaarhojUtils.h"
SkaarhojUtils utils;






// LCD Functions for the serial display:
#include <SoftwareSerial.h>
#define txPin 2

SoftwareSerial LCD = SoftwareSerial(0, txPin);
// Since the LCD does not send data back to the Arduino, we should only define the txPin
const int LCDdelay=1;  // 10 is conservative, 2 actually works

// wbp: goto with row & column
void lcdPosition(int row, int col) {
  LCD.write(0xFE);   //command flag
  LCD.write((col + row*64 + 128));    //position 
  AtemSwitcher.delay(LCDdelay);
}
void clearLCD(){
  LCD.write(0xFE);   //command flag
  LCD.write(0x01);   //clear command.
  AtemSwitcher.delay(LCDdelay);
}
void backlightOn() {  //turns on the backlight
  LCD.write(0x7C);   //command flag for backlight stuff
  LCD.write(157);    //light level.
  AtemSwitcher.delay(LCDdelay);
}
void backlightOff(){  //turns off the backlight
  LCD.write(0x7C);   //command flag for backlight stuff
  LCD.write(128);     //light level for off.
  AtemSwitcher.delay(LCDdelay);
}
void serCommand(){   //a general function to call the command flag for issuing all other commands   
  LCD.write(0xFE);
}





uint8_t userButtonMode = 0;  // 0-3
uint8_t setMenuValues = 0;  //


//**************************
// MENU Items and Controls
// ************************
// Configuration of the menu items and hierarchi plus call-back functions:
MenuBackend menu = MenuBackend(menuUseEvent,menuChangeEvent);
  // Beneath is list of menu items needed to build the menu
  MenuItem menu_UP1       = MenuItem(menu, "< Exit", 1);
  MenuItem menu_mediab1   = MenuItem(menu, "Media Bank 1", 1);    // On Change: Show selected item/Value (2nd encoder rotates). On Use: N/A
  MenuItem menu_mediab2   = MenuItem(menu, "Media Bank 2", 1);    // (As Media Bank 1)
  MenuItem menu_userbut   = MenuItem(menu, "User Buttons", 1);    // On Change: N/A. On Use: Show active configuration
    MenuItem menu_usrcfg1 = MenuItem(menu, "DSK1     VGA+PIPAUTO         PIP", 2);  // On Change: Use it (and se as default)! On Use: Exit
    MenuItem menu_usrcfg2 = MenuItem(menu, "DSK1        DSK2AUTO         PIP", 2);  // (As above)
    MenuItem menu_usrcfg3 = MenuItem(menu, "KEY1        KEY2KEY3        KEY4", 2);  // (As above)
    MenuItem menu_usrcfg4 = MenuItem(menu, "COLOR1    COLOR2BLACK       BARS", 2);  // (As above)
  MenuItem menu_trans     = MenuItem(menu, "Transitions", 1);    // (As User Buttons)
    MenuItem menu_trtype  = MenuItem(menu, "Type", 2);  // (As Media Bank 1)
    MenuItem menu_trtime  = MenuItem(menu, "Trans. Time", 2);  // (As Media Bank 1)
  MenuItem menu_ftb       = MenuItem(menu, "Fade To Black", 1);
    MenuItem menu_ftbtime = MenuItem(menu, "FTB Time", 2);
    MenuItem menu_ftbexec = MenuItem(menu, "Do Fade to Black", 2);
  MenuItem menu_aux1      = MenuItem(menu, "AUX 1", 1);    // (As Media Bank 1)
  MenuItem menu_aux2      = MenuItem(menu, "AUX 2", 1);    // (As Media Bank 1)
  MenuItem menu_aux3      = MenuItem(menu, "AUX 3", 1);    // (As Media Bank 1)
  MenuItem menu_network   = MenuItem(menu, "Network", 1);
    MenuItem menu_ownIP   = MenuItem(menu, "Own IP", 2);
    MenuItem menu_AtemIP  = MenuItem(menu, "ATEM IP", 2);
  MenuItem menu_UP2       = MenuItem(menu, "< Exit", 1);

// This function builds the menu and connects the correct items together
void menuSetup()
{
  Serial.println("Setting up menu...");

  // Add first to the menu root:
  menu.getRoot().add(menu_mediab1); 

    // Setup the rest of menu items on level 1:
    menu_mediab1.addBefore(menu_UP1);
    menu_mediab1.addAfter(menu_mediab2);
    menu_mediab2.addAfter(menu_userbut);
    menu_userbut.addAfter(menu_trans);
    menu_trans.addAfter(menu_ftb);
    menu_ftb.addAfter(menu_aux1);
    menu_aux1.addAfter(menu_aux2);
    menu_aux2.addAfter(menu_aux3);
    menu_aux3.addAfter(menu_network);
    menu_network.addAfter(menu_UP2);

      // Set up user button menu:
    menu_usrcfg1.addAfter(menu_usrcfg2);  // Chain subitems...
    menu_usrcfg2.addAfter(menu_usrcfg3);
    menu_usrcfg3.addAfter(menu_usrcfg4);
    menu_usrcfg2.addLeft(menu_userbut);  // Add parent item - starting with number 2...
    menu_usrcfg3.addLeft(menu_userbut);
    menu_usrcfg4.addLeft(menu_userbut);
    menu_userbut.addRight(menu_usrcfg1);     // Add the submenu to the parent - this will also see "left" for "menu_usercfg1"

      // Set up transition menu:
    menu_trtype.addAfter(menu_trtime);    // Chain subitems...
    menu_trtime.addLeft(menu_trans);      // Add parent item
    menu_trans.addRight(menu_trtype);     // Add the submenu to the parent - this will also see "left" for "menu_trtype"

      // Set up fade-to-black menu:
    menu_ftbtime.addAfter(menu_ftbexec);    // Chain subitems...
    menu_ftbexec.addLeft(menu_ftb);      // Add parent item
    menu_ftb.addRight(menu_ftbtime);     // Add the submenu to the parent

      // Set up network menu:
    menu_ownIP.addAfter(menu_AtemIP);    // Chain subitems...
    menu_AtemIP.addLeft(menu_network);      // Add parent item
    menu_network.addRight(menu_ownIP);     // Add the submenu to the parent
}

/*
  Here all use events are handled. Mainly these are used to navigate in to and out of menu items with the encoder button.
*/
void menuUseEvent(MenuUseEvent used)
{
  setMenuValues=0;

  if (used.item.getName()=="MenuRoot")  {
     menu.moveDown(); 
  }
  
    // Exit in upper level:
  if (used.item.isEqual(menu_UP1) || used.item.isEqual(menu_UP2))  {
    menu.toRoot(); 
  }

    // This will set the selected element as default when entering the menu again.
    // PS: I don't know why I needed to put the "*" before "used.item.getLeft()" It was a lucky guess, or...?
  if (used.item.getLeft())  {
    used.item.addLeft(*used.item.getLeft());
  }
  
    // Using an element moves left or right depending on where there are elements.
    // This works fine for a two level menu like this one.
  if((bool)used.item.getRight())  {
    menu.moveRight();
  } else {
    menu.moveLeft();
  }
}

/*
  Here we get a notification whenever the user changes the menu
  That is, when the menu is navigated
*/
void menuChangeEvent(MenuChangeEvent changed)
{
  setMenuValues=0;
  
  if (changed.to.getName()=="MenuRoot")  {
      // Show default text.... status whatever....
    clearLCD();
    lcdPosition(0,0);
    LCD.print("    SKAARHOJ     1UCTRL100SDXL");    
    setMenuValues=0;
  } else {
      // Show the item name in upper line:
    lcdPosition(0,0);
    LCD.print(changed.to.getName());
    for(int i=strlen(changed.to.getName()); i<16; i++)  {
      LCD.print(" ");
    }
    
      // If there are no menu items to the right, we assume its a value change:
    if (!(bool)changed.to.getRight())  {
        if (!menu_userbut.isEqual(*changed.to.getLeft()))  {  // If it is not the special "User Button" selection, show the value and set a flag for Encoder 1 to operate (see ....)
          lcdPosition(1,0);
          LCD.print("                ");  // Clear the line...
        }
        
          // Make settings as a consequence of menu selection:
        if (changed.to.getName() == menu_usrcfg1.getName())  { userButtonMode=0; }
        if (changed.to.getName() == menu_usrcfg2.getName())  { userButtonMode=1; }
        if (changed.to.getName() == menu_usrcfg3.getName())  { userButtonMode=2; }
        if (changed.to.getName() == menu_usrcfg4.getName())  { userButtonMode=3; }
        if (changed.to.getName() == menu_mediab1.getName())  { setMenuValues=1;  }
        if (changed.to.getName() == menu_mediab2.getName())  { setMenuValues=2;  }
        if (changed.to.getName() == menu_aux1.getName())  { setMenuValues=3;  }
        if (changed.to.getName() == menu_aux2.getName())  { setMenuValues=4;  }
        if (changed.to.getName() == menu_aux3.getName())  { setMenuValues=5;  }
        if (changed.to.getName() == menu_trtype.getName())  { setMenuValues=10;  }
        if (changed.to.getName() == menu_trtime.getName())  { setMenuValues=11;  }
        if (changed.to.getName() == menu_ftbtime.getName())  { setMenuValues=20;  }
        if (changed.to.getName() == menu_ftbexec.getName())  { setMenuValues=21;  }
        if (changed.to.getName() == menu_ownIP.getName())  { setMenuValues=30;  }
        if (changed.to.getName() == menu_AtemIP.getName())  { setMenuValues=31;  }
          // TODO: I HAVE to find another way to match the items here because two items with the same name will choke!!
        
        
    } else {  // Just clear the displays second line if there are items to the right in the menu:
      lcdPosition(0,15);
      LCD.print(">                ");  // Arrow + Clear the line...
    }
  }
}

















void setup() { 
  
  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  

  // Start up LCD:
  pinMode(txPin, OUTPUT);
  LCD.begin(9600);
  clearLCD();
  lcdPosition(0,0);
  LCD.print("    SKAARHOJ          C100     ");
  backlightOn();
  
  
    // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin(); 
  
    // Set up the SkaarhojBI8 boards:
  inputSelect.begin(0,false);
  cmdSelect.begin(1,false);
  
  inputSelect.setDefaultColor(0);  // Off by default
  cmdSelect.setDefaultColor(0);  // Off by default
  
  inputSelect.testSequence();
  cmdSelect.testSequence();
  
  // Initializing the slider:
  utils.uniDirectionalSlider_init();
  
  // Initializing menu related:
  utils.encoders_init();
  menuSetup();


  // Connect to an ATEM switcher on this address and using this local port:
  // The port number is chosen randomly among high numbers.
  clearLCD();
  lcdPosition(0,0);
  LCD.print("Connecting to:   192.168.0.50");
  
  AtemSwitcher.begin(IPAddress(192, 168, 0, 50), 56417);    // <= SETUP!
  AtemSwitcher.serialOutput(true);
  AtemSwitcher.connect();
}






// These variables are used to track state, for instance when the VGA+PIP button has been pushed.
bool preVGA_active = false;
bool preVGA_UpstreamkeyerStatus = false;
int preVGA_programInput = 0;
bool AtemOnline = false;



void loop() {

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();
  menuNavigation();
  menuValues();
  
  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
      if (AtemOnline)  {
        AtemOnline = false;

       clearLCD();
       lcdPosition(0,0);
       LCD.print("Connection Lost!Reconnecting...");

        inputSelect.setDefaultColor(0);  // Off by default
        cmdSelect.setDefaultColor(0);  // Off by default
        inputSelect.setButtonColorsToDefault();
        cmdSelect.setButtonColorsToDefault();
      }
     
     AtemSwitcher.connect();
  }

    // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;

      clearLCD();
      lcdPosition(0,0);
      LCD.print("Connected");
      lcdPosition(1,0);
      LCD.print(AtemSwitcher._ATEM_pin);
      lcdPosition(1,11);
      LCD.print(" v .");
      lcdPosition(1,13);
      LCD.print(AtemSwitcher._ATEM_ver_m);
      lcdPosition(1,15);
      LCD.print(AtemSwitcher._ATEM_ver_l);

      inputSelect.setDefaultColor(5);  // Dimmed by default
      cmdSelect.setDefaultColor(5);  // Dimmed by default
      inputSelect.setButtonColorsToDefault();
      cmdSelect.setButtonColorsToDefault();
    }
    
    
    setButtonColors();
    AtemSwitcher.runLoop();  // Call here and there...
    
    readingButtonsAndSendingCommands();
    AtemSwitcher.runLoop();  // Call here and there...
  }
}




// Navigation for the main menu:
void menuNavigation() {
  int encValue = utils.encoders_state(0,1000);
  switch(encValue)  {
     case 1:
       menu.moveDown();
     break; 
     case -1:
       menu.moveUp();
     break; 
     default:
       if (encValue>2)  {
         if (encValue<1000)  {
           menu.use();
         } else {
           menu.toRoot();
         }
       }
     break;
  } 
}

uint8_t prev_setMenuValues = 0;
void menuValues()  {
      int sVal;
      int encValue = utils.encoders_state(1,1000);
      
      switch(setMenuValues)  {
        case 1:  // Media bank 1 selector:
        case 2:  // Media bank 2 selector:
          sVal = AtemSwitcher.getMediaPlayerStill(setMenuValues);

          if (encValue==1 || encValue==-1)  {
            sVal+=encValue;
            if (sVal>32) sVal=1;
            if (sVal<1) sVal=32;
            AtemSwitcher.mediaPlayerSelectSource(setMenuValues, false, sVal) ;
          }
          lcdPosition(1,0);
          LCD.print("Still ");
          menuValues_printValue(sVal,6,3);
        break;
        case 3:  // AUX 1
        case 4:  // AUX 2
        case 5:  // AUX 3
          sVal = AtemSwitcher.getAuxState(setMenuValues-2);
          if (encValue==1 || encValue==-1)  {
            sVal+=encValue;
            if (sVal>19) sVal=0;
            if (sVal<0) sVal=19;
            AtemSwitcher.changeAuxState(setMenuValues-2, sVal);
            menuValues_clearValueLine();
          }
          lcdPosition(1,0);
          menuValues_printSource(sVal);
        break;
        
        case 10:  // Transition: Type
          sVal = AtemSwitcher.getTransitionType();
          if (encValue==1 || encValue==-1)  {
            sVal+=encValue;
            if (sVal>4) sVal=0;
            if (sVal<0) sVal=4;
            AtemSwitcher.changeTransitionType(sVal);
            menuValues_clearValueLine();
          }
          lcdPosition(1,0);
          menuValues_printTrType(sVal);
        break;
        case 11:  // Transition: Time
          sVal = AtemSwitcher.getTransitionMixTime();
          if (encValue==1 || encValue==-1)  {
            sVal+=encValue;
            if (sVal>0xFA) sVal=1;
            if (sVal<1) sVal=0xFA;
            AtemSwitcher.changeTransitionMixTime(sVal);
            menuValues_clearValueLine();
          }
          lcdPosition(1,0);
          LCD.print("Frames: ");
          menuValues_printValue(sVal,8,3);
        break;
        case 20:  // Fade-to-black: Time
          sVal = AtemSwitcher.getFadeToBlackTime();
          if (encValue==1 || encValue==-1)  {
            sVal+=encValue;
            if (sVal>0xFA) sVal=1;
            if (sVal<1) sVal=0xFA;
            AtemSwitcher.changeFadeToBlackTime(sVal);
            menuValues_clearValueLine();
          }
          lcdPosition(1,0);
          LCD.print("Frames: ");
          menuValues_printValue(sVal,8,3);
        break;
        case 21:  // Fade-to-black: Execute
          lcdPosition(1,0);
          LCD.print("Press to execute");
          if (encValue>2 && encValue <1000)  {
             AtemSwitcher.fadeToBlackActivate(); 
          }
        break;
        case 30:  // Own IP
        break;
        case 31:  // Atem IP
        break;

        default:
        break;        
      }
}
void menuValues_clearValueLine()  {
          lcdPosition(1,0);
          LCD.print("                ");
}
void menuValues_printValue(int number, uint8_t pos, uint8_t padding)  {
          lcdPosition(1,pos);
          LCD.print(number);
          for(int i=String(number).length(); i<padding; i++)  {
            LCD.print(" ");
          }
}
void menuValues_printSource(int sVal)  {
   switch(sVal)  {
       case 0: LCD.print("Black"); break;
       case 1: LCD.print("Camera 1"); break;
       case 2: LCD.print("Camera 2"); break;
       case 3: LCD.print("Camera 3"); break;
       case 4: LCD.print("Camera 4"); break;
       case 5: LCD.print("Camera 5"); break;
       case 6: LCD.print("Camera 6"); break;
       case 7: LCD.print("Camera 7"); break;
       case 8: LCD.print("Camera 8"); break;
       case 9: LCD.print("Color Bars"); break;
       case 10: LCD.print("Color 1"); break;
       case 11: LCD.print("Color 2"); break;
       case 12: LCD.print("Media Player 1"); break;
       case 13: LCD.print("Media Play.1 key"); break;
       case 14: LCD.print("Media Player 2"); break;
       case 15: LCD.print("Media Play.2 key"); break;
       case 16: LCD.print("Program"); break;
       case 17: LCD.print("Preview"); break;
       case 18: LCD.print("Clean Feed 1"); break;
       case 19: LCD.print("Clean Feed 2"); break;
       default: LCD.print("N/A"); break;
   } 
}
void menuValues_printTrType(int sVal)  {
   switch(sVal)  {
       case 0: LCD.print("Mix"); break;
       case 1: LCD.print("Dip"); break;
       case 2: LCD.print("Wipe"); break;
       case 3: LCD.print("DVE"); break;
       case 4: LCD.print("Sting"); break;
       default: LCD.print("N/A"); break;
   } 
}


void setButtonColors()  {
    // Setting colors of input select buttons:
    for (uint8_t i=1;i<=8;i++)  {
      uint8_t idx = i>4 ? i-4 : i+4;  // Mirroring because of buttons on PCB
      if (AtemSwitcher.getProgramTally(i))  {
        inputSelect.setButtonColor(idx, 2);
      } else if (AtemSwitcher.getPreviewTally(i))  {
        inputSelect.setButtonColor(idx, 3);
      } else {
        inputSelect.setButtonColor(idx, 5);   
      }
    }
    
      // The user button mode tells us, how the four user buttons should be programmed. This sets the colors to match the function:
    switch(userButtonMode)  {
       case 1:
          // Setting colors of the command buttons:
          cmdSelect.setButtonColor(7, AtemSwitcher.getDownstreamKeyerStatus(1) ? 4 : 5);    // DSK1 button
          cmdSelect.setButtonColor(8, AtemSwitcher.getDownstreamKeyerStatus(2) ? 4 : 5);    // DSK2 button
          cmdSelect.setButtonColor(3, AtemSwitcher.getTransitionPosition()>0 ? 4 : 5);     // Auto button
          cmdSelect.setButtonColor(4, AtemSwitcher.getUpstreamKeyerStatus(1) ? 4 : 5);     // PIP button
      break; 
       case 2:
          // Setting colors of the command buttons:
          cmdSelect.setButtonColor(7, AtemSwitcher.getUpstreamKeyerStatus(1) ? 4 : 5);     // Key1
          cmdSelect.setButtonColor(8, AtemSwitcher.getUpstreamKeyerStatus(2) ? 4 : 5);     // Key2
          cmdSelect.setButtonColor(3, AtemSwitcher.getUpstreamKeyerStatus(3) ? 4 : 5);     // Key3
          cmdSelect.setButtonColor(4, AtemSwitcher.getUpstreamKeyerStatus(4) ? 4 : 5);     // Key4
      break; 
       case 3:
          // Setting colors of the command buttons:
          cmdSelect.setButtonColor(7, AtemSwitcher.getProgramInput()==10 ? 2 : (AtemSwitcher.getPreviewInput()==10 ? 3 : 5));     // Color1
          cmdSelect.setButtonColor(8, AtemSwitcher.getProgramInput()==11 ? 2 : (AtemSwitcher.getPreviewInput()==11 ? 3 : 5));     // Color2
          cmdSelect.setButtonColor(3, AtemSwitcher.getProgramInput()==0 ? 2 : (AtemSwitcher.getPreviewInput()==0 ? 3 : 5));     // Black
          cmdSelect.setButtonColor(4, AtemSwitcher.getProgramInput()==9 ? 2 : (AtemSwitcher.getPreviewInput()==9 ? 3 : 5));     // Bars
      break; 
      default:
          // Setting colors of the command buttons:
          cmdSelect.setButtonColor(7, AtemSwitcher.getDownstreamKeyerStatus(1) ? 4 : 5);    // DSK1 button
          cmdSelect.setButtonColor(8, preVGA_active ? 4 : 5);     // VGA+PIP button
          cmdSelect.setButtonColor(3, AtemSwitcher.getTransitionPosition()>0 ? 4 : 5);     // Auto button
          cmdSelect.setButtonColor(4, AtemSwitcher.getUpstreamKeyerStatus(1) ? 4 : 5);     // PIP button
      break;
    }

    if (!cmdSelect.buttonIsPressed(1))  {
      cmdSelect.setButtonColor(1, 5);   // de-highlight CUT button
    }
}


void readingButtonsAndSendingCommands() {

  // Sending commands for input selection:
    uint8_t busSelection = inputSelect.buttonDownAll();
    if (inputSelect.isButtonIn(1, busSelection))  { AtemSwitcher.changePreviewInput(5); }
    if (inputSelect.isButtonIn(2, busSelection))   { AtemSwitcher.changePreviewInput(6); }
    if (inputSelect.isButtonIn(3, busSelection))   { AtemSwitcher.changePreviewInput(7); }
    if (inputSelect.isButtonIn(4, busSelection))  { AtemSwitcher.changePreviewInput(8); }
    if (inputSelect.isButtonIn(5, busSelection))  { AtemSwitcher.changePreviewInput(1); }
    if (inputSelect.isButtonIn(6, busSelection))   { AtemSwitcher.changePreviewInput(2); }
    if (inputSelect.isButtonIn(7, busSelection))  { AtemSwitcher.changePreviewInput(3); }
    if (inputSelect.isButtonIn(8, busSelection))  { AtemSwitcher.changePreviewInput(4); }
  
  
      // "T-bar" slider:
    if (utils.uniDirectionalSlider_hasMoved())  {
      AtemSwitcher.changeTransitionPosition(utils.uniDirectionalSlider_position());
      AtemSwitcher.delay(20);
      if (utils.uniDirectionalSlider_isAtEnd())  {
	AtemSwitcher.changeTransitionPositionDone();
	AtemSwitcher.delay(5);  
      }
    }  
    
    // Cut button:
    uint8_t cmdSelection = cmdSelect.buttonDownAll();
    if (cmdSelection & (B1 << 0))  { 
      cmdSelect.setButtonColor(1, 4);    // Highlight CUT button
        // If VGA is the one source, make Auto instead!
      if (AtemSwitcher.getProgramInput()==8 || AtemSwitcher.getPreviewInput()==8)  {
        AtemSwitcher.doAuto(); 
      } else {
        AtemSwitcher.doCut(); 
      }
      preVGA_active = false;
    }

    switch(userButtonMode)  {
       case 1:
          if (cmdSelection & (B1 << 6))  { AtemSwitcher.changeDownstreamKeyOn(1, !AtemSwitcher.getDownstreamKeyerStatus(1)); }  // DSK1
          if (cmdSelection & (B1 << 7))  { AtemSwitcher.changeDownstreamKeyOn(2, !AtemSwitcher.getDownstreamKeyerStatus(2)); }  // DSK1
          if (cmdSelection & (B1 << 2))  { AtemSwitcher.doAuto(); preVGA_active = false;}
          if (cmdSelection & (B1 << 3))  { cmd_pipToggle(); }  // PIP
       break;
       case 2:
          if (cmdSelection & (B1 << 6))  { AtemSwitcher.changeUpstreamKeyOn(1, !AtemSwitcher.getUpstreamKeyerStatus(1)); }  // Key1
          if (cmdSelection & (B1 << 7))  { AtemSwitcher.changeUpstreamKeyOn(2, !AtemSwitcher.getUpstreamKeyerStatus(2)); }  // Key2
          if (cmdSelection & (B1 << 2))  { AtemSwitcher.changeUpstreamKeyOn(3, !AtemSwitcher.getUpstreamKeyerStatus(3)); }  // Key3
          if (cmdSelection & (B1 << 3))  { AtemSwitcher.changeUpstreamKeyOn(4, !AtemSwitcher.getUpstreamKeyerStatus(4)); }  // Key4
       break;
       case 3:
          if (cmdSelection & (B1 << 6))  { AtemSwitcher.changePreviewInput(10); }  // Color1
          if (cmdSelection & (B1 << 7))  { AtemSwitcher.changePreviewInput(11); }  // Color2  
          if (cmdSelection & (B1 << 2))  { AtemSwitcher.changePreviewInput(0); }   // Black
          if (cmdSelection & (B1 << 3))  { AtemSwitcher.changePreviewInput(9); }  // Bars
       break;
       default:
          if (cmdSelection & (B1 << 6))  { AtemSwitcher.changeDownstreamKeyOn(1, !AtemSwitcher.getDownstreamKeyerStatus(1)); }  // DSK1
          if (cmdSelection & (B1 << 7))  { cmd_vgaToggle(); }    
          if (cmdSelection & (B1 << 2))  { AtemSwitcher.doAuto(); preVGA_active = false;}
          if (cmdSelection & (B1 << 3))  { cmd_pipToggle(); }  // PIP
       break;
    }
}


void cmd_vgaToggle()  {
         if (!preVGA_active)  {
          preVGA_active = true;
          preVGA_UpstreamkeyerStatus = AtemSwitcher.getUpstreamKeyerStatus(1);
          preVGA_programInput = AtemSwitcher.getProgramInput();
  
          AtemSwitcher.changeProgramInput(8);
          AtemSwitcher.changeUpstreamKeyOn(1, true); 
        } else {
          preVGA_active = false;
          AtemSwitcher.changeProgramInput(preVGA_programInput);
          AtemSwitcher.changeUpstreamKeyOn(1, preVGA_UpstreamkeyerStatus); 
        }
 
}


void cmd_pipToggle()  {
      // For Picture-in-picture, do an "auto" transition:
      unsigned long timeoutTime = millis()+5000;

        // First, store original preview input:
      uint8_t tempPreviewInput = AtemSwitcher.getPreviewInput();  
        
        // Then, set preview=program (so auto doesn't change input)
      AtemSwitcher.changePreviewInput(AtemSwitcher.getProgramInput());  
      while(AtemSwitcher.getProgramInput()!=AtemSwitcher.getPreviewInput())  {
           AtemSwitcher.runLoop();
           if (timeoutTime<millis()) {break;}
      }
        
        // Then set transition status:
      bool tempOnNextTransitionStatus = AtemSwitcher.getUpstreamKeyerOnNextTransitionStatus(1);
      AtemSwitcher.changeUpstreamKeyNextTransition(1, true);  // Set upstream key next transition
      while(!AtemSwitcher.getUpstreamKeyerOnNextTransitionStatus(1))  {  
           AtemSwitcher.runLoop();
           if (timeoutTime<millis()) {break;}
      }

        // Make Auto Transition:      
      AtemSwitcher.doAuto();
      while(AtemSwitcher.getTransitionPosition()==0)  {
           AtemSwitcher.runLoop();
           if (timeoutTime<millis()) {break;}
      }
      while(AtemSwitcher.getTransitionPosition()>0)  {
           AtemSwitcher.runLoop();
           if (timeoutTime<millis()) {break;}
      }

        // Then reset transition status:
      AtemSwitcher.changeUpstreamKeyNextTransition(1, tempOnNextTransitionStatus);
      while(tempOnNextTransitionStatus!=AtemSwitcher.getUpstreamKeyerOnNextTransitionStatus(1))  {  
           AtemSwitcher.runLoop();
           if (timeoutTime<millis()) {break;}
      }
        // Reset preview bus:
      AtemSwitcher.changePreviewInput(tempPreviewInput);  
      while(tempPreviewInput!=AtemSwitcher.getPreviewInput())  {
           AtemSwitcher.runLoop();
           if (timeoutTime<millis()) {break;}
      }
        // Finally, tell us how we did:
     if (timeoutTime<millis()) {
       Serial.println("Timed out during operation!");
     } else {
      Serial.println("DONE!");
     }  
}
