/*****************
 * Basic T-bar with a potentiometer
 * Works with Ethernet enabled arduino devices (Arduino Ethernet or a model with Ethernet shield)
 * Make sure to configure IP and addresses! Look for "<= SETUP" in the code below!
 *
 * This example also uses SkaarhojUtils library. 
 * Download it from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
 * Connect the outer pins of a 10K potentiometer to GND and 5V. Connect the middle pin to A0. Connect also a 10nF capacitor between GND and middle pin (stabilizes the analog readout)
 * See also http://skaarhoj.com/wiki/index.php/SKAARHOJ_Slider_board
 * 
 * - kasper
 */
 
 
 
 
#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>


// MAC address and IP address for this *particular* Ethernet Shield!
// MAC address is printed on the shield
// IP address is an available address you choose on your subnet where the switcher is also present:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x21, 0x16 };      // <= SETUP!
IPAddress ip(192, 168, 0, 20);              // <= SETUP!






// Include ATEM library and make an instance:
#include <ATEM.h>
ATEM AtemSwitcher;

// All related to library "SkaarhojUtils". Used for "T-bar" potentiometer:
#include "SkaarhojUtils.h"
SkaarhojUtils utils;



void setup() { 
  
  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  
  
  // Initializing the slider:
  utils.uniDirectionalSlider_init();
  
  AtemSwitcher.begin(IPAddress(192, 168, 0, 50), 56417);    // <= SETUP!
  AtemSwitcher.serialOutput(true);	// Comment this line out for production code!!
  AtemSwitcher.connect();
}






// These variables are used to track state, for instance when the VGA+PIP button has been pushed.
bool AtemOnline = false;

void loop() {

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();
  
  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
      if (AtemOnline)  {
        AtemOnline = false;
      }
     
     AtemSwitcher.connect();
  }

    // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
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
}
