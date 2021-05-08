import Radio
import Thermostat

if __name__ == "__main__":
    r = Radio.Radio()
    t = Thermostat.SmartThermostat(r)
    r.mainLoop(t.mainLoop)
