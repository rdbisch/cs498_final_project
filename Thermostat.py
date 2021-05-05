# Thermostat.py
# Smart Thermostat to control Smart Registers
import time
import math

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
        self.state = SR_OPEN
        self.addr = addr
        self.flag = flag
        self.lastReading = None

    def registerReading(self, temp):
        self.lastReading = time.time()
        self.temps.append(temp)
        if len(self.temps) > queue_size:
            self.temps.pop(0)

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
        self.registers = {}
        self.ambientTemp = []

    def readTemp():
        t = TempSensor.read_temp()
        self.ambientTemp ++ t
        if len(self.ambientTemp) > queue_size:
            self.ambientTemp.pop(0)

    # Ordinary clock tick with no radio activity
    def tick(self):
        # Read ambient temperature
        readTemp()
        # Decide if we should turn on thermostat?
        # Decide if we've lost sensors
        # Decide if we should open/close rooms

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
            print("Command was -->|{}|<--".format(mesgs[1]))
            for k in commands.keys():
                print("\t{} == {}?   {}".format(k, mesgs[1], k == mesgs[1]))
            raise ThermostatException("Unknown command {} with args {} in SmartThermostat.recv.".format(mesgs[1], mesg))
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
            flag = int(math.log2(max_flag)) + 1
        if flag > 31:
            raise ThermostatException("Out of flags in SmartThermostat.")

        addr = int(args[0])
        flag = 2**flag
        reg = SmartRegister(addr, flag)
        assert(addr not in self.registers)
        self.registers[addr] = reg
        self.radio.send("{} SETFLAG {}".format(addr, flag))

    def post(self, args):
        addr = int(args[0])

        # This can happen if we reboot the thermostat
        # while registers are still running
        if addr not in self.registers:
            self.addr(args)
        
        try:
            temp = float(args[1])
            self.registers[addr].registerReading(temp)
        except ValueError as err:
            pass

    # A human changed the register setting manually.
    def manual(self, args):
        addr = int(args[0])
        assert(addr in self.registers)
        self.registers[addr].flipState()