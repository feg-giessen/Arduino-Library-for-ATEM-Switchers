/*****************
 * Touch Control for ATEM panel
 * Requires connection to a touch screen via the SKAARHOJ utils library.
 * Works with Ethernet enabled arduino devices (Arduino Ethernet or a model with Ethernet shield)
 * Make sure to configure IP and addresses! Look for "<= SETUP" in the code below!
 * 
 * - kasper
 */


#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>


// MAC address and IP address for this *particular* Ethernet Shield!
// MAC address is printed on the shield
// IP address is an available address you choose on your subnet where the switcher is also present:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x0D, 0x6B, 0x6A };      // MAC address of the Arduino Ethernet behind the screen!
IPAddress ip(192, 168, 0, 21);              // IP address for the switcher control


// Include ATEM library and make an instance:
#include <ATEM.h>

// Connect to an ATEM switcher on this address and using this local port:
// The port number is chosen randomly among high numbers.
ATEM AtemSwitcher;


#include "SkaarhojUtils.h"
SkaarhojUtils utils;


// If this flag is set true, a tap on a source brings it directly to program
// If a tristate LED is connected to digital output 2 (green) and 3 (red) (both active high) it will light red when this functionality is on and green when preview select is on. It will light orange if it's in the process of connecting to the ATEM Switcher.
bool directlyToProgram = false;  


void setup() { 
  utils.touch_init();

  // The line below is calibration numbers for a specific monitor. 
  // Substitute this with calibration for YOUR monitor (see example "Touch_Calibrate")
  utils.touch_calibrationPointRawCoordinates(330,711,763,716,763,363,326,360);

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  
  
  digitalWrite(2,false);
  digitalWrite(3,false);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);

  // Connect to an ATEM switcher on this address and using this local port:
  // The port number is chosen randomly among high numbers.
  AtemSwitcher.begin(IPAddress(192, 168, 0, 50), 56417);    // <= SETUP!
  AtemSwitcher.serialOutput(true);
  AtemSwitcher.connect();
  digitalWrite(3,true);
  digitalWrite(2,true);
}


bool AtemOnline = false;

void loop() {

  // Check for packets, respond to them etc. Keeping the connection alive!
  AtemSwitcher.runLoop();

  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
    if (AtemOnline)  {
      AtemOnline = false;
      digitalWrite(3,false);
      digitalWrite(2,false);
    }

    Serial.println("Connection to ATEM Switcher has timed out - reconnecting!");
    AtemSwitcher.connect();
    digitalWrite(3,true);
    digitalWrite(2,true);
  }

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
    }

    AtemSwitcher.delay(15);	// This determines how smooth you will see the graphic move.
    manageTouch();
    digitalWrite(3,directlyToProgram);
    digitalWrite(2,!directlyToProgram);
  }
}


void manageTouch()  {

  uint8_t tState = utils.touch_state();  // See what the state of touch is:
  if (tState !=0)  {  // Different from zero? Then a touch even has happened:
  
      // Calculate which Multiviewer field on an ATEM we pressed:
    uint8_t MVfieldPressed = touchArea(utils.touch_getEndedXCoord(),utils.touch_getEndedYCoord());

    if (tState==9 && MVfieldPressed==16)  {  // held down in program area
      directlyToProgram=!directlyToProgram;
      Serial.print("directlyToProgram=");
      Serial.println(directlyToProgram);
    } else
    if (tState==1)  {  // Single tap somewhere:
      if (MVfieldPressed == AtemSwitcher.getPreviewInput() || MVfieldPressed==17)  {  // If Preview or a source ON preview is touched, bring in on.
        AtemSwitcher.doCut();
      } 
      else if (MVfieldPressed>=1 && MVfieldPressed<=8)  {  // If any other source is touched, bring it to preview or program depending on mode:
        if (directlyToProgram)  {
          AtemSwitcher.changeProgramInput(MVfieldPressed);
        } else {
          AtemSwitcher.changePreviewInput(MVfieldPressed);
        }
      }
    } 
  }
}

// Returns which field in MV was pressed:
uint8_t touchArea(int x, int y)  {
  if (y<=720/2)  {  // lower than middel
    if (x<=1280/4)  {
      if (y>=720/4)  {
        // 1: 
        return 1;
      } 
      else {
        // 5:
        return 5;
      }
    } 
    else if (x<=1280/2) {
      if (y>=720/4)  {
        // 2: 
        return 2;
      } 
      else {
        // 6:
        return 6;
      }
    } 
    else if (x<=1280/4*3) {
      if (y>=720/4)  {
        // 3: 
        return 3;
      } 
      else {
        // 7:
        return 7;
      }
    } 
    else {
      if (y>=720/4)  {
        // 4: 
        return 4;
      } 
      else {
        // 8:
        return 8;
      }
    }
  } 
  else {  // Preview or program:
    if (x<=1280/2)  {
      // Preview:
      return 17;
    } 
    else {
      // Program:
      return 16;
    }
  }
}

