# Solax X3 EV Charger

Connects nymea to a Solax wallbox. Currently supported models are:

* Solax X3 EV Charger

**!!!!!! IMPORTANT !!!!!!**  
The current implementation only works in combination with the Solax X3 Inverter, as the Modbus Registers shift, when the Inverter is used as Master.  
While the EV Charger is connected via Modbus TCP, the EVC is connected via RS485 to the Inverter.

Register Calculation:
Register(INV) = Register(EVC) - 0x600 + 0x1000

# Requirements

nymea requires the use of a firmware greator or equal version 1.12 on the wallbox.

