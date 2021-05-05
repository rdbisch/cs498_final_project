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
        self.packetCount = 0
        self.badPackets = 0
    
    def mainLoop(self, recvCallback):
        while True:
            self.packetCount += 1

            if self.packetCount % 1000 == 0:
                print("Packets Received {} Bad {} % {}".format(self.packetCount, self.badPackets, float(self.badPackets/self.packetCount)))

            packet = self.rfm9x.receive(with_header=True, keep_listening=True, with_ack=True)
            if packet == None:
                time.sleep(0.1)
                continue
           
            # Received a packet!
            # Print out the raw bytes of the packet:
            #print("Received (raw header):", [hex(x) for x in packet[0:4]])
            #print("Received (raw payload): {0}".format(packet[4:]))
            #print("RSSI: {0}".format(self.rfm9x.last_rssi))
            try:
                print("<<< {}".format(packet[4:]))
                recvCallback(str(packet[4:], "utf-8"))
            except UnicodeDecodeError as err:
                print("### Error encoding packet {}\n{}".format(packet, err))
                self.badPackets += 1

    def send(self, data):
        print(">>> {}".format(data))
        self.rfm9x.send(bytearray(data, "utf-8"))
