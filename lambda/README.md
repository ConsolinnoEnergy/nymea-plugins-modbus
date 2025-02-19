# lambda

Connect nymea to lambda heat pumps. 

In order to use the modbus interface, it is important to enable modbus on the heatpump using the "Connected Heat" web portal from Lambda. 
The instructions for that can be found in the user manual of the heat pump. 

Please also make sure all values are configured as readable and writable for modbus, otherwise the heapump can only be monitored, but without controlling its power consumption.

## Supported Things

* lambda

## Requirements

* The package 'nymea-plugin-lambda' must be installed
* Both devices must be in the same local area network.
* Modbus enabled and all values are readable and writable.

## More

https://lambda-wp.at/
