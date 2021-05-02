"""
Example for using the RFM9x Radio with Raspberry Pi.

Learn Guide: https://learn.adafruit.com/lora-and-lorawan-for-raspberry-pi
Author: Brent Rubell for Adafruit Industries
"""
# Import Python System Libraries
import time
# Import Blinka Libraries
import busio
from digitalio import DigitalInOut, Direction, Pull
import board
# Import RFM9x
import adafruit_rfm9x
import glob

class Radio:
    def __init__(self):
        # Configure LoRa Radio
        self.CS = DigitalInOut(board.CE1)
        self.RESET = DigitalInOut(board.D25)
        self.spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
        self.rfm9x = adafruit_rfm9x.RFM9x(self.spi, self.CS, self.RESET, 915.0)
        self.rfm9x.tx_power = 23
        self.prev_packet = None
    
    def mainLoop(self, recvCallback):
        while True:
            packet = self.rfm9x.receive(with_header=True, keep_listening=True)
            if packet == None:
                time.sleep(0.1)
                continue
           
            # Received a packet!
            # Print out the raw bytes of the packet:
            #print("Received (raw header):", [hex(x) for x in packet[0:4]])
            #print("Received (raw payload): {0}".format(packet[4:]))
            #print("RSSI: {0}".format(self.rfm9x.last_rssi))
            recvCallback(str(packet[4:], "cp437"))

    def send(msg):
        self.rfm9x.send(bytearray(data, "cp437"))
