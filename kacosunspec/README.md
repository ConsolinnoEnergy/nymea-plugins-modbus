# Kaco inverter SunSpec

Connects to a Kaco inverter that supports the SunSpec protocol, using Modbus RTU or TCP.

## Supported Things

Tested with Kaco blueplanet NX3 M2.

Should be compatible with all three phase Kaco inverters that implement SunSpec model 103.

## Configuration for NX3 models

* Modbus TCP  
A Modbus TCP connection to the NX3 is only possible via wifi. This requires the wifi module to be plugged into the inverter. 
The device running nymea needs to be connected to a wifi router, and the Kaco inverter needs to be setup to connect to that wifi router. 
How to connect the Kaco wifi module to a wifi router is described in the Kaco manual.  
The Kaco wifi module needs to be configured to enable Modbus TCP. The Modbus configuration screen also shows you the IP address 
of the inverter and the Modbus ID. These are required to set up the nymea plugin.  
Make sure that no cable is plugged into the RJ45 connector of the Kaco inverter. A connected cable will disrupt Modbus TCP communication.

* Modbus RTU  
The cable for the Modbus RT connection is plugged into the RJ45 connector of the Kaco inverter. How to wire the contacts is described in the manual. 
For Modbus RTU operation, the Kaco wifi module needs to be removed, as it disrupts Modbus RTU communication.  
The parameters for the RTU connection are: baud rate 9600, data bits 8, stop bits 1, parity none. The default Modbus ID is 3, but that can change 
depending on your inverter configuration.
