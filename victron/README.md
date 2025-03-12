# Victron energy

Connects to a Victron GX gateway

## Supported things

* Victron GX devices like Cerbo GX, CCGX, Venus GX
* Victron inverter connected via VE.bus like Multis or Quattros 
* Victron Meters (only system registers)
* Connected batteries (only system registers)

## Tested connections
* modpoll cmd
```
./modpoll -m tcp -a100 -t4:hex -r800 -c6 -0 -1 192.168.188.92
```

## Requirements

* For remote battery control the Victron inverter needs to be connected via VE.bus to the Victron GX gateway


## More

https://www.victronenergy.com/

