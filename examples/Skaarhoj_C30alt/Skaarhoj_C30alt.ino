/*****************
 * Alternative control for the SKAARHOJ C30 series devices
 * This example is programmed for ATEM 1M/E versions
 * The button rows are assumed to be configured as AUX1 BUS-DSK1-Lower Third-Fade To Black / 1-2-3-CUT
 * When AUX1 is enabled, the bus input selector (1-2-3) will select those sources for the AUX1 output instead of Preview.
 * DSK1 enables downstream keyer 1. Lower Third simply enables upstream keyer 4 (assuming it is configured with a lower third graphic)
 * This example has a more modular approach to programming the buttons, trying to address each button separately when possible. Compare the code to the "Skaarhoj_C30" sketch to see this difference.
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

//#include <MemoryFree.h>

// MAC address and IP address for this *particular* Arduino / Ethernet Shield!
// The MAC address is printed on a label on the shield or on the back of your device
// The IP address should be an available address you choose on your subnet where the switcher is also present
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x6C, 0xB7 };      // <= SETUP!  MAC address of the Arduino
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

    // Shows free memory:  
//  Serial << F("freeMemory()=") << freeMemory() << "\n";
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

    // Implement the functions of the buttons and slider:
    buttonFunctions();  // Notice: THis function is in another external file, ButtonFunctions.ino. It's open as a tab in the top of the Arduino IDE!
    sliderFunction();
  }
}



/*************************
 * Slider operation
 *************************/
void sliderFunction()  {

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


