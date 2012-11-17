/*****************
 * Basis control for the SKAARHOJ C50 series devices
 * This example is programmed for ATEM 1M/E versions
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
  0x90, 0xA2, 0xDA, 0x0D, 0x6C, 0x73 };      // <= SETUP!  MAC address of the Arduino
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

SkaarhojBI8 inputSelect;
SkaarhojBI8 cmdSelect;



void setup() { 

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  
  Serial << F("\n- - - - - - - -\nSerial Started\n");

  // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin(); 

  // Set up the SkaarhojBI8 boards:
  inputSelect.begin(0);
  cmdSelect.begin(1);

  inputSelect.setDefaultColor(0);  // Off by default
  cmdSelect.setDefaultColor(0);  // Off by default

  inputSelect.testSequence();
  cmdSelect.testSequence();

  // Initialize a connection to the switcher:
  AtemSwitcher.serialOutput(true);  // Remove or comment out this line for production code. Serial output may decrease performance!
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

  // If connection is gone, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
    if (AtemOnline)  {
      AtemOnline = false;

      Serial.println("Turning off buttons light");
      inputSelect.setDefaultColor(0);  // Off by default
      cmdSelect.setDefaultColor(0);  // Off by default
      inputSelect.setButtonColorsToDefault();
      cmdSelect.setButtonColorsToDefault();
    }

    Serial.println("Connection to ATEM Switcher has timed out - reconnecting!");
    AtemSwitcher.connect();
  }

  // If the switcher has been initialized, check for button presses as reflect status of switcher in button lights:
  if (AtemSwitcher.hasInitialized())  {
    if (!AtemOnline)  {
      AtemOnline = true;
      Serial.println("Turning on buttons");

      inputSelect.setDefaultColor(5);  // Dimmed by default
      cmdSelect.setDefaultColor(5);  // Dimmed by default
      inputSelect.setButtonColorsToDefault();
      cmdSelect.setButtonColorsToDefault();
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
  for (uint8_t i=1;i<=8;i++)  {
    uint8_t idx = i>4 ? i-4 : i+4;  // Mirroring because of buttons on PCB
    if (AtemSwitcher.getProgramTally(i))  {
      inputSelect.setButtonColor(idx, 2);
    } 
    else if (AtemSwitcher.getPreviewTally(i))  {
      inputSelect.setButtonColor(idx, 3);
    } 
    else {
      inputSelect.setButtonColor(idx, 5);   
    }
  }

  // Setting colors of the command buttons:
  cmdSelect.setButtonColor(3, AtemSwitcher.getTransitionPosition()>0 ? 4 : 5);     // Auto button
  cmdSelect.setButtonColor(8, preVGA_active ? 4 : 5);     // VGA+PIP button
  cmdSelect.setButtonColor(4, AtemSwitcher.getUpstreamKeyerStatus(1) ? 4 : 5);     // PIP button
  cmdSelect.setButtonColor(7, AtemSwitcher.getDownstreamKeyerStatus(1) ? 4 : 5);    // DSK1 button
  if (!cmdSelect.buttonIsPressed(1))  {
    cmdSelect.setButtonColor(1, 5);   // de-highlight CUT button
  }
}




/*************************
 * Commands handling
 *************************/
void commandDispatch()  {

  // Sending commands, bus input selection:
  uint8_t busSelection = inputSelect.buttonDownAll();
  if (inputSelect.isButtonIn(1, busSelection))  { 
    AtemSwitcher.changePreviewInput(5); 
  }
  if (inputSelect.isButtonIn(2, busSelection))   { 
    AtemSwitcher.changePreviewInput(6); 
  }
  if (inputSelect.isButtonIn(3, busSelection))   { 
    AtemSwitcher.changePreviewInput(7); 
  }
  if (inputSelect.isButtonIn(4, busSelection))  { 
    AtemSwitcher.changePreviewInput(8); 
  }
  if (inputSelect.isButtonIn(5, busSelection))  { 
    AtemSwitcher.changePreviewInput(1); 
  }
  if (inputSelect.isButtonIn(6, busSelection))   { 
    AtemSwitcher.changePreviewInput(2); 
  }
  if (inputSelect.isButtonIn(7, busSelection))  { 
    AtemSwitcher.changePreviewInput(3); 
  }
  if (inputSelect.isButtonIn(8, busSelection))  { 
    AtemSwitcher.changePreviewInput(4); 
  }



  // Reading buttons from the Command BI8 board:
  uint8_t cmdSelection = cmdSelect.buttonDownAll();
  if (cmdSelection & (B1 << 0))  { 
    commandCut();
  }
  if (cmdSelection & (B1 << 2))  { 
    commandAuto();
  }
  if (cmdSelection & (B1 << 3))  {
    commandPIP();
  }
  if (cmdSelection & (B1 << 6))  { 
    commandDSK1();
  }
  if (cmdSelection & (B1 << 7))  {
    commandPIPVGA();
  }  
}


