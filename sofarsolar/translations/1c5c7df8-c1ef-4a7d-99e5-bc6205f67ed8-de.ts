<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE" sourcelanguage="en_US">
<context>
    <name>IntegrationPluginSofarsolar</name>
    <message>
        <location filename="../integrationpluginsofarsolar.cpp" line="75"/>
        <source>No Modbus RTU interface available. Please set up a Modbus RTU interface first.</source>
        <translation>Keine Modbus RTU Verbindung verfügbar. Bitte zuerst eine Modbus RTU Interface Liste erstellen.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsofarsolar.cpp" line="82"/>
        <source>The Modbus slave address must be a value between 1 and 247.</source>
        <translation>Der Wert der Modbus Slave Adresse muss zwischen 1 und 247 liegen.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsofarsolar.cpp" line="116"/>
        <source>The Modbus address not valid. It must be a value between 1 and 247.</source>
        <translation>Die Modbus Adresse ist ungültig. Der Wert muss zwischen 1 und 247 liegen.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsofarsolar.cpp" line="124"/>
        <source>The Modbus RTU resource is not available.</source>
        <translation>Die Modbus RTU Hardware ist nicht verfügbar.</translation>
    </message>
</context>
<context>
    <name>Sofarsolar</name>
    <message>
        <location filename="../plugininfo.h" line="71"/>
        <location filename="../plugininfo.h" line="74"/>
        <source>Active power</source>
        <extracomment>The name of the StateType ({bd54cf90-4580-4d39-b690-8d75e576d8ba}) of ThingClass sofarsolarMeter
----------
The name of the StateType ({0cba449a-dd8d-4a48-bd74-6a6a1b10d164}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Wirkleistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="77"/>
        <source>Battery capacity</source>
        <extracomment>The name of the StateType ({ea8cb430-f65c-4037-9a3d-0d870c4e839e}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batteriekapazität</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="80"/>
        <source>Battery capacity kWh</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarBattery, Type: settings, ID: {611d05c5-fb58-41d6-a44e-a5cf5363e581})</extracomment>
        <translation>Batteriekapazität in kWh</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="83"/>
        <source>Battery charge cycles</source>
        <extracomment>The name of the StateType ({dc108176-ff3b-4d63-84b2-fab5486f72a0}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Ladezyklen der Batterie</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="86"/>
        <source>Battery charge percent</source>
        <extracomment>The name of the StateType ({ebf9ee5a-5f1f-4c12-ad2f-186a79fe2244}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batterie Ladestand in Prozent</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="89"/>
        <source>Battery current</source>
        <extracomment>The name of the StateType ({5afa9c88-f189-4090-ac7a-14d8b2d69a7f}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batteriestrom</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="92"/>
        <source>Battery low charge</source>
        <extracomment>The name of the StateType ({4191a509-9df7-4b56-bbbf-aca97a2fda3b}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batterie niedriger Ladestand</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="95"/>
        <source>Battery power input/output</source>
        <extracomment>The name of the StateType ({ecb10a97-9209-4f5a-ab3f-2669c41d3bcc}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batterie Lade-/Entladeleistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="98"/>
        <source>Battery state of health</source>
        <extracomment>The name of the StateType ({49050854-2b98-4e59-8f86-546a423d1a5f}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batterie Gesundheitszustand (SoH)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="101"/>
        <source>Battery temperature</source>
        <extracomment>The name of the StateType ({b6bd18e3-9db7-4ae1-9c43-851b6856f18a}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batterietemperatur</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="104"/>
        <source>Battery voltage</source>
        <extracomment>The name of the StateType ({9d49ad76-432e-45b0-9437-eabfd7bfe84c}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batteriespannung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="107"/>
        <source>Charging state</source>
        <extracomment>The name of the StateType ({d89f29d2-4925-43e6-bced-2fdfeabb5e70}) of ThingClass sofarsolarBattery</extracomment>
        <translation>Batterie lädt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="110"/>
        <location filename="../plugininfo.h" line="113"/>
        <location filename="../plugininfo.h" line="116"/>
        <source>Connected</source>
        <extracomment>The name of the StateType ({52162418-4293-4bed-825f-2d18552959e2}) of ThingClass sofarsolarBattery
----------
The name of the StateType ({6ef425b6-473a-4bf8-8f0e-2f148a7df6c7}) of ThingClass sofarsolarMeter
----------
The name of the StateType ({d7c679f8-d5d5-49bc-bff2-d32b1252de51}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Verbunden</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="119"/>
        <source>Current power phase A</source>
        <extracomment>The name of the StateType ({6e7c0d72-c12a-43fb-b425-2be4b249ad17}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Leistung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="122"/>
        <source>Current power phase B</source>
        <extracomment>The name of the StateType ({5a7e4b12-58e5-41b2-9e43-24835dc767f9}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Leistung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="125"/>
        <source>Current power phase C</source>
        <extracomment>The name of the StateType ({0e95cc35-5e66-4709-bd2c-b5017731fbb2}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Leistung Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="128"/>
        <location filename="../plugininfo.h" line="131"/>
        <source>Export Limit</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarInverterRTU, ActionType: exportLimit, ID: {9b88cf03-8e3c-4748-84b5-44235efe94c5})
----------
The name of the StateType ({9b88cf03-8e3c-4748-84b5-44235efe94c5}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Exportlimit</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="134"/>
        <location filename="../plugininfo.h" line="137"/>
        <source>Export Limit enabled</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarInverterRTU, ActionType: exportLimitEnable, ID: {a3545770-78ca-4eb7-a678-8f2b8dad76c6})
----------
The name of the StateType ({a3545770-78ca-4eb7-a678-8f2b8dad76c6}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Exportlimit aktiviert</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="140"/>
        <source>Export limit enabled</source>
        <extracomment>The name of the ActionType ({a3545770-78ca-4eb7-a678-8f2b8dad76c6}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Exportlimit aktiviert</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="143"/>
        <source>Frequency</source>
        <extracomment>The name of the StateType ({345deaf9-986f-4dac-a8ef-1343d9bbf7a5}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Netzfrequenz</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="146"/>
        <source>Modbus RTU master</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarInverterRTU, Type: thing, ID: {879031ad-38dc-43b8-805a-191a00f69484})</extracomment>
        <translation>Modbus RTU Master</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="149"/>
        <source>Modbus slave address</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarInverterRTU, Type: thing, ID: {0f6f24d6-f564-407d-bcb0-8ff34e69b088})</extracomment>
        <translation>Modbus ID</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="152"/>
        <source>Nominal power</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarInverterRTU, Type: thing, ID: {ec7a61e9-2490-4305-b4d0-959349c973c0})</extracomment>
        <translation>Nominalleistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="155"/>
        <source>Phase A current</source>
        <extracomment>The name of the StateType ({f2e4604d-b036-49c8-b303-2db5d4d59792}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Strom Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="158"/>
        <source>Phase B current</source>
        <extracomment>The name of the StateType ({65eaff9d-e14c-431f-95c8-1b0c061c637b}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Strom Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="161"/>
        <source>Phase C current</source>
        <extracomment>The name of the StateType ({2be88b21-48a6-4ce1-a784-8137ae44760f}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Strom Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="164"/>
        <location filename="../plugininfo.h" line="167"/>
        <source>SOFAR solar</source>
        <extracomment>The name of the vendor ({3461376f-c6ca-4efb-bd87-b901669c200d})
----------
The name of the plugin Sofarsolar ({1c5c7df8-c1ef-4a7d-99e5-bc6205f67ed8})</extracomment>
        <translation>SOFAR solar</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="170"/>
        <source>SOFAR solar Battery</source>
        <extracomment>The name of the ThingClass ({d6a41d7d-15cb-4b46-ac56-b2def70690e5})</extracomment>
        <translation>SOFAR solar Batterie</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="173"/>
        <source>SOFAR solar HYD Inverter</source>
        <extracomment>The name of the ThingClass ({c3e20738-ca51-4b78-bacf-e129ac39ed97})</extracomment>
        <translation>SOFAR solar HYD Wechselrichter</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="176"/>
        <source>SOFAR solar Meter</source>
        <extracomment>The name of the ThingClass ({3f83fb32-ffc2-415d-9d29-f5a75d38ed9d})</extracomment>
        <translation>SOFAR solar Wurzelzähler</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="179"/>
        <source>Set export limit</source>
        <extracomment>The name of the ActionType ({9b88cf03-8e3c-4748-84b5-44235efe94c5}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Exportlimit</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="182"/>
        <source>Slave address</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarInverterRTU, Type: discovery, ID: {bc732e9e-3e19-4587-ae94-45a6ed1b8724})</extracomment>
        <translation>Modbus ID</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="185"/>
        <source>System Status</source>
        <extracomment>The name of the StateType ({1db29288-f102-4598-908d-4ce02a307811}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>System Status</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="188"/>
        <source>Total energy exported to grid</source>
        <extracomment>The name of the StateType ({27430fe4-ff5b-427a-98a4-f02ac6fc9747}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Netzeinspeisung gesamt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="191"/>
        <source>Total energy imported from grid</source>
        <extracomment>The name of the StateType ({0ac77f5e-e188-4095-b289-b8d84a9d45ed}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Netzbezug gesamt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="194"/>
        <source>Total energy produced</source>
        <extracomment>The name of the StateType ({595a0b47-595d-4a33-ad9b-b1fa59088cea}) of ThingClass sofarsolarInverterRTU</extracomment>
        <translation>Netzbezug gesamt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="197"/>
        <source>Unit</source>
        <extracomment>The name of the ParamType (ThingClass: sofarsolarBattery, Type: thing, ID: {4903fd92-86c6-4105-877e-172c503e42e9})</extracomment>
        <translation>Nummer</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="200"/>
        <source>Voltage phase A</source>
        <extracomment>The name of the StateType ({dc54d944-0aff-4cb8-a312-4fc6144d33f6}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Spannung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="203"/>
        <source>Voltage phase B</source>
        <extracomment>The name of the StateType ({4add9dac-f093-4a4d-8c5a-2da9ffcdcf3a}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Spannung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="206"/>
        <source>Voltage phase C</source>
        <extracomment>The name of the StateType ({e54745e2-2348-4eeb-8ac6-a2a70aa1d33e}) of ThingClass sofarsolarMeter</extracomment>
        <translation>Spannung Phase C</translation>
    </message>
</context>
</TS>
