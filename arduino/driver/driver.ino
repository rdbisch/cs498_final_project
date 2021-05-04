/* driver.ino
 * rdb4 4/10/2021
 *
 * This is the main driver code for the floor register hardware.
 * It is based off sample code available in
 * external_code/lora_test.ino
 * external_code/pct2075_test.ino
 * and
 * Servo demo code sweep
 */

#include <Adafruit_PCT2075.h>
#include <SPI.h>
#include <RH_RF95.h>

/* for feather32u4 */
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
 
// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

Adafruit_PCT2075 PCT2075;

/* SmartRegister settings */
#define SR_UNDEFINED 0xDEADBEEF
#define SR_OPEN = 0
#define SR_CLOSED = 1
int sr_addr;
int sr_state = SR_UNDEFINED;
 
void setup() {
    pinMode(RFM95_RST, OUTPUT);
    digitalWrite(RFM95_RST, HIGH);

    Serial.begin(115200);
    while (!Serial) {
        delay(1);
    }

    delay(100);

    Serial.println("Feather LoRa TX Test!");

    // manual reset
    digitalWrite(RFM95_RST, LOW);
    delay(10);
    digitalWrite(RFM95_RST, HIGH);
    delay(10);

    while (!rf95.init()) {
        Serial.println("LoRa radio init failed");
        Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
        while (1);
    }
    Serial.println("LoRa radio init OK!");

    // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
    if (!rf95.setFrequency(RF95_FREQ)) {
        Serial.println("setFrequency failed");
        while (1);
    }
    Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);

    // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

    // The default transmitter power is 13dBm, using PA_BOOST.
    // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
    // you can set transmitter powers from 5 to 23 dBm:
    rf95.setTxPower(23, false);

    /*** Get address ***/
    Serial.println("Getting address from controller.");
    char radiopacket[10] = "ADDR";
    rf95.send((uint8_t *)radiopacket, 4);

    /*** Wait for address reply ***/
    Serial.println("Waiting for address...");
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
    
    if (rf95.waitAvailableTimeout(1000)) { 
      // Should be a reply message for us now   
      if (rf95.recv(buf, &len)) {
        sscanf(buf, "SETADDR %d", &sr_addr);
        Serial.print("Got reply: ");
        Serial.println((char*)buf);
        Serial.println("Set address to: ");
        Serial.println(sr_addr);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);    
      }
    }

    /******* PCT2075 Setup **********/
    PCT2075 = Adafruit_PCT2075();
    while (1) {
        if (!PCT2075.begin()) {
            Serial.println("Couldn't find PCT2075 chip");
            delay(1000);
        } else {
            Serial.println("Fantastic!");
            break;
        }
    }
    Serial.println("Found PCT2075 chip");
}
 
int16_t packetnum = 0;  // packet counter, we increment per xmission
 
void loop()
{
  
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(500);  
  
  Serial.println("Transmitting..."); // Send a message to rf95_server

  float f = PCT2075.getTemperature();
  char radiopacket[30];
  sprintf(radiopacket, "POST %d %d.%02d", sr_addr, (int)f, (int)(f*100)%100);

  Serial.print("Sending "); Serial.println(radiopacket);
  radiopacket[29] = 0;
  
  Serial.println("Sending...");
  delay(10);
  rf95.send((uint8_t *)radiopacket, strlen(radiopacket));
 
  Serial.println("Waiting for packet to complete..."); 
  delay(10);
  rf95.waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
 
  Serial.println("Waiting for reply...");
  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
   {
      Serial.print("Got reply: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);    
    }
    else
    {
      Serial.println("Receive failed");
    }
  }
  else
  {
    Serial.println("No reply, is there a listener around?");
  }
  Serial.println("Going to sleep for 8 seconds.");

  // Mimic sleeping
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(500);  
  delay(7000);
}
