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

# Configure LoRa Radio
CS = DigitalInOut(board.CE1)
RESET = DigitalInOut(board.D25)
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
rfm9x = adafruit_rfm9x.RFM9x(spi, CS, RESET, 915.0)
rfm9x.tx_power = 23
prev_packet = None

# From https://learn.adafruit.com/adafruits-raspberry-pi-lesson-11-ds18b20-temperature-sensing/software
base_dir = '/sys/bus/w1/devices/'
device_folder = glob.glob(base_dir + '28*')[0]
device_file = device_folder + '/w1_slave'
 
def read_temp_raw():
    f = open(device_file, 'r')
    lines = f.readlines()
    f.close()
    return lines
 
def read_temp():
    lines = read_temp_raw()
    while lines[0].strip()[-3:] != 'YES':
        time.sleep(0.2)
        lines = read_temp_raw()
    equals_pos = lines[1].find('t=')
    if equals_pos != -1:
        temp_string = lines[1][equals_pos+2:]
        temp_c = float(temp_string) / 1000.0
        temp_f = temp_c * 9.0 / 5.0 + 32.0
        return temp_c, temp_f

#class Thermostat:
#    def __init__(self):
#        self.nodes = { 2**i : None for i in range(1, 32) }
#        self.

tempSum = 0
tempCount = 0
tempSumSq = 0
badPackets = 0

while True:
    print("Temp at thermostat is {}".format(read_temp()))
    packet = None
    # check for packet rx
    packet = rfm9x.receive(with_header=True)
    if packet is None:
        print("- Waiting for PKT -")
    else:
        # Received a packet!
        # Print out the raw bytes of the packet:
        print("Received (raw header):", [hex(x) for x in packet[0:4]])
        print("Received (raw payload): {0}".format(packet[4:]))
        print("RSSI: {0}".format(rfm9x.last_rssi))

        try:
            test = str(packet[4:], "cp437")
            temp = float(test.split(' ')[3])
            tempSum += temp
            tempSumSq += temp*temp
            tempCount += 1
        except ValueError:
            badPackets += 1

        if tempCount % 10 == 0:
            mean = tempSum / tempCount
            var = tempSumSq / tempCount - mean*mean
            data = "average temp {} std {} badpackets {}\x00".format(mean, var, badPackets)
            rfm9x.send(bytearray(data, "cp437"))

        # send reading after any packet received
        # Display the packet text and rssi
        #print(str(packet, "utf-8"))
        #print(packet)
        time.sleep(1)

    time.sleep(0.1)
