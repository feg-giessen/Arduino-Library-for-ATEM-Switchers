/*****************
 * Basis control for the SKAARHOJ C100 models
 * This example is programmed for ATEM 1M/E versions
 *
 * This example also uses a number of custom libraries which you must install first. 
 * Search for "#include" in this file to find the libraries. Then download the libraries from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
 * Works with Ethernet enabled arduino devices (Arduino Mega with Ethernet shield preferred because of memory consumption)
 * Make sure to configure MAC address! Look for "<= SETUP" in the code below!
 * 
 * - kasper
 */
 

 /* General TODO:
 - Better handling of menu generation, comparison of items. Understand that code!!
 - Strings, PROGMEM etc. -> Save memory, what's the memory usage anyway?
 */
 

 
// Including libraries: 
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include "WebServer.h"  // For web interface
#include <EEPROM.h>      // For storing IP numbers
#include <MenuBackend.h>  // Library for menu navigation. Must (for some reason) be included early! Otherwise compilation chokes.


// MAC address for this *particular* Ethernet Shield!
// MAC address is printed on the shield
static uint8_t mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x7F, 0x7D };    // <= SETUP!      
static uint8_t default_ip[] = {     // IP for Configuration Mode (192.168.10.99)  // TODO: Change this...
  192, 168, 10, 97 };

uint8_t ip[4];        // Will hold the C100 IP address
uint8_t atem_ip[4];  // Will hold the ATEM IP address

  


// no-cost stream operator as described at 
// http://sundial.org/arduino/?page_id=119
template<class T>
inline Print &operator <<(Print &obj, T arg)
{ 
  obj.print(arg); 
  return obj; 
}



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




































/*************************************************************
 *
 *
 *                     LCD PANEL FUNCTIONS
 *
 *
 **********************************************************/




#include <SoftwareSerial.h>
#define txPin 7

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




































/*************************************************************
 *
 *
 *                     MENU SYSTEM
 *
 *
 **********************************************************/



uint8_t userButtonMode = 0;  // 0-3
uint8_t setMenuValues = 0;  //
uint8_t BUSselect = 0;  // Preview/Program by default

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
    MenuItem menu_usrcfg5 = MenuItem(menu, "AUX1        AUX2AUX3     PROGRAM", 2);  // (As above)
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
    menu_aux3.addAfter(menu_UP2);

      // Set up user button menu:
    menu_usrcfg1.addAfter(menu_usrcfg2);  // Chain subitems...
    menu_usrcfg2.addAfter(menu_usrcfg3);
    menu_usrcfg3.addAfter(menu_usrcfg4);
    menu_usrcfg4.addAfter(menu_usrcfg5);
    menu_usrcfg2.addLeft(menu_userbut);  // Add parent item - starting with number 2...
    menu_usrcfg3.addLeft(menu_userbut);
    menu_usrcfg4.addLeft(menu_userbut);
    menu_usrcfg5.addLeft(menu_userbut);
    menu_userbut.addRight(menu_usrcfg1);     // Add the submenu to the parent - this will also see "left" for "menu_usercfg1"

      // Set up transition menu:
    menu_trtype.addAfter(menu_trtime);    // Chain subitems...
    menu_trtime.addLeft(menu_trans);      // Add parent item
    menu_trans.addRight(menu_trtype);     // Add the submenu to the parent - this will also see "left" for "menu_trtype"

      // Set up fade-to-black menu:
    menu_ftbtime.addAfter(menu_ftbexec);    // Chain subitems...
    menu_ftbexec.addLeft(menu_ftb);      // Add parent item
    menu_ftb.addRight(menu_ftbtime);     // Add the submenu to the parent
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
    LCD.print("    SKAARHOJ          C100    ");    
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
        if (changed.to.getName() == menu_usrcfg5.getName())  { userButtonMode=4; }
        if (changed.to.getName() == menu_mediab1.getName())  { setMenuValues=1;  }
        if (changed.to.getName() == menu_mediab2.getName())  { setMenuValues=2;  }
        if (changed.to.getName() == menu_aux1.getName())  { setMenuValues=3;  }
        if (changed.to.getName() == menu_aux2.getName())  { setMenuValues=4;  }
        if (changed.to.getName() == menu_aux3.getName())  { setMenuValues=5;  }
        if (changed.to.getName() == menu_trtype.getName())  { setMenuValues=10;  }
        if (changed.to.getName() == menu_trtime.getName())  { setMenuValues=11;  }
        if (changed.to.getName() == menu_ftbtime.getName())  { setMenuValues=20;  }
        if (changed.to.getName() == menu_ftbexec.getName())  { setMenuValues=21;  }
          // TODO: I HAVE to find another way to match the items here because two items with the same name will choke!!
        
        
    } else {  // Just clear the displays second line if there are items to the right in the menu:
      lcdPosition(0,15);
      LCD.print(">                ");  // Arrow + Clear the line...
    }
  }
}




void _enc0active()  {
  utils.encoders_interrupt(0);
}
void _enc1active()  {
  utils.encoders_interrupt(1);
}
















































/*************************************************************
 *
 *
 *                     Webserver
 *
 *
 **********************************************************/




#define PREFIX ""
WebServer webserver(PREFIX, 80);

void logoCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  /* this data was taken from a PNG file that was converted to a C data structure
   * by running it through the directfb-csource application. */
  P(logoData) = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0xc8, 0x00, 0x00, 0x00, 0x1f, 0x08, 0x03, 0x00, 0x00, 0x00, 0x95, 0xbd, 0x72,
    0x3b, 0x00, 0x00, 0x00, 0x19, 0x74, 0x45, 0x58, 0x74, 0x53, 0x6f, 0x66, 0x74, 0x77, 0x61, 0x72,
    0x65, 0x00, 0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x49, 0x6d, 0x61, 0x67, 0x65, 0x52, 0x65, 0x61,
    0x64, 0x79, 0x71, 0xc9, 0x65, 0x3c, 0x00, 0x00, 0x00, 0x30, 0x50, 0x4c, 0x54, 0x45, 0x9d, 0x98,
    0x98, 0xff, 0xff, 0xff, 0xd0, 0xcf, 0xcf, 0x63, 0x4a, 0x4a, 0x4c, 0x2e, 0x2e, 0xe6, 0xe6, 0xe6,
    0x3e, 0x13, 0x13, 0xf5, 0xf6, 0xf6, 0x81, 0x76, 0x76, 0xb1, 0xae, 0xae, 0xd8, 0xd8, 0xd8, 0xec,
    0xec, 0xec, 0xf1, 0xf0, 0xf0, 0xfb, 0xfb, 0xfb, 0xc4, 0xc0, 0xc0, 0xe0, 0xdf, 0xdf, 0x89, 0x3d,
    0x63, 0x99, 0x00, 0x00, 0x04, 0x48, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0xec, 0x58, 0xeb, 0x72,
    0xa5, 0x20, 0x0c, 0x46, 0xc0, 0x12, 0xd0, 0xc0, 0xfb, 0xbf, 0xed, 0xe6, 0x02, 0x01, 0x3d, 0xdb,
    0x99, 0x6e, 0x3d, 0xb3, 0x3f, 0x3a, 0x8d, 0xa7, 0x1d, 0x41, 0x2e, 0xb9, 0x7c, 0xf9, 0x82, 0xba,
    0xed, 0x87, 0x88, 0xfb, 0x35, 0xe4, 0xd7, 0x90, 0x5f, 0x43, 0xbe, 0x6c, 0x48, 0x2a, 0xb5, 0x36,
    0x1c, 0x2d, 0x44, 0x9c, 0x77, 0xf8, 0x97, 0xee, 0xe5, 0xe9, 0xa5, 0xf1, 0xd2, 0xf1, 0x3a, 0x13,
    0xdb, 0xba, 0xd3, 0x67, 0x2b, 0x7f, 0xd3, 0x90, 0x1d, 0xe2, 0x07, 0x49, 0x80, 0x43, 0x9a, 0x25,
    0x86, 0x28, 0x0f, 0xd1, 0xc5, 0x10, 0x62, 0xee, 0xb6, 0x86, 0xd1, 0x3d, 0xa4, 0x52, 0x47, 0xf0,
    0x63, 0x35, 0x1e, 0x2a, 0xbf, 0xbe, 0x0c, 0x52, 0x03, 0xce, 0xbe, 0xcc, 0x18, 0xe7, 0x21, 0xd0,
    0x46, 0x31, 0xba, 0x73, 0x5d, 0xe8, 0xe4, 0xa1, 0xf8, 0xdc, 0x90, 0x26, 0x66, 0x88, 0x29, 0x45,
    0x1e, 0xd0, 0x1d, 0xaf, 0x8b, 0xc0, 0x7d, 0xb1, 0xea, 0x16, 0x4e, 0x1a, 0xc7, 0xe2, 0x46, 0xd6,
    0xe9, 0x03, 0x7a, 0x0b, 0x3e, 0xa6, 0x84, 0xc6, 0xde, 0xe1, 0xe1, 0xaa, 0x3e, 0x0f, 0xcc, 0xbc,
    0xa0, 0xb3, 0x21, 0xb1, 0x2c, 0x9a, 0x78, 0x9e, 0x73, 0x3e, 0x37, 0x24, 0x2c, 0x2a, 0xa4, 0xde,
    0x76, 0x43, 0xd1, 0x50, 0x7b, 0xd4, 0xe2, 0x7d, 0x7f, 0xd5, 0xaa, 0x7b, 0xf2, 0x8c, 0x8b, 0x21,
    0xd2, 0x59, 0x79, 0xb2, 0xd8, 0x7d, 0x44, 0x76, 0xc7, 0x70, 0xcc, 0xb0, 0x64, 0xbf, 0xae, 0xf4,
    0x86, 0x88, 0x14, 0x5e, 0x16, 0x72, 0x76, 0x51, 0x1d, 0x97, 0xd8, 0x80, 0x8a, 0x67, 0x98, 0x31,
    0x9a, 0x1e, 0xaf, 0xb6, 0x5f, 0x57, 0x3d, 0xec, 0xcb, 0x22, 0xce, 0x81, 0x93, 0x81, 0x7e, 0xd5,
    0xae, 0xf4, 0x20, 0x69, 0x4c, 0x69, 0x27, 0x41, 0xf2, 0xa2, 0x39, 0x4f, 0x71, 0xcf, 0x0d, 0x61,
    0xd7, 0x11, 0xac, 0x71, 0xcb, 0xbc, 0x21, 0xaa, 0x4e, 0x45, 0xed, 0x80, 0xb6, 0x5a, 0x1b, 0x25,
    0x52, 0x1d, 0x58, 0xac, 0x16, 0xbb, 0x5a, 0x46, 0xa0, 0xfa, 0x9f, 0xe1, 0xa3, 0x40, 0xb2, 0xb8,
    0x52, 0x0f, 0xaf, 0x0b, 0x09, 0x8b, 0x06, 0x18, 0xfb, 0x96, 0x33, 0xb8, 0x12, 0xfa, 0xba, 0xbd,
    0xc7, 0x10, 0x86, 0x14, 0x42, 0x08, 0x94, 0x85, 0xb2, 0xaf, 0x97, 0xa4, 0x84, 0x03, 0x17, 0xa7,
    0xb1, 0x23, 0x47, 0x4a, 0xa8, 0x9b, 0x41, 0x31, 0x33, 0xd0, 0x31, 0x91, 0x9a, 0xb7, 0x14, 0xed,
    0x19, 0xa8, 0xff, 0x41, 0x13, 0x0e, 0x2d, 0x06, 0x96, 0xa3, 0xf7, 0x9c, 0xf9, 0xa6, 0x21, 0x82,
    0x7e, 0x61, 0x15, 0x4c, 0x67, 0xd2, 0x4d, 0x02, 0xf7, 0x45, 0x97, 0x86, 0xcf, 0x24, 0x58, 0x6c,
    0x5c, 0xe8, 0x5d, 0x82, 0x77, 0x57, 0xd5, 0xf9, 0xa6, 0xbd, 0x45, 0xaa, 0xaa, 0x76, 0xd9, 0x57,
    0x12, 0x7d, 0x26, 0x99, 0xd2, 0xf7, 0x4c, 0x8b, 0xdd, 0x1b, 0x7a, 0xcb, 0xa6, 0x87, 0xc9, 0xae,
    0xf0, 0x0f, 0x03, 0xfe, 0x41, 0x20, 0x23, 0x5e, 0xc4, 0x35, 0xf8, 0x8e, 0x9f, 0x8c, 0x0d, 0x15,
    0x1c, 0x65, 0xe8, 0x23, 0xfe, 0x77, 0xb5, 0x66, 0x17, 0x3a, 0x6d, 0xe5, 0x35, 0xf9, 0x99, 0xbe,
    0x04, 0x7c, 0x05, 0xb7, 0x7b, 0x00, 0x6f, 0x8d, 0x27, 0x86, 0x1c, 0x3d, 0x91, 0x95, 0xa0, 0x92,
    0x91, 0xd8, 0xf4, 0x92, 0xd3, 0xac, 0x06, 0x83, 0x40, 0xb7, 0x8c, 0x13, 0x5e, 0x92, 0xb6, 0x5c,
    0xd5, 0xce, 0x57, 0x86, 0x1a, 0x93, 0x67, 0x82, 0xc3, 0x1a, 0x11, 0x98, 0xa1, 0x7a, 0x58, 0x10,
    0x4f, 0xd7, 0x95, 0x77, 0xa6, 0x13, 0x27, 0xb6, 0x31, 0x09, 0x83, 0x8f, 0xeb, 0x62, 0xb6, 0xa4,
    0x14, 0xa8, 0xed, 0x68, 0x31, 0x5a, 0xfd, 0x1f, 0x43, 0x4e, 0x8a, 0xb5, 0xa8, 0xa2, 0x39, 0x78,
    0x49, 0x8b, 0xb0, 0xac, 0xce, 0x4e, 0x89, 0xf5, 0x4d, 0x47, 0x94, 0x43, 0x4d, 0xe1, 0x02, 0xa6,
    0x3a, 0x81, 0x40, 0xbd, 0x4d, 0xff, 0x05, 0xdf, 0x76, 0x37, 0x74, 0xe1, 0x48, 0x44, 0x68, 0x6d,
    0x0f, 0x3a, 0x48, 0xfc, 0x4f, 0x35, 0x3d, 0x8a, 0x19, 0x8c, 0x1f, 0x14, 0xac, 0xf9, 0x4a, 0x97,
    0x06, 0x4f, 0xd4, 0xcd, 0xdb, 0xd5, 0x33, 0xda, 0x58, 0x76, 0x7a, 0x60, 0xc8, 0x5e, 0x33, 0x3b,
    0x70, 0x4b, 0x39, 0x68, 0xb8, 0x85, 0x5c, 0xc0, 0x2f, 0xd1, 0x2f, 0x51, 0x99, 0x56, 0xfe, 0x60,
    0x56, 0xf9, 0xce, 0xbf, 0x75, 0xe4, 0x3a, 0x31, 0x6c, 0x64, 0x3a, 0xc0, 0x51, 0xac, 0x9b, 0xe5,
    0x5c, 0x56, 0xd2, 0xaa, 0x0b, 0xb2, 0xac, 0x42, 0x49, 0x74, 0x1f, 0x1f, 0x1a, 0x85, 0x64, 0xf2,
    0xa4, 0x7b, 0xd5, 0x09, 0xf6, 0x0d, 0x8b, 0x91, 0x62, 0xb8, 0x17, 0xed, 0x76, 0x29, 0xe3, 0xb9,
    0x47, 0xa8, 0x76, 0x0b, 0x9d, 0xd1, 0x5c, 0x32, 0xef, 0x97, 0x4e, 0x73, 0xfd, 0x48, 0x22, 0x1e,
    0x39, 0x32, 0x40, 0xb9, 0x53, 0xf1, 0xf7, 0x23, 0x92, 0x2d, 0x07, 0x85, 0x3c, 0x92, 0xea, 0x84,
    0x93, 0xfe, 0x95, 0xa0, 0xfa, 0xa5, 0x95, 0x1c, 0x2e, 0x3d, 0x0e, 0xad, 0x76, 0x2b, 0x7b, 0x95,
    0x4b, 0x36, 0x8f, 0xa3, 0xca, 0x2c, 0x82, 0x1a, 0xe1, 0xe2, 0x75, 0xae, 0x4c, 0xc9, 0xcf, 0x4f,
    0xbf, 0x52, 0x45, 0x80, 0x88, 0x13, 0x34, 0xdb, 0xab, 0x61, 0xa2, 0xc9, 0x91, 0x44, 0x09, 0x8a,
    0x06, 0xd4, 0x2c, 0x15, 0x21, 0x16, 0xe1, 0x7d, 0xe2, 0x5a, 0x6a, 0x57, 0x27, 0x35, 0x72, 0xf1,
    0x7f, 0xa7, 0x52, 0xbc, 0xd6, 0x15, 0x39, 0x82, 0x06, 0x9d, 0x56, 0xf5, 0x28, 0xea, 0x14, 0x7d,
    0xda, 0x0c, 0xfb, 0x1b, 0xde, 0x47, 0xdc, 0xca, 0x92, 0x45, 0x35, 0x39, 0x26, 0x92, 0x15, 0xc2,
    0x5e, 0x4a, 0x0a, 0x2a, 0xce, 0x83, 0x1d, 0x05, 0x44, 0x3b, 0x3a, 0xb7, 0x4e, 0x74, 0x34, 0x55,
    0xeb, 0x98, 0x75, 0xdd, 0x8e, 0x2a, 0x75, 0x65, 0x36, 0x3e, 0x13, 0x4d, 0xc8, 0x3a, 0x7c, 0x6e,
    0x08, 0x1e, 0x93, 0xf1, 0x43, 0xe7, 0xff, 0xce, 0xf0, 0x87, 0x9c, 0x22, 0xe3, 0x02, 0x61, 0x49,
    0xa8, 0x3c, 0xcf, 0xe7, 0x9a, 0xc9, 0x6d, 0x8b, 0x56, 0xe1, 0xbb, 0xde, 0xd9, 0xdc, 0x9c, 0x26,
    0x41, 0x65, 0x4b, 0xad, 0xe8, 0x8e, 0x9e, 0x47, 0xf3, 0xd8, 0xff, 0x9c, 0x7e, 0x89, 0xaf, 0x04,
    0xee, 0x01, 0x3c, 0xf2, 0x0b, 0x52, 0x34, 0x52, 0xe7, 0x7b, 0xe2, 0xd4, 0x18, 0xc6, 0x2b, 0x5d,
    0xa3, 0x86, 0xe3, 0x5e, 0x73, 0x61, 0xa6, 0xa7, 0x05, 0x81, 0xdf, 0x9c, 0xb4, 0xab, 0xd2, 0x10,
    0x40, 0xcf, 0xff, 0x05, 0x6b, 0x27, 0x3f, 0xeb, 0x04, 0xe5, 0xe9, 0x9e, 0x8b, 0x0b, 0x54, 0x61,
    0xb6, 0x2d, 0x0b, 0x5f, 0x47, 0x78, 0x66, 0xc7, 0x5a, 0x10, 0x7d, 0x26, 0xf1, 0xb2, 0x71, 0x6a,
    0xa5, 0xd8, 0x3b, 0x4e, 0xf1, 0x74, 0x15, 0x3f, 0x11, 0xbc, 0x97, 0xb2, 0x23, 0x8d, 0xb0, 0x9a,
    0xaf, 0xc3, 0xcf, 0x52, 0xca, 0x38, 0x97, 0x51, 0x07, 0x15, 0x12, 0xfa, 0xaf, 0xb3, 0x70, 0x97,
    0xf6, 0x78, 0xa3, 0xa6, 0xb7, 0x85, 0xda, 0x92, 0x2d, 0x47, 0xdc, 0x9f, 0x6d, 0xe6, 0x5b, 0x3e,
    0x3e, 0x3c, 0x7a, 0x69, 0xfe, 0x47, 0xc1, 0x6b, 0xe3, 0xf1, 0xce, 0x3f, 0xe7, 0x2b, 0x0a, 0x7c,
    0x49, 0x1c, 0xff, 0xfa, 0xc5, 0xbf, 0xbf, 0x49, 0xd6, 0xeb, 0x26, 0x35, 0x0b, 0x45, 0x4f, 0x91,
    0x13, 0x8b, 0x09, 0xa3, 0x56, 0xa4, 0x95, 0x26, 0xb2, 0xb7, 0x5d, 0xe5, 0xd8, 0x0f, 0x96, 0x53,
    0x85, 0xde, 0x2d, 0x54, 0x30, 0x8d, 0x0f, 0x35, 0xd7, 0x20, 0xba, 0xcf, 0x65, 0x55, 0x4c, 0x6e,
    0x5e, 0x94, 0xbc, 0xaa, 0x7a, 0xd5, 0x57, 0x55, 0xf6, 0xab, 0xd2, 0xac, 0xf7, 0x94, 0xa1, 0xf9,
    0xa2, 0xbb, 0x69, 0x7f, 0x9c, 0x2f, 0x06, 0x88, 0x09, 0xc3, 0x08, 0x7c, 0xc5, 0xa2, 0xb3, 0x4f,
    0x51, 0x2f, 0x03, 0xf4, 0x7e, 0x7e, 0xa7, 0xc2, 0x6d, 0xfb, 0x64, 0xe4, 0x48, 0x2f, 0xc4, 0x7b,
    0x9e, 0xa1, 0xae, 0xf3, 0x1f, 0x72, 0xef, 0xc7, 0xe4, 0xc8, 0x1f, 0x01, 0x06, 0x00, 0x32, 0xa6,
    0x4d, 0x73, 0xd7, 0x92, 0xaa, 0x63, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82
  };

  if (type == WebServer::POST)
  {
    // ignore POST data
    server.httpFail();
    return;
  }

  /* for a GET or HEAD, send the standard "it's all OK headers" but identify our data as a PNG file */
  server.httpSuccess("image/png");

  /* we don't output the body for a HEAD request */
  if (type == WebServer::GET)
  {
    server.writeP(logoData, sizeof(logoData));
  }
}

// commands are functions that get called by the webserver framework
// they can read any posted data from client, and they output to server
void webDefaultView(WebServer &server, WebServer::ConnectionType type)
{
  P(htmlHead) =
    "<html>"
    "<head>"
    "<title>SKAARHOJ C100 Configuration</title>"
    "<style type=\"text/css\">"
    "BODY { font-family: sans-serif }"
    "H1 { font-size: 14pt; text-decoration: underline }"
    "P  { font-size: 10pt; }"
    "</style>"
    "</head>"
    "<body>"
    "<img src='logo.png'><br/>";

  int i;
  server.httpSuccess();
  server.printP(htmlHead);

  server << "<form action='" PREFIX "/form' method='post'>";

  // C100 Panel IP:
  server << "<h1>C100 Panel IP Address:</h1><p>";
  for (i = 0; i <= 3; ++i)
  {
    server << "<input type='text' name='IP" << i << "' value='" << EEPROM.read(i+2) << "' id='IP" << i << "' size='4'>";
    if (i<3)  server << '.';
  }
  server << "<hr/>";

  // ATEM Switcher Panel IP:
  server << "<h1>ATEM Switcher IP Address:</h1><p>";
  for (i = 0; i <= 3; ++i)
  {
    server << "<input type='text' name='ATEM_IP" << i << "' value='" << EEPROM.read(i+2+4) << "' id='ATEM_IP" << i << "' size='4'>";
    if (i<3)  server << '.';
  }
  server << "<hr/>";

  // End form and page:
  server << "<input type='submit' value='Submit'/></form>";
  server << "<br><i>(Reset / Pull the power after submitting the form successfully)</i>";
  server << "</body></html>";
}

void formCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  if (type == WebServer::POST)
  {
    bool repeat;
    char name[16], value[16];
    do
    {
      repeat = server.readPOSTparam(name, 16, value, 16);
      String Name = String(name);

      // C100 Panel IP:
      if (Name.startsWith("IP"))  {
        int addr = strtoul(name + 2, NULL, 10);
        int val = strtoul(value, NULL, 10);
        if (addr>=0 && addr <=3)  {
          EEPROM.write(addr+2, val);  // IP address stored in bytes 0-3
        }
      }

      // ATEM Switcher Panel IP:
      if (Name.startsWith("ATEM_IP"))  {
        int addr = strtoul(name + 7, NULL, 10);
        int val = strtoul(value, NULL, 10);
        if (addr>=0 && addr <=3)  {
          EEPROM.write(addr+2+4, val);  // IP address stored in bytes 4-7
        }
      }

    } 
    while (repeat);

    server.httpSeeOther(PREFIX "/form");
  }
  else
    webDefaultView(server, type);
}
void defaultCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  webDefaultView(server, type);  
}





























/*************************************************************
 *
 *
 *                     MAIN PROGRAM CODE AHEAD
 *
 *
 **********************************************************/



bool isConfigMode;  // If set, the system will run the Web Configurator, not the normal program

void setup() { 
  Serial.begin(9600);


  // *********************************
  // Start up BI8 boards and I2C bus:
  // *********************************
    // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin(); 
  
    // Set up the SkaarhojBI8 boards:
  inputSelect.begin(0,false);
  cmdSelect.begin(1,false);


  
  // *********************************
  // Mode of Operation (Normal / Configuration)
  // *********************************
     // Determine web config mode
     // This is done by:
     // -  either flipping a switch connecting A1 to GND
     // -  Holding the CUT button during start up.
  pinMode(A1,INPUT_PULLUP);
  delay(100);
  isConfigMode = cmdSelect.buttonIsPressed(1) || (analogRead(A1) < 100) ? true : false;
  



  // *********************************
  // INITIALIZE EEPROM memory:
  // *********************************
  // Check if EEPROM has ever been initialized, if not, install default IP
  if (EEPROM.read(0) != 12 &&  EEPROM.read(1) != 232)  {  // Just randomly selected values which should be unlikely to be in EEPROM by default.
    // Set these random values so this initialization is only run once!
    EEPROM.write(0,12);
    EEPROM.write(1,232);

    // Set default IP address for Arduino/C100 panel (192.168.10.99)
    EEPROM.write(0+2,192);
    EEPROM.write(1+2,168);
    EEPROM.write(2+2,10);
    EEPROM.write(3+2,99);  // Just some value I chose, probably below DHCP range?

    // Set default IP address for ATEM Switcher (192.168.10.240):
    EEPROM.write(0+2+4,192);
    EEPROM.write(1+2+4,168);
    EEPROM.write(2+2+4,10);
    EEPROM.write(3+2+4,240);
  }


 
  // *********************************
  // Setting up IP addresses, starting Ethernet
  // *********************************
  if (isConfigMode)  {
    // Setting the default ip address for configuration mode:
    ip[0] = default_ip[0];
    ip[1] = default_ip[1];
    ip[2] = default_ip[2];
    ip[3] = default_ip[3];
  } else {
    ip[0] = EEPROM.read(0+2);
    ip[1] = EEPROM.read(1+2);
    ip[2] = EEPROM.read(2+2);
    ip[3] = EEPROM.read(3+2);
  }

  // Setting the ATEM IP address:
  atem_ip[0] = EEPROM.read(0+2+4);
  atem_ip[1] = EEPROM.read(1+2+4);
  atem_ip[2] = EEPROM.read(2+2+4);
  atem_ip[3] = EEPROM.read(3+2+4);
  
  Serial << "C100 Panel IP Address: " << ip[0] << "." << ip[1] << "." << ip[2] << "." << ip[3] << "\n";
  Serial << "ATEM Switcher IP Address: " << atem_ip[0] << "." << atem_ip[1] << "." << atem_ip[2] << "." << atem_ip[3] << "\n";

  Ethernet.begin(mac, ip);


  // ********************
  // Start up LCD (has to be after Ethernet has been started!)
  // ********************
  pinMode(txPin, OUTPUT);
  LCD.begin(9600);
  clearLCD();
  lcdPosition(0,0);
  LCD.print("    SKAARHOJ          C100     ");
  backlightOn();
  delay(2000);


  // *********************************
  // Final Setup based on mode
  // *********************************
  if (isConfigMode)  {
    
      // LCD IP info:
    clearLCD();
    lcdPosition(0,0);
    LCD.print("CONFIG MODE, IP:");
    lcdPosition(1,0);
    LCD.print(ip[0]);
    LCD.print('.');
    LCD.print(ip[1]);
    LCD.print('.');
    LCD.print(ip[2]);
    LCD.print('.');
    LCD.print(ip[3]);

       // Red by default:
    inputSelect.setDefaultColor(2);
    cmdSelect.setDefaultColor(2); 
    inputSelect.setButtonColorsToDefault();
    cmdSelect.setButtonColorsToDefault();

    webserver.begin();
    webserver.setDefaultCommand(&defaultCmd);
    webserver.addCommand("form", &formCmd);
    webserver.addCommand("logo.png", &logoCmd);
  } else {

      // LCD IP info:
    clearLCD();
    lcdPosition(0,0);
    LCD.print("IP Address:");
    lcdPosition(1,0);
    LCD.print(ip[0]);
    LCD.print('.');
    LCD.print(ip[1]);
    LCD.print('.');
    LCD.print(ip[2]);
    LCD.print('.');
    LCD.print(ip[3]);
    
    delay(1000);

      // Colors of buttons:
    inputSelect.setDefaultColor(0);  // Off by default
    cmdSelect.setDefaultColor(0);  // Off by default

    inputSelect.testSequence();
    cmdSelect.testSequence();
  
    // Initializing the slider:
    utils.uniDirectionalSlider_init();
    
    // Initializing menu control:
    utils.encoders_init();
    attachInterrupt(0, _enc1active, RISING);
    attachInterrupt(1, _enc0active, RISING);
    
    menuSetup();
  
    // Connect to an ATEM switcher on this address and using this local port:
    // The port number is chosen randomly among high numbers.
    clearLCD();
    lcdPosition(0,0);
    LCD.print("Connecting to:");
    lcdPosition(1,0);
    LCD.print(atem_ip[0]);
    LCD.print('.');
    LCD.print(atem_ip[1]);
    LCD.print('.');
    LCD.print(atem_ip[2]);
    LCD.print('.');
    LCD.print(atem_ip[3]);
    
    AtemSwitcher.begin(IPAddress(atem_ip[0],atem_ip[1],atem_ip[2],atem_ip[3]), 56417);
    AtemSwitcher.serialOutput(true);  // TODO: Comment out
    AtemSwitcher.connect();
  }
}






/*************************************************************
 * Loop function (runtime loop)
 *************************************************************/

// These variables are used to track state, for instance when the VGA+PIP button has been pushed.
bool preVGA_active = false;
bool preVGA_UpstreamkeyerStatus = false;
int preVGA_programInput = 0;

// AtemOnline is true, if there is a connection to the ATEM switcher
bool AtemOnline = false;

// The loop function:
void loop() {
  
  if (isConfigMode)  {
    webserver.processConnection();
  } else {
  
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
}








/**********************************
 * Navigation for the main menu:
 ***********************************/
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






/**********************************
 * Button State Colors set:
 ***********************************/
void setButtonColors()  {
    // Setting colors of input select buttons:
    for (uint8_t i=1;i<=8;i++)  {
      uint8_t idx = i>4 ? i-4 : i+4;  // Mirroring because of buttons on PCB
      if (BUSselect==0)  {
        if (AtemSwitcher.getProgramTally(i))  {
          inputSelect.setButtonColor(idx, 2);
        } else if (AtemSwitcher.getPreviewTally(i))  {
          inputSelect.setButtonColor(idx, 3);
        } else {
          inputSelect.setButtonColor(idx, 5);   
        }
      } else if (BUSselect<=3)  {
        if (i==8?(AtemSwitcher.getAuxState(BUSselect)==16):(AtemSwitcher.getAuxState(BUSselect)==i))  {
          inputSelect.setButtonColor(idx, 2);
        } else {
          inputSelect.setButtonColor(idx, 5);   
        }
      } else if (BUSselect==4)  {
        if (AtemSwitcher.getProgramTally(i))  {
          inputSelect.setButtonColor(idx, 2);
        } else {
          inputSelect.setButtonColor(idx, 5);   
        }        
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
       case 4:
          // Setting colors of the command buttons:
          cmdSelect.setButtonColor(7, BUSselect==1 ? 2 : 5);     // AUX1
          cmdSelect.setButtonColor(8, BUSselect==2 ? 2 : 5);     // AUX2
          cmdSelect.setButtonColor(3, BUSselect==3 ? 2 : 5);     // AUX3
          cmdSelect.setButtonColor(4, BUSselect==4 ? 2 : 5);     // Program
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









/**********************************
 * ATEM Commands
 ***********************************/
void readingButtonsAndSendingCommands() {

  // Sending commands for input selection:
    uint8_t busSelection = inputSelect.buttonDownAll();
    if (BUSselect==0)  {
      if (inputSelect.isButtonIn(1, busSelection))  { AtemSwitcher.changePreviewInput(5); }
      if (inputSelect.isButtonIn(2, busSelection))   { AtemSwitcher.changePreviewInput(6); }
      if (inputSelect.isButtonIn(3, busSelection))   { AtemSwitcher.changePreviewInput(7); }
      if (inputSelect.isButtonIn(4, busSelection))  { AtemSwitcher.changePreviewInput(8); }
      if (inputSelect.isButtonIn(5, busSelection))  { AtemSwitcher.changePreviewInput(1); }
      if (inputSelect.isButtonIn(6, busSelection))   { AtemSwitcher.changePreviewInput(2); }
      if (inputSelect.isButtonIn(7, busSelection))  { AtemSwitcher.changePreviewInput(3); }
      if (inputSelect.isButtonIn(8, busSelection))  { AtemSwitcher.changePreviewInput(4); }
    } else if (BUSselect<=3)  {
      if (inputSelect.isButtonIn(1, busSelection))  { AtemSwitcher.changeAuxState(BUSselect, 5); }
      if (inputSelect.isButtonIn(2, busSelection))   { AtemSwitcher.changeAuxState(BUSselect, 6);  }
      if (inputSelect.isButtonIn(3, busSelection))   { AtemSwitcher.changeAuxState(BUSselect, 7);  }
      if (inputSelect.isButtonIn(4, busSelection))  { AtemSwitcher.changeAuxState(BUSselect, 16);  }
      if (inputSelect.isButtonIn(5, busSelection))  { AtemSwitcher.changeAuxState(BUSselect, 1); }
      if (inputSelect.isButtonIn(6, busSelection))   { AtemSwitcher.changeAuxState(BUSselect, 2);  }
      if (inputSelect.isButtonIn(7, busSelection))  { AtemSwitcher.changeAuxState(BUSselect, 3);  }
      if (inputSelect.isButtonIn(8, busSelection))  { AtemSwitcher.changeAuxState(BUSselect, 4);  }
    } else if (BUSselect==4)  {
      if (inputSelect.isButtonIn(1, busSelection))  { AtemSwitcher.changeProgramInput(5); }
      if (inputSelect.isButtonIn(2, busSelection))   { AtemSwitcher.changeProgramInput(6); }
      if (inputSelect.isButtonIn(3, busSelection))   { AtemSwitcher.changeProgramInput(7); }
      if (inputSelect.isButtonIn(4, busSelection))  { AtemSwitcher.changeProgramInput(8); }
      if (inputSelect.isButtonIn(5, busSelection))  { AtemSwitcher.changeProgramInput(1); }
      if (inputSelect.isButtonIn(6, busSelection))   { AtemSwitcher.changeProgramInput(2); }
      if (inputSelect.isButtonIn(7, busSelection))  { AtemSwitcher.changeProgramInput(3); }
      if (inputSelect.isButtonIn(8, busSelection))  { AtemSwitcher.changeProgramInput(4); }
    } 
  
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
       case 4:
          // Setting colors of the command buttons:
          if (cmdSelection & (B1 << 6))  { BUSselect= BUSselect==1 ? 0 : 1;   }
          if (cmdSelection & (B1 << 7))  { BUSselect= BUSselect==2 ? 0 : 2;   }  
          if (cmdSelection & (B1 << 2))  { BUSselect= BUSselect==3 ? 0 : 3;   }   
          if (cmdSelection & (B1 << 3))  { BUSselect= BUSselect==4 ? 0 : 4;  }  
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
