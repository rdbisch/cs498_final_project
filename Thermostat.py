# Thermostat.py
# Smart Thermostat to control Smart Registers
from datetime import datetime, timedelta
import math
import pandas as pd

# Local temp
import TempSensor

# How many readings to keep local
queue_size = 10

class ThermostatException(Exception):
    pass

# This is an abstract data structure
#  to represent a (remote) physical device.
SR_UNDEFINED = 2
SR_OPEN = 0
SR_CLOSED = 1
class SmartRegister:
    def __init__(self, addr, flag):
        self.temps = []
        self.dates = []
        self.flags = []
        self.voltage = []     
        self.state = SR_OPEN
        self.addr = addr
        self.flag = flag

    def registerReading(self, temp, flag, voltage):        
        self.temps.append(temp)
        self.dates.append(datetime.now())
        self.flags.append(int((flag & self.flag) > 0))
        self.voltage.append(voltage)

    # Make a neatly formattd string representation 
    # this will be printed to main console 
    def __str__(self):
        t = datetime.now()
        temp = self.temps[-1]
        date = self.dates[-1]
        flag = self.flags[-1]
        voltage = self.voltage[-1]

        d = t - date      
        if d < timedelta(days = 1):
            time_str = "{:02d}:{:02d}".format(date.hour, date.minute)
        else:
            time_str = "{:02d}/{:02d}".format(date.month, date.days)

        if voltage != None:
            s = "r={0} {4:4.2f}v @{1} t={2:6.2f} flag={3}"
        else:
            s = "r={0}   NAv @{1} t={2:6.2f} flag={3}"

        return s.format(self.addr, time_str, temp, flag, voltage)

    # This is meant to be called periodically 
    #  to keep memory footprint low.
    # In the curren design of an update every 8s, a 2-week update
    #  should have about 2.4k rows, so hopefully 10k memory is ok (per register)
    def archiveData(self, filename, asof, delta): 
        # Otherwise, just dump the data out to a CSV
        #  Another ocmponent will read this in for analytics later
        df = pd.DataFrame({
            "temperature": self.temps,
            "datetime": self.dates,
            "flags": self.flags,
            "voltage": self.voltage
        })
        df.to_csv(filename)

        # Now we truncate the data to be fresher than "delta"

        # Find the first date in dates that is older than days old.
        #  (remember these lists are all stored in increasing order of time)
        # so as long as "asof" is newer than anything in the list,
        # all we have to do is find the first element younger than our delta.
        found = None
        for idx, t in enumerate(self.dates):
            if asof - t < delta:
                found = idx
                break

        # If we didn't actually find anything,
        #  no truncation has to happen.
        if found == None: return
 
        # Truncate internal structure
        self.temps = self.temps[found:]
        self.dates = self.dates[found:]
        self.flags = self.flags[found:]
        self.voltage = self.voltage[found:]

    def openRegister(self):
        assert(self.state == SR_CLOSED)
        self.state = SR_OPEN

    def closeRegister(self):
        assert(self.state == SR_OPEN)
        self.state = SR_CLOSED

    def flipState(self):
        if self.state == SR_CLOSED: self.openRegister()
        elif self.state == SR_OPEN: self.closeRegister()
        else: 
            raise ThermostatException("Register in unknown state")


class SmartThermostat:
    def __init__(self, radio):
        self.radio = radio
        # This houses all of our discovered smart registers.  The key is their
        #  pseudo-MAC.   The initial value 0 represent this device, i.e. the thermostat's
        #  own temperature reading.
        self.registers = { 0: SmartRegister(0,0) }
        self.flags = 0
        self.lastTick = datetime.now()
        self.lastSave = self.lastTick
        self.TICK_FREQUENCY = timedelta(seconds = 4)
        self.SAVE_FREQUENCY = timedelta(seconds = 30)
        self.demo = 0

    def archiveData(self, filename):
        with open(filename, "w") as f:
            f.write("registers\n")
            for addr in self.registers.keys():
                f.write(str(addr) + "\n")

    def readTemp(self):
        t = TempSensor.read_temp()
        self.registers[0].registerReading(t, 0, None)  #TODO Change this to system ON or OFF.

    # This is called by radio
    #  to process events.
    def mainLoop(self, mesg):
        self.recv(mesg)

        # Figure out if we should do a self-update
        t = datetime.now()
        if t - self.lastTick > self.TICK_FREQUENCY:
            self.tick()
            self.lastTick = t

        # Store off data
        if t - self.lastSave > self.SAVE_FREQUENCY:
            print("============= DATA FLUSH ===============\n")
            # isoformat() returns "2021-05-06T14:01:13.313976"
            # [0:13] truncates 2 digits after T
            suffix = t.isoformat()[0:13]
            self.archiveData("data/smart_{}.csv".format(suffix))
            for r in self.registers.values():
                r.archiveData(                    
                    "data/register_{}_{}.csv".format(r.addr, suffix),
                    t,
                    timedelta(days=14))
            self.lastSave = t

    # Ordinary clock tick with no radio activity
    def tick(self):
        # Read ambient temperature
        self.readTemp()
        # Update log
        outstr = "{0:3d}/{1:5d} (bad/total)pckt   ".format(
            self.radio.badPackets,
            self.radio.packetCount)  
        for r in self.registers.values():
            outstr += r.__str__() + "   "
        print(outstr)

        ambientTemp = self.registers[0].temps[-1]
        # Decide if we should turn on thermostat?
        if ambientTemp < 23:
            # Turn on thermostat
            # Switch relay if we have one...

            # Figure out what rooms to open or close.
            if self.demo % 4 == 0: threshold = 21
            else: threshold = 23
            self.demo += 1

            temp = 0
            for r in self.registers.values():
                if r.temps[-1] < threshold: temp += r.flag
            if temp != self.flags:
                self.flags = temp
                self.radio.send("0 BAFFLE {} ".format(self.flags));

        # Decide if we've lost sensors

    def recv(self, mesg):
        commands = { "ADDR": self.addr,
                     "POST": self.post,
                     "MAN": self.manual }
        mesgs = mesg.split(' ')
        # Our protocol is <addr> <command> <args>
        # Since we are the server, our valid address is 0 (broadcast) or 1.
        #  So if its not here, ignore the message

        addr = int(mesgs[0])
        if (addr != 0 and addr != 1): return

        if (mesgs[1] not in commands):
            t = "Unknown command {} with args {} in SmartThermostat.recv."
            print(t.format(mesgs[1], mesg))
        else:
            commands[mesgs[1]](mesgs[2:])
    
    def addr(self, args):
        # Message is of form
        # 1 ADDR abcde
        #
        # where abcde is the register's unique address.

        # Find first unused flag.
        if len(self.registers) == 0: flag = 1
        else: 
            max_flag = max([ r.flag for r in self.registers.values()])
            # == 0 is our own flag, so we hardcode a bypass
            if max_flag == 0: flag = 1
            else: flag = int(math.log2(max_flag)) + 1
        if flag > 31:
            raise ThermostatException("Out of flags in SmartThermostat.")

        addr = int(args[0])
        flag = 2**flag
        reg = SmartRegister(addr, flag)
        assert(addr not in self.registers)
        self.registers[addr] = reg
        self.radio.send("{} SETFLAG {} ".format(addr, flag))

    def post(self, args):
        addr = int(args[0])

        # This can happen if we reboot the thermostat
        # while registers are still running
        if addr not in self.registers:
            self.addr(args)
        
        try:
            temp = float(args[1])
            voltage = float(args[2])
            self.registers[addr].registerReading(temp, self.flags, voltage)
        except ValueError as err:
            pass

    # A human changed the register setting manually.
    def manual(self, args):
        addr = int(args[0])
        assert(addr in self.registers)
        self.registers[addr].flipState()