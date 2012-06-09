/*****************
 * Basis control for the SKAARHOJ C100 models
 * This example is programmed for ATEM 1M/E versions
 *
 * This example also uses a number of custom libraries which you must install first. 
 * Search for "#include" in this file to find the libraries. Then download the libraries from http://skaarhoj.com/wiki/index.php/Libraries_for_Arduino
 *
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
  0x90, 0xA2, 0xDA, 0x00, 0xE6, 0x73 };      // <= SETUP!
IPAddress ip(192, 168, 0, 20);              // <= SETUP!


// Include ATEM library and make an instance:
#include <ATEM.h>

// Connect to an ATEM switcher on this address and using this local port:
// The port number is chosen randomly among high numbers.
ATEM AtemSwitcher;


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
  
    // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin(); 
  
    // Set up the SkaarhojBI8 boards:
  inputSelect.begin(0,false);
  cmdSelect.begin(1,false);
  
  inputSelect.setDefaultColor(0);  // Off by default
  cmdSelect.setDefaultColor(0);  // Off by default
  
  inputSelect.testSequence();
  cmdSelect.testSequence();


  // Initialize a connection to the switcher:
  AtemSwitcher.begin(IPAddress(192, 168, 0, 50), 56417);    // <= SETUP!
//  AtemSwitcher.serialOutput(true);
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
    
    // Setting colors of the command buttons:
    cmdSelect.setButtonColor(3, AtemSwitcher.getTransitionPosition()>0 ? 4 : 5);     // Auto button
    cmdSelect.setButtonColor(8, preVGA_active ? 4 : 5);     // VGA+PIP button
    cmdSelect.setButtonColor(4, AtemSwitcher.getUpstreamKeyerStatus(1) ? 4 : 5);     // PIP button
    cmdSelect.setButtonColor(7, AtemSwitcher.getDownstreamKeyerStatus(1) ? 4 : 5);    // DSK1 button
    if (!cmdSelect.buttonIsPressed(1))  {
      cmdSelect.setButtonColor(1, 5);   // de-highlight CUT button
    }
  
  
    // Sending commands:
    uint8_t busSelection = inputSelect.buttonDownAll();
    if (inputSelect.isButtonIn(1, busSelection))  { AtemSwitcher.changePreviewInput(5); }
    if (inputSelect.isButtonIn(2, busSelection))   { AtemSwitcher.changePreviewInput(6); }
    if (inputSelect.isButtonIn(3, busSelection))   { AtemSwitcher.changePreviewInput(7); }
    if (inputSelect.isButtonIn(4, busSelection))  { AtemSwitcher.changePreviewInput(8); }
    if (inputSelect.isButtonIn(5, busSelection))  { AtemSwitcher.changePreviewInput(1); }
    if (inputSelect.isButtonIn(6, busSelection))   { AtemSwitcher.changePreviewInput(2); }
    if (inputSelect.isButtonIn(7, busSelection))  { AtemSwitcher.changePreviewInput(3); }
    if (inputSelect.isButtonIn(8, busSelection))  { AtemSwitcher.changePreviewInput(4); }
  
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
    if (cmdSelection & (B1 << 2))  { AtemSwitcher.doAuto(); preVGA_active = false;}
    if (cmdSelection & (B1 << 3))  {
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
    }  // PIP
    
    if (cmdSelection & (B1 << 6))  { AtemSwitcher.changeDownstreamKeyOn(1, !AtemSwitcher.getDownstreamKeyerStatus(1)); }  // DSK1
    if (cmdSelection & (B1 << 7))  { // VGA + PIP
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
  }
}
