# ABB 

Connects nymea to a ABB wallboxes. Currently supported models are:

* terra AC

## Supported Things

## Requirements

## More 

* [ABB terra AC homepage](https://new.abb.com/ev-charging/de/terra-ac-wandladestation)

# ToDo
- translationfile

- check states

- Versioncontrol in *discovery.cpp and *pluginabb.cpp: info-message

- ChargingState in abb-registers.json: Interprete Byte 1 (see 5.6 in ABB_Terra_AC_Charger_ModbusCommunication_v1.7.pdf)
    + check if Spare-Bytes are 0
    + interpret bit 7 = 0: charging at rated current.

- Registers maxChargingCurrent and minChargingCurrent not available