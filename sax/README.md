# sax battery storage
--------------------------------

Connect nymea to sax battery storage over Modbus TCP and get soc and power values.


## Notes

* the following statetypes might be not availabel:
    + connected
    + batteryCritical

* what means "cached": true in integrationpluginsax.json?
    -> state is cached in a file and available after reboot again

### ToDo 
+ convert type:sunssf

### Used states so far in plugin

+ battery current

+ battery voltage

+ Connected

+ CurrentPower
    * MINUS or PLUS?
+ battery power factor

+ ChargingState

+ BatteryCritical

+ smartmeter frequency
+ smartmeter frequency factor

+ battery stateOfHealth

+ smartmeter totalEnergyProduced
+ smartmeter totalEnergyConsumed
+ smartmeter energyFactor

+ battery state

+ currentPhaseA
+ currentPhaseB
+ currentPhaseC

+ powerPhaseA
+ powerPhaseB
+ powerPhaseC
+ powerFactorSmartmeter

+ voltagePhaseA
+ voltagePhaseB
+ voltagePhaseC

+ Soc

+ Capacity


## Supported Things

* SAX Homespeicher

## Tested connections

* 

## Requirements

* 

## More
* [Sax homepage](https://sax-power.net/produkte/sax-power-home/)
