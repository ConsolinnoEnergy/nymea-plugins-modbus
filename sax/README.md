# sax battery storage
--------------------------------

Connect nymea to sax battery storage over Modbus TCP and get soc and power values.


## Notes

* refreh values:
    + done via of m_pluginTimer?
    + refreshTime as parameter necessary?
        -> nope
        

* where do i put macros?
    + refreshTime

* connect(kostalConnection, &KostalModbusTcpConnection::updateFinished, thing, [=](){
    V.S.



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
