# Thermostat.py
# Smart Thermostat to control Smart Registers
import time

# Local temp
import TempSensor

# How many readings to keep local
queue_size = 10

# This is an abstract data structure
#  to represent a (remote) physical device.
SR_UNDEFINED = 2
SR_OPEN = 0
SR_CLOSED = 1
class SmartRegister:
    def __init__(self, addr):
        self.temps = []
        self.state = SR_OPEN
        self.addr = addr
        self.lastReading = None

    def registerReading(self, temp):
        self.lastReading = time.time()
        self.temps ++ temp
        if len(self.temps) > queue_size:
            self.temps.pop_front()

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
            raise "Register in unknown state"

class SmartThermostat:
    def __init__(self, radio):
        self.radio = radio
        self.registers = {}
        self.ambientTemp = []
        self.mode = HEAT

    def readTemp():
        t = TempSensor.read_temp()
        self.ambientTemp ++ t
        if len(self.ambientTemp) > queue_size:
            self.ambientTemp.pop_front()

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
        if (mesgs[0] not in commands):
            raise "Unknown command {} with args {} in SmartThermostat.recv.".format(mesgs[0], msg)
        else:
            commands[mesgs[0]](mesg[1:])
    
    def addr(self, args):
        # Find first unused address.
        if len(registers) == 0: addr = 1
        else: addr = int(math.log2(max(registers.keys()))) + 1
        if addr > 31:
            raise "Out of addresses in SmartThermostat."

        addr = 2**i
        reg = SmartRegister(addr)
        assert(addr not in registers)
        registers[addr] = reg

    def post(self, args):
        addr = int(args[0])
        assert(addr in registers)
        registers[addr].registerReading(float(args[1]))

    # A human changed the register setting manually.
    def manual(self, args):
        addr = int(args[0])
        assert(addr in registers)
        registers[addr].flipState()