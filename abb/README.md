# ABB 

Connects nymea to a ABB wallboxes. Currently supported models are:

* terra AC

## Supported Things

## Requirements

## More 

* [ABB terra AC homepage](https://new.abb.com/ev-charging/de/terra-ac-wandladestation)

# ToDo

- translationfile


- fix state types `sessionEnergy` 
```
Nov 30 16:28:40 1u0022-co-testChristian nymead[10017]:  W | Thing: No such state type "{277e7d7f-3e1e-49bc-8005-80e5229ed680}" in Thing(ABB Terra, id: {0c25aa5a-da0b-48d4-9867-0bbc35f449bb}, ThingClassId: {910fddb2-989d-4c6c-8264-cf877c1e53e4})
Nov 30 16:28:40 1u0022-co-testChristian nymead[10017]:  W | Thing: No such state type "{b20e21fd-8131-430c-9557-7d24d65f333a}" in Thing(ABB Terra, id: {0c25aa5a-da0b-48d4-9867-0bbc35f449bb}, ThingClassId: {910fddb2-989d-4c6c-8264-cf877c1e53e4})
```


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
    -> available from nymea 1.8


- Registers maxChargingCurrent and minChargingCurrent not available