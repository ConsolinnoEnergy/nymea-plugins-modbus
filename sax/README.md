# sax battery storage
--------------------------------

Connect nymea to sax battery storage over Modbus TCP and get soc and power values.


## Notes
        
* ask Sax Power for version register


* currentPower Smartmert Register oder aus Phasenleistungen summieren?


*  fix `"enums": ...  "name": "StateBattery",`, ausgelesen wird value=3, welcher aktuell nicht definiert wurde
## Supported Things

* SAX Power Home (up to three modules together)
    * 5,2 kWh
    * 10,4 kWh
    * 15,6 kWh

## Tested connections
* modpoll cmd
```
modpoll -0 -1 -a40 -r 40071 -c 50 -t4 -p 502 192.168.179.33
nymea-modbus-cli -a 192.168.179.33  -m 40 -r 40115 -t holding -d
```

## Requirements

* 

## More
* [Sax homepage](https://sax-power.net/produkte/sax-power-home/)
