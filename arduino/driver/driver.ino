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
#include <SPI.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RH_RF95.h>
#include <Servo.h>

// Data wire is plugged into digital pin 2 on the Arduino
#define ONE_WIRE_BUS 10

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

/* Comment out for real run */
/* #define DEBUG */

/* Which pin is servo on */
#define SERVO_PIN 11
Servo baffle;
int baffle_pos;
#define BAFFLE_OPEN 90
#define BAFFLE_CLOSED -90

/* For battery measurement 
 *  https://learn.adafruit.com/adafruit-feather-32u4-basic-proto/power-management
 */
#define VBATPIN A9

/* for feather32u4 */
#define RFM95_CS 8
#define RFM95_RST 4
#define RFM95_INT 7

// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0
 
// Singleton instance of the radio driver
RH_RF95 rf95(RFM95_CS, RFM95_INT);

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
    Serial.println("Uncomment '#define SERIAL_DEBUG' in RH_RF95.cpp for");
    Serial.println(" detailed debug info");
#endif
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(250);
      digitalWrite(LED_BUILTIN, LOW);
      delay(250);
    }
  }
#ifdef DEBUG
  Serial.println("LoRa radio init OK!");
#endif
  // Defaults after init are 434.0MHz, modulation GFSK_Rb250Fd250, +13dbM
  if (!rf95.setFrequency(RF95_FREQ)) {
#ifdef DEBUG
      Serial.println("setFrequency failed");
#endif
      while (1) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(250);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
      }
  }
#ifdef DEBUG
  Serial.print("Set Freq to: "); Serial.println(RF95_FREQ);
#endif

  rf95.setTxPower(23, false);

  /**** ****/
  sensors.begin();  // Start up the library

  /*** Setup Servo ***/
  baffle.attach(SERVO_PIN);
  baffle_open();
  delay(15);

#ifdef DEBUG
  Serial.println("\n\n\nMAIN LOOP STARTING\n\n\n");  
#endif

}

float getTemp() {
  // Send the command to get temperatures
  sensors.requestTemperatures(); 
  return sensors.getTempCByIndex(0);
}
 
int16_t packetnum = 0;  // packet counter, we increment per xmission
int state = 0;

void loop() {
  float v = analogRead(VBATPIN);
  v *= 2;    // we divided by 2, so multiply back
  v *= 3.3;  // Multiply by 3.3V, our reference voltage
  v /= 1024; // convert to voltage

  float f = getTemp();
  char radiopacket[30];
  sprintf(radiopacket,
    "1 POST %u %d.%02d %d.02%d",
    sr_addr,
    (int)f,
    (int)(f*100)%100,
    (int)v,
    (int)(v*100)%100);
    
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

        buf[len] = 0;
#ifdef DEBUG
        Serial.print("<");
        Serial.println((char*)buf);
#endif
        int ret = sscanf(buf, "%u %n", &check_addr, &bytes);
        if (ret < 1 || (check_addr > 0 && (check_addr != sr_addr))) {
#ifdef DEBUG
          Serial.println("address did not match pattern.\n");
#endif          
          goto next;
        }

        rest = buf + bytes;
        if (rest[0] == 'S') setFlag(rest);
        else if (rest[0] == 'B') setBaffle(rest);
        else {
          ;
#ifdef DEBUG
          Serial.println("Unhandled command in message.");
          Serial.println((char*)buf);
          Serial.println("************");
#endif          
        }
    }
  }
  /* Blink the LED so we can see things are still working. */
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}

/** Handle SETFLAG message **/
void setFlag(uint8_t* message) {
#ifdef DEBUG
  Serial.println("in setFlag....sr_flag was...");
  Serial.println((char*)message);
  Serial.println(sr_flag);
#endif  
  sscanf(message, "SETFLAG %d", &sr_flag);
#ifdef DEBUG
  Serial.println(sr_flag);
#endif  
}

/** Handle BAFFLE message **/
void setBaffle(uint8_t* message) {
  int flags;
  sscanf(message, "BAFFLE %d", &flags);

  int r = (flags & sr_flag);
#ifdef DEBUG  
  Serial.println("setBaffle debug  flags, sr_flags, r");
  Serial.println(flags);
  Serial.println(sr_flag);
  Serial.println(r);
#endif  
  if (r) baffle_open();
  else baffle_close();
}

void baffle_open() {
  if (baffle_pos != BAFFLE_OPEN) {
#ifdef DEBUG    
      Serial.println("OPENING BAFFLE");
#endif      
      baffle.write(BAFFLE_OPEN);
      baffle_pos = BAFFLE_OPEN;
  }    
}

void baffle_close() {
  if (baffle_pos != BAFFLE_CLOSED) {
#ifdef DEBUG    
      Serial.println("CLOSING BAFFLE");
#endif      
      baffle.write(BAFFLE_CLOSED);
      baffle_pos = BAFFLE_CLOSED;
  }    
}
