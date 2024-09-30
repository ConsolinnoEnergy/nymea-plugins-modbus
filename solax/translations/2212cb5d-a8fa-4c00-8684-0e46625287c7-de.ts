<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de">
<context>
    <name>IntegrationPluginSolax</name>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="74"/>
        <source>The network device discovery is not available.</source>
        <translation>Die Network Device Discovery ist nicht verfügbar.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="112"/>
        <source>No Modbus RTU interface available. Please set up a Modbus RTU interface first.</source>
        <translation>Kein Modbus RTU Interface verfügbar. Bitte zuerst ein Modbus RTU Interface konfigurieren.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="119"/>
        <source>No modbus RTU master with appropriate settings found.</source>
        <translation>Kein Modbus RTU Master mit zum Gerät passenden Einstellungen verfügbar. Bitte zuerst einen Modbus RTU Master entsprechend konfigurieren.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="181"/>
        <source>The MAC address is not vaild. Please reconfigure the device to fix this.</source>
        <translation>Die MAC Adresse ist ungültig. Bitte Gerät neu konfigurieren.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="214"/>
        <source>The Modbus address not valid. It must be a value between 1 and 247.</source>
        <translation>Ungültige Modbus ID. Die ID muss einen Wert zwischen 1 und 247 haben.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="221"/>
        <source>The Modbus RTU resource is not available.</source>
        <translation>Der ausgewählte Modbus RTU Master ist nicht verfügbar.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="1458"/>
        <source>No error, everything ok.</source>
        <translation>Keine Fehler, alles in Ordnung.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="1460"/>
        <source>Error reported! Check with Solax.</source>
        <translation>Fehler gemeldet! Kontaktieren Sie Solax.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="1484"/>
        <source>Warning reported! Check with Solax.</source>
        <translation>Warnung gemeldet! Kontaktieren Sie Solax.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsolax.cpp" line="1482"/>
        <source>No warning, everything ok.</source>
        <translation>Keine Warnungen, alles in Ordnung.</translation>
    </message>
</context>
<context>
    <name>Solax</name>
    <message>
        <location filename="../plugininfo.h" line="122"/>
        <location filename="../plugininfo.h" line="125"/>
        <location filename="../plugininfo.h" line="128"/>
        <source>Active power</source>
        <extracomment>The name of the StateType ({ea30fbf2-deef-4e6d-843b-e7123faa2ee8}) of ThingClass solaxMeter
----------
The name of the StateType ({da92b1db-b743-47a2-a6f9-36c975e1305f}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({33aaf5de-8307-455f-930e-3cdd5a8387f2}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Wirkleistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="131"/>
        <source>Active power limit</source>
        <extracomment>The name of the StateType ({e446c3dd-c929-4a09-80a1-c0be3cc50a0e}) of ThingClass solaxX3InverterRTU</extracomment>
        <translation>Wirkleistung Limit</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="134"/>
        <source>Battery capacity</source>
        <extracomment>The name of the StateType ({413f5f64-d3d9-4fa0-981e-d707c1367139}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie Kapazität</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="137"/>
        <location filename="../plugininfo.h" line="140"/>
        <source>Battery capacity (if present) [kWh]</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterRTU, Type: thing, ID: {c64aa05c-ea87-417e-91d9-238cdd55a4a5})
----------
The name of the ParamType (ThingClass: solaxX3InverterTCP, Type: thing, ID: {c2986cf6-65e0-4a80-9f6e-9cdd1fb839f1})</extracomment>
        <translation>Batterie Kapazität (wenn vorhanden) [kWh]</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="143"/>
        <source>Battery charge percent</source>
        <extracomment>The name of the StateType ({0a380479-c472-4645-9d89-68dd73ee5439}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie Ladestand</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="146"/>
        <source>Battery current</source>
        <extracomment>The name of the StateType ({aa5fd804-36d4-4ea4-8d67-eaad70b33bad}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie Strom</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="149"/>
        <location filename="../plugininfo.h" line="152"/>
        <source>Battery force power (+:charge, -:discharge)</source>
        <extracomment>The name of the ParamType (ThingClass: solaxBattery, ActionType: forcePower, ID: {868ba45f-1f1d-4444-bd19-a258db509a23})
----------
The name of the StateType ({868ba45f-1f1d-4444-bd19-a258db509a23}) of ThingClass solaxBattery</extracomment>
        <translation>Lade- (+) bzw. Entladeleistung (-)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="155"/>
        <source>Battery force power enabled</source>
        <extracomment>The name of the ActionType ({868ba45f-1f1d-4444-bd19-a258db509a23}) of ThingClass solaxBattery</extracomment>
        <translation>Forcierte Batterie Leistung aktiviert</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="158"/>
        <location filename="../plugininfo.h" line="161"/>
        <source>Battery force power timeout</source>
        <extracomment>The name of the ParamType (ThingClass: solaxBattery, ActionType: forcePowerTimeout, ID: {9523ae17-1919-4e18-9c50-534dd0ea9ff2})
----------
The name of the StateType ({9523ae17-1919-4e18-9c50-534dd0ea9ff2}) of ThingClass solaxBattery</extracomment>
        <translation>Dauer der forcierten Batterie Leistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="164"/>
        <source>Battery force power timeout enabled</source>
        <extracomment>The name of the ActionType ({9523ae17-1919-4e18-9c50-534dd0ea9ff2}) of ThingClass solaxBattery</extracomment>
        <translation>Auszeit für Batterie Leistung aktiviert</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="167"/>
        <source>Battery low charge</source>
        <extracomment>The name of the StateType ({09f60c18-4fc9-4117-b58c-b0b470eeface}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie niedriger Ladestand</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="170"/>
        <location filename="../plugininfo.h" line="173"/>
        <source>Battery manual mode</source>
        <extracomment>The name of the ParamType (ThingClass: solaxBattery, ActionType: enableForcePower, ID: {8806455a-280a-4063-aa6c-5b1acb59b897})
----------
The name of the StateType ({8806455a-280a-4063-aa6c-5b1acb59b897}) of ThingClass solaxBattery</extracomment>
        <translation>Manueller Modus</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="176"/>
        <location filename="../plugininfo.h" line="179"/>
        <source>Battery manual mode enabled</source>
        <extracomment>The name of the ActionType ({8806455a-280a-4063-aa6c-5b1acb59b897}) of ThingClass solaxBattery
----------
The name of the StateType ({4cd7a11a-3cf1-4254-9a36-fd836b19fba7}) of ThingClass solaxBattery</extracomment>
        <translation>Manueller Modus aktiviert</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="182"/>
        <source>Battery power input/output</source>
        <extracomment>The name of the StateType ({a7da06fc-2cf8-427b-87f3-707f0f7cbc8c}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie Lade-/Entladeleistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="185"/>
        <source>Battery voltage</source>
        <extracomment>The name of the StateType ({80254271-949b-4b32-93de-7b2554942d8f}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie Spannung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="188"/>
        <source>Charging state</source>
        <extracomment>The name of the StateType ({46ac0f13-4619-4a29-a869-f9a257555994}) of ThingClass solaxBattery</extracomment>
        <translation>Batterie lädt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="191"/>
        <location filename="../plugininfo.h" line="194"/>
        <location filename="../plugininfo.h" line="197"/>
        <location filename="../plugininfo.h" line="200"/>
        <source>Connected</source>
        <extracomment>The name of the StateType ({0f4ace08-576b-4b7b-8e3a-ec740a826e0b}) of ThingClass solaxBattery
----------
The name of the StateType ({386c4fdf-3e33-47bf-879d-2acf9411a1a4}) of ThingClass solaxMeter
----------
The name of the StateType ({7145dfcf-711b-4e66-b2d4-61217395677e}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({badb5699-4022-4cbe-b856-7623b088def4}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Verbunden</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="203"/>
        <source>Current power phase A</source>
        <extracomment>The name of the StateType ({4bd36e81-7981-471b-ac9f-90b22014c6f0}) of ThingClass solaxMeter</extracomment>
        <translation>Leistung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="206"/>
        <source>Current power phase B</source>
        <extracomment>The name of the StateType ({8a92966f-775a-4da7-b3a6-292813b1eaa7}) of ThingClass solaxMeter</extracomment>
        <translation>Leistung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="209"/>
        <source>Current power phase C</source>
        <extracomment>The name of the StateType ({4d3daf5c-cd17-4027-bcbe-c548994f1c27}) of ThingClass solaxMeter</extracomment>
        <translation>Leistung Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="212"/>
        <location filename="../plugininfo.h" line="215"/>
        <source>Energy produced today</source>
        <extracomment>The name of the StateType ({15fded99-2304-4779-92b8-bc76dcbb6267}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({448bc272-6ad5-4c53-8646-b52b004c2fe6}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Heute produzierte PV Energie</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="218"/>
        <location filename="../plugininfo.h" line="221"/>
        <source>Error message</source>
        <extracomment>The name of the StateType ({ec84a765-6c18-4ffe-9372-93d20853d300}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({259c7479-8fb3-44ac-84bb-74cfe4a90336}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Fehlermeldung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="224"/>
        <location filename="../plugininfo.h" line="227"/>
        <source>Export power limit</source>
        <extracomment>The name of the StateType ({69560779-c196-4a09-bda4-56ea66f33fb1}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({ff0f093a-5be6-4086-8c82-9b12ec902213}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Export Limit</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="230"/>
        <location filename="../plugininfo.h" line="233"/>
        <source>Firmware version</source>
        <extracomment>The name of the StateType ({eefe145d-04fd-4beb-baaa-a0233cec5f00}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({749d3c0d-2102-4f74-852c-2ae2cb7e8f32}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Firmware Version</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="236"/>
        <source>Frequency phase A</source>
        <extracomment>The name of the StateType ({cce5c14e-f808-4366-a9f2-2ea1395c960c}) of ThingClass solaxMeter</extracomment>
        <translation>Phase A Netzfrequenz</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="239"/>
        <source>Frequency phase B</source>
        <extracomment>The name of the StateType ({26f8984f-9303-43f7-82a6-bc83a24b03b1}) of ThingClass solaxMeter</extracomment>
        <translation>Phase B Netzfrequenz</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="242"/>
        <source>Frequency phase C</source>
        <extracomment>The name of the StateType ({e1074feb-3c73-4376-8a3f-e5cbacd61418}) of ThingClass solaxMeter</extracomment>
        <translation>Phase C Netzfrequenz</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="245"/>
        <location filename="../plugininfo.h" line="248"/>
        <source>Grid Export Limit (percent of inverter nominal power)</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterRTU, ActionType: setExportLimit, ID: {56517915-3bd5-47da-85c2-950dae05cdd7})
----------
The name of the ParamType (ThingClass: solaxX3InverterTCP, ActionType: setExportLimit, ID: {bb8548d9-d318-49f8-92a1-2c947faf8836})</extracomment>
        <translation>Netz-Exportgrenze (% der Nennleistung)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="251"/>
        <source>Grid frequency</source>
        <extracomment>The name of the StateType ({ed3a0202-f348-41e9-9ebd-3edac0ebb259}) of ThingClass solaxMeter</extracomment>
        <translation>Netzfrequenz</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="254"/>
        <source>IP address</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterTCP, Type: thing, ID: {f2089b5c-407b-47d1-a250-cd201292c769})</extracomment>
        <translation>IP Adresse</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="257"/>
        <location filename="../plugininfo.h" line="260"/>
        <source>Inverter current</source>
        <extracomment>The name of the StateType ({1b298d08-722b-4870-acd7-2ec34584c972}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({6c0486b0-45ac-4623-86cb-b556a5a3f263}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Wechselrichter Strom</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="263"/>
        <location filename="../plugininfo.h" line="266"/>
        <source>Inverter status</source>
        <extracomment>The name of the StateType ({e2b992f6-eb95-4d88-adc1-6426f4d071bc}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({0084168f-3ea3-4d79-b275-447d145587aa}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Wechselrichter Status</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="269"/>
        <location filename="../plugininfo.h" line="272"/>
        <source>Inverter voltage</source>
        <extracomment>The name of the StateType ({997034d0-7e32-4fcd-a295-102462891b6b}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({ae43476d-763b-4d75-a8eb-fddb46cd1e14}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Wechselrichter Spannung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="287"/>
        <source>Nominal Power for control</source>
        <extracomment>The name of the StateType ({d10df429-b0ce-4078-b7e6-843a8e798d9f}) of ThingClass solaxBattery</extracomment>
        <translation>Nennleistung für die Steuerung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="290"/>
        <location filename="../plugininfo.h" line="293"/>
        <source>PV1 current</source>
        <extracomment>The name of the StateType ({8096dd57-b70c-4c75-a5db-0603c9f493b9}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({ff1827dc-88db-4765-8a80-9b1d8a192281}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>PV1 Strom</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="296"/>
        <location filename="../plugininfo.h" line="299"/>
        <source>PV1 power</source>
        <extracomment>The name of the StateType ({29c368c0-20a6-4fe5-be11-2c87cac6c7fd}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({21d03306-a98e-4973-835c-2d41d3c64573}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>PV1 Leistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="302"/>
        <location filename="../plugininfo.h" line="305"/>
        <source>PV1 voltage</source>
        <extracomment>The name of the StateType ({cd1abef8-7d64-4690-bf98-20ef6d24683f}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({fa3f1289-5900-4311-9d3a-5cf3249f9145}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>PV1 Spannung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="308"/>
        <location filename="../plugininfo.h" line="311"/>
        <source>PV2 current</source>
        <extracomment>The name of the StateType ({90ee2234-9a8b-4a0f-91e6-61fd8b917c96}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({6e62add8-72de-401b-840b-0e52365ddb1c}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>PV2 Strom</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="314"/>
        <location filename="../plugininfo.h" line="317"/>
        <source>PV2 power</source>
        <extracomment>The name of the StateType ({d636d626-053c-459a-8793-e7ebc7d8d8bf}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({53432bf2-900e-485b-9d72-04a9d2d2c8fd}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>PV2 Leistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="320"/>
        <location filename="../plugininfo.h" line="323"/>
        <source>PV2 voltage</source>
        <extracomment>The name of the StateType ({876661a8-e0a4-43b7-a6f2-75d1aa7fc03b}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({1b857285-b735-43ac-aec5-041f78d58fff}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>PV2 Spannung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="338"/>
        <location filename="../plugininfo.h" line="341"/>
        <source>Rated power</source>
        <extracomment>The name of the StateType ({03cb0b85-f466-4d2d-aafd-92acc8f716f3}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({e2c14c76-7d93-4dee-bb4e-5b6f18b20633}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Leistungsklasse</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="344"/>
        <location filename="../plugininfo.h" line="347"/>
        <source>Set Grid export limit</source>
        <extracomment>The name of the ActionType ({c92ee09c-40ee-4eac-9ce4-7f2399ddb714}) of ThingClass solaxX3InverterRTU
----------
The name of the ActionType ({4142896c-da53-4ba8-b0de-14f0dd24ae5c}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Netz Exportgrenze festlegen</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="362"/>
        <source>Solax X3 Hybrid Inverter (RTU)</source>
        <extracomment>The name of the ThingClass ({4190fc56-3804-43c4-bcd3-c5f86638513e})</extracomment>
        <translation>Solax X3 Hybrid Inverter (RTU)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="365"/>
        <source>Solax X3 Hybrid Inverter (TCP)</source>
        <extracomment>The name of the ThingClass ({b1cb3137-b293-4df2-ae4f-b662c6835653})</extracomment>
        <translation>Solax X3 Hybrid Inverter (TCP)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="368"/>
        <location filename="../plugininfo.h" line="371"/>
        <location filename="../plugininfo.h" line="374"/>
        <source>Temperature</source>
        <extracomment>The name of the StateType ({c58d4489-f963-4add-9b18-b845860178c5}) of ThingClass solaxBattery
----------
The name of the StateType ({072c40e6-48f6-4170-a370-2a5ea2510c89}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({ca61046c-abf2-423d-8a48-5864d3940d57}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Temperatur</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="377"/>
        <source>Timeout countdown</source>
        <extracomment>The name of the StateType ({5cdace8a-a034-4b83-af85-2060d165d056}) of ThingClass solaxBattery</extracomment>
        <translation>Verbleibende Zeit</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="401"/>
        <source>Warning message</source>
        <extracomment>The name of the StateType ({2f16aef5-9459-4205-8e81-d444354b7c2a}) of ThingClass solaxBattery</extracomment>
        <translation>Warnmeldung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="404"/>
        <source>min Battery level set</source>
        <extracomment>The name of the ActionType ({28235635-d544-42b4-8376-c2bda6a00123}) of ThingClass solaxBattery</extracomment>
        <translation>Setze minimalen Batteriestand</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="407"/>
        <location filename="../plugininfo.h" line="410"/>
        <source>min. Battery level</source>
        <extracomment>The name of the ParamType (ThingClass: solaxBattery, ActionType: minBatteryLevel, ID: {28235635-d544-42b4-8376-c2bda6a00123})
----------
The name of the StateType ({28235635-d544-42b4-8376-c2bda6a00123}) of ThingClass solaxBattery</extracomment>
        <translation>Minimale Ladung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="275"/>
        <source>MAC address</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterTCP, Type: thing, ID: {83ffc100-a926-4590-87d4-4e69f3aa7948})</extracomment>
        <translation>MAC Adresse</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="278"/>
        <location filename="../plugininfo.h" line="281"/>
        <source>Modbus ID</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterRTU, Type: thing, ID: {7baf3bf5-d7b5-492a-a9f2-84789e45964f})
----------
The name of the ParamType (ThingClass: solaxX3InverterTCP, Type: thing, ID: {6ee3b4a1-b41e-4594-9002-eef12f8c2a08})</extracomment>
        <translation>Modbus ID</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="284"/>
        <source>Modbus RTU master</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterRTU, Type: thing, ID: {186dc796-e2ef-4034-9693-8561cd44c2ac})</extracomment>
        <translation>Modbus RTU Master</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="326"/>
        <source>Phase A current</source>
        <extracomment>The name of the StateType ({beda0148-a8bb-4979-9f33-06afa4bd402c}) of ThingClass solaxMeter</extracomment>
        <translation>Strom Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="329"/>
        <source>Phase B current</source>
        <extracomment>The name of the StateType ({57c604aa-3d1f-46c2-aabb-c8084311d5da}) of ThingClass solaxMeter</extracomment>
        <translation>Strom Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="332"/>
        <source>Phase C current</source>
        <extracomment>The name of the StateType ({28a8a285-e509-4215-a03b-144a6d3130f7}) of ThingClass solaxMeter</extracomment>
        <translation>Strom Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="335"/>
        <source>Port</source>
        <extracomment>The name of the ParamType (ThingClass: solaxX3InverterTCP, Type: thing, ID: {dbac37a7-f311-4083-b56f-fd0490f1afcc})</extracomment>
        <translation>Port</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="350"/>
        <location filename="../plugininfo.h" line="353"/>
        <source>Solax</source>
        <extracomment>The name of the vendor ({093a76b1-f60d-41de-8d34-0ffe74a62c19})
----------
The name of the plugin Solax ({2212cb5d-a8fa-4c00-8684-0e46625287c7})</extracomment>
        <translation>Solax</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="356"/>
        <source>Solax Battery</source>
        <extracomment>The name of the ThingClass ({fb017a49-0a2e-4ae6-8277-3da0142cbbd2})</extracomment>
        <translation>Solax Batterie</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="359"/>
        <source>Solax Meter</source>
        <extracomment>The name of the ThingClass ({d67da0b2-34af-4c96-bad8-22b770629a28})</extracomment>
        <translation>Solax Wurzelzähler</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="380"/>
        <source>Total energy exported to grid</source>
        <extracomment>The name of the StateType ({51170a92-812b-4932-9b7f-b90b5947114a}) of ThingClass solaxMeter</extracomment>
        <translation>Netzeinspeisung gesamt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="383"/>
        <source>Total energy imported from grid</source>
        <extracomment>The name of the StateType ({2a65f11c-cc5d-4dcf-85a8-4d21ae487ffc}) of ThingClass solaxMeter</extracomment>
        <translation>Netzbezug gesamt</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="386"/>
        <location filename="../plugininfo.h" line="389"/>
        <source>Total energy produced</source>
        <extracomment>The name of the StateType ({d894bfa9-7d1e-4eb8-99a2-410fab5994a9}) of ThingClass solaxX3InverterRTU
----------
The name of the StateType ({077dfd18-4785-4069-8af5-f94a00389ede}) of ThingClass solaxX3InverterTCP</extracomment>
        <translation>Gesamte produzierte PV Energie</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="392"/>
        <source>Voltage phase A</source>
        <extracomment>The name of the StateType ({20a9a0f1-456b-43ad-8d07-def6c43d51d3}) of ThingClass solaxMeter</extracomment>
        <translation>Spannung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="395"/>
        <source>Voltage phase B</source>
        <extracomment>The name of the StateType ({0b5c1bc9-3982-45e2-87ae-cb3f3f6db031}) of ThingClass solaxMeter</extracomment>
        <translation>Spannung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="398"/>
        <source>Voltage phase C</source>
        <extracomment>The name of the StateType ({5ec86b4c-b217-459c-a961-bf00dfbe8e98}) of ThingClass solaxMeter</extracomment>
        <translation>Spannung Phase C</translation>
    </message>
</context>
</TS>
