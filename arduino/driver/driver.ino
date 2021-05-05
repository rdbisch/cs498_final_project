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
int state = 0;

void loop() {

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

  /**** RECEIVE LOOP ********/
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
next:
  if (rf95.waitAvailableTimeout(1000)) { 
    if (rf95.recv(buf, &len)) {
        unsigned int check_addr;
        int bytes;
        uint8_t* rest = 0;
        
        int ret = sscanf(buf, "%u %n", &check_addr, &bytes);
        if (ret < 1 || check_addr != sr_addr) {
          goto next;
        }

        rest = buf + bytes;
        if (rest[0] == 'R') setFlag(bytes + ret);
        else if (rest[0] == 'S') setBaffle(bytes + ret);
        else {
          Serial.println("Unhandled command in message.");
          Serial.println((char*)buf);
          Serial.println("************");
        }
    }
  }

  // Mimic sleeping
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(500);                       // wait for a second
  digitalWrite(LED_BUILTIN, LOW);    // turn the LED off by making the voltage LOW
  delay(500);  
  //delay(7000);
}

/** Handle SETFLAG message **/
void setFlag(uint8_t* message) {
  sscanf(message, "SETFLAG %d", &sr_flag);
}

/** Handle BAFFLE message **/
void setBaffle(uint8_t* message) {
  ;
}
