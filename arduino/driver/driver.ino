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

/* Comment out for real run */
#define DEBUG

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
unsigned int sr_addr;      // Unique ID generated at startup
int sr_flag;      // Bit mask flag set by server
int sr_state = SR_UNDEFINED;
 
void setup() {
  pinMode(RFM95_RST, OUTPUT);
  digitalWrite(RFM95_RST, HIGH);


#ifdef DEBUG
  Serial.begin(115200);
  while (!Serial) {
      delay(1);
  }
#endif
  delay(100);

  randomSeed(analogRead(0));
  sr_addr = random();
  while (sr_addr == 0) sr_addr = random();
#ifdef DEBUG  
  Serial.println("Random integer is ");
  Serial.println(sr_addr);
#endif

#ifdef DEBUG
  Serial.println("Feather LoRa TX Test!");
#endif
  // manual reset
  digitalWrite(RFM95_RST, LOW);
  delay(10);
  digitalWrite(RFM95_RST, HIGH);
  delay(10);

  while (!rf95.init()) {
#ifdef DEBUG
    Serial.println("LoRa radio init failed");
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for detailed debug info");
#endif
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
      delay(250);                       // wait for a second
      digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
      delay(250);           
    }
  }
#ifndef DEBUG
  Serial.println("LoRa radio init OK!");
#endif
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
#ifdef DEBUG
      Serial.println("setFrequency failed");
#endif
      while (1) {
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(250);                       // wait for a second
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
        delay(500);          
      }
  }
#ifdef DEBUG
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
#endif
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  rf95.setTxPower(23, false);

  /*** Get address ***/  

  char radiopacket[32];
  sprintf(radiopacket, "1 ADDR %u", sr_addr);
  rf95.send((uint8_t *)radiopacket, strlen(radiopacket));

  /** TODO MOVE TO LOOP ***/
  /*** Wait for address reply ***/
#ifdef DEBUG    
  Serial.println("Getting address from controller.");
  Serial.print("sent ");
  Serial.println(radiopacket);
  Serial.println("Waiting for address...");
#endif
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  /* Loop listening to radio until we get a flag assigned. */
  while (1) {
    if (rf95.waitAvailableTimeout(1000)) { 
      // Should be a reply message for us now   
      if (rf95.recv(buf, &len)) {
  #ifdef DEBUG
        Serial.print("Got reply: ");
        Serial.println((char*)buf);
        Serial.print("RSSI: ");
        Serial.println(rf95.lastRssi(), DEC);    
  #endif
        unsigned int check_addr;
        int bytes;
        uint8_t* rest = 0;
        int ret = sscanf(buf, "%u %n", &check_addr, &bytes);
        if (ret < 1 || check_addr != sr_addr) {
  #ifdef DEBUG
          Serial.println("Got message meant for");
          Serial.println(check_addr);
          Serial.println("ret is first, sr_addr is second.");
          Serial.println(ret);
          Serial.println(sr_addr);
  #endif        
          continue;
        } else {
          rest = buf + bytes;
  #ifdef DEBUG
          Serial.println("Rest is");
          Serial.println((char*)rest);
  #endif
          ret = sscanf(rest, "SETFLAG %d", &sr_flag);
          if (ret == 1) break;
  #ifdef DEBUG
          Serial.println("sscanf failed.\n");
          while (1);
  #endif
          
        }
      }
    }
  }

  /******* PCT2075 Setup **********/
  PCT2075 = Adafruit_PCT2075();
  while (1) {
    if (!PCT2075.begin()) {
#ifdef DEBUG          
      Serial.println("Couldn't find PCT2075 chip");
#endif
      while (1) {
        digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
        delay(500);                       // wait for a second
        digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
        delay(250);              
      }
    } else {
      break;
    }
  }
}
 
int16_t packetnum = 0;  // packet counter, we increment per xmission
 
void loop()
{

  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(500);  

  float f = PCT2075.getTemperature();
  char radiopacket[30];
  sprintf(radiopacket, "1 POST %u %d.%02d", sr_addr, (int)f, (int)(f*100)%100);

#ifdef DEBUG
  Serial.print("Sending "); Serial.println(radiopacket);
#endif

  rf95.send((uint8_t *)radiopacket, strlen(radiopacket));
  delay(10);
  rf95.waitPacketSent();

  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);

  if (rf95.waitAvailableTimeout(1000))
  { 
    // Should be a reply message for us now   
    if (rf95.recv(buf, &len))
    {
#ifdef DEBUG      
      Serial.print("Got reply: ");
      Serial.println((char*)buf);
      Serial.print("RSSI: ");
      Serial.println(rf95.lastRssi(), DEC);    
#endif
    }
    else
    {
#ifdef DEBUG
      Serial.println("Receive failed");
#endif
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
  //delay(7000);
}
