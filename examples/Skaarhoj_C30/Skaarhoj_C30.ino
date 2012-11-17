/*****************
 * Basis control for the SKAARHOJ C30 series devices
 * This example is programmed for ATEM 1M/E versions
 * The button rows are assumed to be configured as 1-2-3-AUTO / 4-5-6-CUT
 *
 * This example also uses several custom libraries which you must install first. 
 * Search for "#include" in this file to find the libraries. Then download the libraries from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
 * Works with ethernet-enabled arduino devices (Arduino Ethernet or a model with Ethernet shield)
 * Make sure to configure IP and addresses! Look for all instances of "<= SETUP" in the code below!
 * 
 * - kasper
 */



// Including libraries: 
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>


// MAC address and IP address for this *particular* Arduino / Ethernet Shield!
// The MAC address is printed on a label on the shield or on the back of your device
// The IP address should be an available address you choose on your subnet where the switcher is also present
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x6B, 0xB9 };      // <= SETUP!  MAC address of the Arduino
IPAddress ip(192, 168, 10, 99);              // <= SETUP!  IP address of the Arduino


// Include ATEM library and make an instance:
// Connect to an ATEM switcher on this address and using this local port:
// The port number is chosen randomly among high numbers.
#include <ATEM.h>
ATEM AtemSwitcher(IPAddress(192, 168, 10, 240), 56417);  // <= SETUP (the IP address of the ATEM switcher)



// No-cost stream operator as described at 
// http://arduiniana.org/libraries/streaming/
template<class T>
inline Print &operator <<(Print &obj, T arg)
{  
  obj.print(arg); 
  return obj; 
}



// All related to library "SkaarhojBI8":
#include "Wire.h"
#include "MCP23017.h"
#include "PCA9685.h"
#include "SkaarhojBI8.h"
#include "SkaarhojUtils.h"

SkaarhojBI8 buttons;
SkaarhojUtils utils;



void setup() { 

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);
  Serial << F("\n- - - - - - - -\nSerial Started\n");  

  // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin(); 

  // Set up the SkaarhojBI8 boards:
  buttons.begin(0);

  buttons.setDefaultColor(0);  // Off by default
  buttons.testSequence();

  // Initializing the slider:
  utils.uniDirectionalSlider_init();

  // Initialize a connection to the switcher:
//  AtemSwitcher.serialOutput(true);  // Remove or comment out this line for production code. Serial output may decrease performance!
  AtemSwitcher.connect();
}



bool AtemOnline = false;

void loop() {

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
    if (AtemOnline)  {
      AtemOnline = false;

      Serial << F("Turning off buttons light\n");
      buttons.setDefaultColor(0);  // Off by default
      buttons.setButtonColorsToDefault();
    }

    Serial << F("Connection to ATEM Switcher has timed out - reconnecting!\n");
    AtemSwitcher.connect();
  }

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
      Serial << F("Turning on buttons\n");

      buttons.setDefaultColor(5);  // Dimmed by default
      buttons.setButtonColorsToDefault();
    }


    setButtonColors();
    commandDispatch();
  }
}




/*************************
 * Set button colors
 *************************/
void setButtonColors()  {

  // Setting colors of input select buttons:
  for (uint8_t i=1;i<=6;i++)  {
    if (AtemSwitcher.getProgramTally(i))  {
      buttons.setButtonColor(i, 2);
    } 
    else if (AtemSwitcher.getPreviewTally(i))  {
      buttons.setButtonColor(i, 3);
    } 
    else {
      buttons.setButtonColor(i, 5);   
    }
  }

  // Setting colors of the command buttons:
  buttons.setButtonColor(7, AtemSwitcher.getTransitionPosition()>0 ? 4 : 5);     // Auto button
  if (!buttons.buttonIsPressed(8))  {
    buttons.setButtonColor(8, 5);   // de-highlight CUT button
  }

}




/*************************
 * Commands handling
 *************************/
void commandDispatch()  {

  // Sending commands:
  uint8_t busSelection = buttons.buttonDownAll();
  if(busSelection > 0) {
    Serial << (busSelection, BIN) << "\n";
  }
  
  // The following 6 if-clauses detects if a button is pressed for input selection:
  if (buttons.isButtonIn(1, busSelection))  { 
    AtemSwitcher.changePreviewInput(1); 
  }
  if (buttons.isButtonIn(2, busSelection))   { 
    AtemSwitcher.changePreviewInput(2); 
  }
  if (buttons.isButtonIn(3, busSelection))   { 
    AtemSwitcher.changePreviewInput(3); 
  }
  if (buttons.isButtonIn(4, busSelection))  { 
    AtemSwitcher.changePreviewInput(4); 
  }
  if (buttons.isButtonIn(5, busSelection))  { 
    AtemSwitcher.changePreviewInput(5); 
  }
  if (buttons.isButtonIn(6, busSelection))   { 
    AtemSwitcher.changePreviewInput(6); 
  }

  // The following detects if a button is pressed for either AUTO or CUT:
  if (buttons.isButtonIn(7, busSelection))  { 
    AtemSwitcher.doAuto(); 
  }
  if (buttons.isButtonIn(8, busSelection))  {
    buttons.setButtonColor(8,4);    // Highlight CUT button
    AtemSwitcher.doCut(); 
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
}

