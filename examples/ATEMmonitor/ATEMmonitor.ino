/*****************
 * Example: ATEM Monitor
 * Connects to the Atem Switcher and outputs changes to Preview and Program on the Serial monitor (at 9600 baud)
 *
 * - kasper
 */
/*****************
 * TO MAKE THIS EXAMPLE WORK:
 * - You must have an Arduino with Ethernet Shield (or compatible such as "Arduino Ethernet", http://arduino.cc/en/Main/ArduinoBoardEthernet)
 * - You must have an Atem Switcher connected to the same network as the Arduino - and you should have it working with the desktop software
 * - You must make specific set ups in the below lines where the comment "// SETUP" is found!
 */





#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>


// MAC address and IP address for this *particular* Ethernet Shield!
// MAC address is printed on the shield
// IP address is an available address you choose on your subnet where the switcher is also present:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x00, 0xE8, 0xE9 };		// <= SETUP
IPAddress ip(192, 168, 0, 22);				// <= SETUP


// Include ATEM library and make an instance:
#include <ATEM.h>

// Connect to an ATEM switcher on this address and using this local port:
// The port number is chosen randomly among high numbers.
ATEM AtemSwitcher(IPAddress(192, 168, 0, 50), 56417);  // <= SETUP (the IP address of the ATEM switcher)



void setup() { 

  // Start the Ethernet, Serial (debugging) and UDP:
  Ethernet.begin(mac,ip);
  Serial.begin(9600);  
  Serial.println("Serial started.");

  // Initialize a connection to the switcher:
  AtemSwitcher.serialOutput(true);
  AtemSwitcher.connect();
}

void loop() {
      // Check for packets, respond to them etc. Keeping the connection alive!
      // VERY important that this function is called all the time - otherwise connection might be lost because packets from the switcher is overlooked and not responded to.
  AtemSwitcher.runLoop();

    // If connection is gone anyway, try to reconnect:
  if (AtemSwitcher.isConnectionTimedOut())  {
     Serial.println("Connection to ATEM Switcher has timed out - reconnecting!");
     AtemSwitcher.connect();
  }  
  
    // If you fancy to make delays in your sketches, ALWAYS do it using the AtemSwitcher delay function - this will wait while calling runLoop() checking for packets and thus keeping the connection up.
  AtemSwitcher.delay(5000);


    // If you monitor the serial output, you should see a lot of "ACK, rpID: xxxx" and then every 5 seconds this message:
  Serial.println("End of normal loop() - still kicking?");
  
    // Now, try also to disconnect the network cable - and see if it reconnects properly.
}