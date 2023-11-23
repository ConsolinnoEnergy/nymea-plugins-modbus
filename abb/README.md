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
```
    "enums": [
        {
            "name": "ChargingState",
            "values": [
                {
                    "key": "undefined",
                    "value": 11
                },
                {
                    "key": "A",
                    "value": 32768 #0x8000
                },
                {
                    "key": "B1",
                    "value": 33024 #0x8100
                },
                {
                    "key": "B2",
                    "value": 33280 #0x8200
                },
                {
                    "key": "C1",
                    "value": 33536 #0x8300
                },
                {
                    "key": "C2",
                    "value": 33792 #0x8400
                },
                {
                    "key": "others",
                    "value": 34048 #0x8500
                }
            ]
        }
    ],
```
- chargingCurrent in abb-registers.json: is actually "access": "WO" (was not compilable)
    + is read during init, might cause problems



- Registers maxChargingCurrent and minChargingCurrent not available