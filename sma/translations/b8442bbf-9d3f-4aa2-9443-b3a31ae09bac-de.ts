<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="de_DE" sourcelanguage="en_US">
<context>
    <name>IntegrationPluginSma</name>
    <message>
        <location filename="../integrationpluginsma.cpp" line="57"/>
        <location filename="../integrationpluginsma.cpp" line="151"/>
        <source>Unable to discover devices in your network.</source>
        <translation>Gerät konnte im Netzwerk nicht gefunden werden.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="101"/>
        <location filename="../integrationpluginsma.cpp" line="158"/>
        <source>Unable to discover the network.</source>
        <translation>Netzwerksuche war nicht möglich.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="208"/>
        <source>The network device discovery is not available.</source>
        <translation>Die Geräteerkennung ist nicht verfügbar.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="217"/>
        <location filename="../integrationpluginsma.cpp" line="253"/>
        <source>Serial: </source>
        <translation>Seriennummer: </translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="244"/>
        <source>Unable to scan the network. Please ensure that the system is installed correctly.</source>
        <translation>Netzwerksuche war nicht möglihch. Bitte stellen Sie sicher, dass das System korrekt installiert wurde.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="279"/>
        <source>Please enter the password of your inverter. If no password has been explicitly set, leave it empty to use the default password for SMA inverters.</source>
        <translation>Bitte geben sie Ihr Wechselrichter Passwort ein. Wenn kein Passwort explizit vergeben wurde, geben Sie keines ein. Dann wird dass Default Passwort verwendet.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="289"/>
        <source>The password can not be longer than 12 characters.</source>
        <translation>Das Password darf nicht länger als 12 Zeichen sein.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="343"/>
        <source>Unable to communicate with the meter.</source>
        <translation>Keine Kommunikation mit dem Smart Meter.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="421"/>
        <source>Failed to log in with the given password. Please try again.</source>
        <translation>Login mit dem hinterlegten Passwort fehlgeschlagen. Bitte versuchen Sie es nochmal.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="542"/>
        <location filename="../integrationpluginsma.cpp" line="594"/>
        <source>The MAC address is not known. Please reconfigure the thing.</source>
        <translation>Die MAC Adresse ist unbekannt. Bitte konfigurieren Sie das Gerät erneut.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="554"/>
        <location filename="../integrationpluginsma.cpp" line="606"/>
        <source>The host address is not known yet. Trying again later.</source>
        <translation>Die Hostadresse ist unbekannt. Bitte versuchen Sie es erneut.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="859"/>
        <source>Could not initialize the communication with the inverter.</source>
        <translation>Die Kommunikation mit dem Wechselrichter konnte nicht initialisiert werden.</translation>
    </message>
    <message>
        <location filename="../integrationpluginsma.cpp" line="990"/>
        <source>Could not initialize the communication with the battery inverter.</source>
        <translation>Die Kommunikation mit dem Batteriewechselrichter konnte nicht initialisiert werden.</translation>
    </message>
</context>
<context>
    <name>sma</name>
    <message>
        <location filename="../plugininfo.h" line="120"/>
        <source>Available energy</source>
        <extracomment>The name of the StateType ({38a413cd-3d09-482d-8d25-b602db3b6540}) of ThingClass speedwireBattery</extracomment>
        <translation>Batteriekapazität</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="123"/>
        <source>Battery capacity</source>
        <extracomment>The name of the ParamType (ThingClass: speedwireBattery, Type: settings, ID: {f4f37bce-c60b-40a2-adac-230f48a7db8f})</extracomment>
        <translation>Batteriekapazität</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="126"/>
        <location filename="../plugininfo.h" line="129"/>
        <source>Battery critical</source>
        <extracomment>The name of the StateType ({56f18b28-ed88-4c1a-a297-a5cad109b055}) of ThingClass modbusBatteryInverter
----------
The name of the StateType ({d815aedf-e836-4274-9b51-2f0128420c46}) of ThingClass speedwireBattery</extracomment>
        <translation>Batterie kritisch</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="132"/>
        <location filename="../plugininfo.h" line="135"/>
        <source>Battery level</source>
        <extracomment>The name of the StateType ({fe5ca68e-ddc2-45e7-aac2-b0e67ac40f87}) of ThingClass modbusBatteryInverter
----------
The name of the StateType ({ec534954-8ee4-46f4-94b6-b48b375b1d7d}) of ThingClass speedwireBattery</extracomment>
        <translation>Batterieladestand (SoC)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="138"/>
        <source>Capacity</source>
        <extracomment>The name of the StateType ({13cdb994-dd9e-49ac-a347-d2ab9aef5b45}) of ThingClass modbusBatteryInverter</extracomment>
        <translation>Batteriekapazität</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="141"/>
        <location filename="../plugininfo.h" line="144"/>
        <source>Charging state</source>
        <extracomment>The name of the StateType ({a313b416-5ded-43c9-b1a1-a9af50492d0b}) of ThingClass modbusBatteryInverter
----------
The name of the StateType ({93310fa3-8237-423b-9062-62e0626e8c70}) of ThingClass speedwireBattery</extracomment>
        <translation>Ladezustand</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="147"/>
        <location filename="../plugininfo.h" line="150"/>
        <location filename="../plugininfo.h" line="153"/>
        <location filename="../plugininfo.h" line="156"/>
        <location filename="../plugininfo.h" line="159"/>
        <location filename="../plugininfo.h" line="162"/>
        <source>Connected</source>
        <extracomment>The name of the StateType ({9c4999a1-304d-4724-99cd-eb0cd27590ef}) of ThingClass modbusBatteryInverter
----------
The name of the StateType ({3c60e2a7-31f3-4b0b-a3f9-ede042e82f22}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({7f242169-c01a-4c9a-ac71-4f9fa5409875}) of ThingClass speedwireBattery
----------
The name of the StateType ({aaff72c3-c70a-4a2f-bed1-89f38cebe442}) of ThingClass speedwireInverter
----------
The name of the StateType ({35733d27-4fe0-439a-be71-7c1597481659}) of ThingClass speedwireMeter
----------
The name of the StateType ({c05e6a1a-252c-4f2b-8b31-09cf113d01c1}) of ThingClass sunnyWebBox</extracomment>
        <translation>Verbunden</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="165"/>
        <source>Current</source>
        <extracomment>The name of the StateType ({541c110d-2f56-44bb-8f7e-de55759b942d}) of ThingClass speedwireBattery</extracomment>
        <translation>Strom</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="168"/>
        <location filename="../plugininfo.h" line="171"/>
        <location filename="../plugininfo.h" line="174"/>
        <source>Current phase A</source>
        <extracomment>The name of the StateType ({48d4a7b7-b09a-4255-83dd-9eab8ea3a51c}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({2a6c59ca-853a-47d6-96fb-0c85edf32f52}) of ThingClass speedwireInverter
----------
The name of the StateType ({45bbdbef-1832-4870-bff5-299e580fb4da}) of ThingClass speedwireMeter</extracomment>
        <translation>Strom Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="177"/>
        <location filename="../plugininfo.h" line="180"/>
        <location filename="../plugininfo.h" line="183"/>
        <source>Current phase B</source>
        <extracomment>The name of the StateType ({479b27c4-01fc-45ef-a462-b8d8499b3422}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({4db96fec-737c-4c4b-bf07-5ef2fd62508a}) of ThingClass speedwireInverter
----------
The name of the StateType ({b3a4fdd2-b6b8-4c58-9da3-2084ad414022}) of ThingClass speedwireMeter</extracomment>
        <translation>Strom Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="186"/>
        <location filename="../plugininfo.h" line="189"/>
        <location filename="../plugininfo.h" line="192"/>
        <source>Current phase C</source>
        <extracomment>The name of the StateType ({f82bbba1-c68a-4c43-a3e5-10b00ed924d7}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({0f23fb0e-a440-4ac2-9aff-896bc65feb2c}) of ThingClass speedwireInverter
----------
The name of the StateType ({b3655188-3854-4336-ae3c-61d3bda6fc4d}) of ThingClass speedwireMeter</extracomment>
        <translation>Strom Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="195"/>
        <location filename="../plugininfo.h" line="198"/>
        <location filename="../plugininfo.h" line="201"/>
        <location filename="../plugininfo.h" line="204"/>
        <location filename="../plugininfo.h" line="207"/>
        <location filename="../plugininfo.h" line="210"/>
        <source>Current power</source>
        <extracomment>The name of the StateType ({e1a91af1-8d1a-4564-9ade-b5488d63b90d}) of ThingClass modbusBatteryInverter
----------
The name of the StateType ({225beb67-95ca-495c-aca8-cd3fd4efedd5}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({f0f69109-83a4-4b2a-9e16-66aa33c2e169}) of ThingClass speedwireBattery
----------
The name of the StateType ({d7ceb482-5df8-4c0c-82bd-62ce7ba22c43}) of ThingClass speedwireInverter
----------
The name of the StateType ({d4ac7f37-e30a-44e4-93cb-ad16df18b8f1}) of ThingClass speedwireMeter
----------
The name of the StateType ({ff4ff872-2f0f-4ca4-9fe2-220eeaf16cc2}) of ThingClass sunnyWebBox</extracomment>
        <translation>aktuelle Leistung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="213"/>
        <location filename="../plugininfo.h" line="216"/>
        <source>Current power phase A</source>
        <extracomment>The name of the StateType ({9283d5a9-b185-4678-beb1-1c6ce6f76930}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({c5d09c63-7461-4fb8-a6fe-bc7aa919be30}) of ThingClass speedwireMeter</extracomment>
        <translation>aktuelle Leistung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="219"/>
        <location filename="../plugininfo.h" line="222"/>
        <source>Current power phase B</source>
        <extracomment>The name of the StateType ({8a87319c-f6ab-4eb1-bb17-a65f80289a56}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({c52d4422-b521-4804-a7a7-c4398e91e760}) of ThingClass speedwireMeter</extracomment>
        <translation>aktuelle Leistung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="225"/>
        <location filename="../plugininfo.h" line="228"/>
        <source>Current power phase C</source>
        <extracomment>The name of the StateType ({1f930456-5947-476c-b74b-480f1e81a799}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({555e892c-3ca7-4100-9832-6ac13b87eb04}) of ThingClass speedwireMeter</extracomment>
        <translation>aktuelle Leistung Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="231"/>
        <source>DC power MPP1</source>
        <extracomment>The name of the StateType ({b366f680-6134-488b-8362-b1b824a8daca}) of ThingClass speedwireInverter</extracomment>
        <translation>DC Leistung MPP1</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="234"/>
        <source>DC power MPP2</source>
        <extracomment>The name of the StateType ({87d9b654-5558-47a3-9db9-ffd7c23b4774}) of ThingClass speedwireInverter</extracomment>
        <translation>DC Leistung MPP2</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="237"/>
        <source>Day energy produced</source>
        <extracomment>The name of the StateType ({16f34c5c-8dbb-4dcc-9faa-4b782d57226c}) of ThingClass sunnyWebBox</extracomment>
        <translation>heutige Energieerzeugung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="240"/>
        <source>Energy consumed phase A</source>
        <extracomment>The name of the StateType ({b4ff2c71-f81d-4904-bbac-0c0c6e8a5a33}) of ThingClass speedwireMeter</extracomment>
        <translation>Energieverbrauch Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="243"/>
        <source>Energy consumed phase B</source>
        <extracomment>The name of the StateType ({c4e5f569-ac5d-4761-a898-888880bfd59f}) of ThingClass speedwireMeter</extracomment>
        <translation>Energieverbrauch Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="246"/>
        <source>Energy consumed phase C</source>
        <extracomment>The name of the StateType ({aabc02d7-8dc3-4637-8bf2-dc2e0e737ad3}) of ThingClass speedwireMeter</extracomment>
        <translation>Energieverbrauch Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="249"/>
        <source>Energy produced phase A</source>
        <extracomment>The name of the StateType ({754c3b67-768a-47f7-99d8-f66c198f0835}) of ThingClass speedwireMeter</extracomment>
        <translation>Energieerzeugung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="252"/>
        <source>Energy produced phase B</source>
        <extracomment>The name of the StateType ({7eb08c45-24cf-40ce-be28-f3564f087672}) of ThingClass speedwireMeter</extracomment>
        <translation>Energieerzeugung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="255"/>
        <source>Energy produced phase C</source>
        <extracomment>The name of the StateType ({1eb2bf01-5ec6-42e5-b348-ac1e95199d14}) of ThingClass speedwireMeter</extracomment>
        <translation>Energieerzeugung Phase C</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="258"/>
        <location filename="../plugininfo.h" line="261"/>
        <source>Energy produced today</source>
        <extracomment>The name of the StateType ({b8fb66fa-46b5-4ed7-82a7-29fe5257caa9}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({e8bc8f81-e5c5-4900-b429-93fcaa262fcb}) of ThingClass speedwireInverter</extracomment>
        <translation>Energieerzeugung heute</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="264"/>
        <source>Error</source>
        <extracomment>The name of the StateType ({4e64f9ca-7e5a-4897-8035-6f2ae88fde89}) of ThingClass sunnyWebBox</extracomment>
        <translation>Fehler</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="267"/>
        <location filename="../plugininfo.h" line="270"/>
        <location filename="../plugininfo.h" line="273"/>
        <location filename="../plugininfo.h" line="276"/>
        <source>Firmware version</source>
        <extracomment>The name of the StateType ({952b3d30-1c09-4b0e-b303-56c89d3fa108}) of ThingClass modbusBatteryInverter
----------
The name of the StateType ({3f290cbc-0578-479a-ab98-d89b5549184d}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({6d76cc7b-9e00-4561-be7b-4e2a6b8f7b66}) of ThingClass speedwireInverter
----------
The name of the StateType ({a685393c-8b7e-42c5-bb41-f9907c074626}) of ThingClass speedwireMeter</extracomment>
        <translation>Firmwareversion</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="279"/>
        <source>Frequency</source>
        <extracomment>The name of the StateType ({fdccf5de-7413-4480-9ca0-1151665dede8}) of ThingClass speedwireInverter</extracomment>
        <translation>Frequenz</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="282"/>
        <location filename="../plugininfo.h" line="285"/>
        <source>Host address</source>
        <extracomment>The name of the ParamType (ThingClass: speedwireInverter, Type: thing, ID: {c8098d53-69eb-4d0b-9f07-e43c4a0ea9a9})
----------
The name of the ParamType (ThingClass: sunnyWebBox, Type: thing, ID: {864d4162-e3ce-48b8-b8ac-c1b971b52d42})</extracomment>
        <translation>Hostadresse</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="288"/>
        <location filename="../plugininfo.h" line="291"/>
        <location filename="../plugininfo.h" line="294"/>
        <location filename="../plugininfo.h" line="297"/>
        <source>MAC address</source>
        <extracomment>The name of the ParamType (ThingClass: modbusBatteryInverter, Type: thing, ID: {03a5a009-0edc-4370-924a-785e7fcee30a})
----------
The name of the ParamType (ThingClass: modbusSolarInverter, Type: thing, ID: {3cea46a0-9535-4612-9971-19167109e63c})
----------
The name of the ParamType (ThingClass: speedwireInverter, Type: thing, ID: {7df0ab60-0f11-4495-8e0d-508ba2b6d858})
----------
The name of the ParamType (ThingClass: sunnyWebBox, Type: thing, ID: {03f32361-4e13-4597-a346-af8d16a986b3})</extracomment>
        <translation>MAC Adresse</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="300"/>
        <source>Mode</source>
        <extracomment>The name of the StateType ({1974550b-6059-4b0e-83f4-70177e20dac3}) of ThingClass sunnyWebBox</extracomment>
        <translation>Modus</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="303"/>
        <location filename="../plugininfo.h" line="306"/>
        <source>Model ID</source>
        <extracomment>The name of the ParamType (ThingClass: speedwireInverter, Type: thing, ID: {d9892f74-5b93-4c98-8da2-72aca033273a})
----------
The name of the ParamType (ThingClass: speedwireMeter, Type: thing, ID: {abdc114d-1fac-4454-8b82-871ed5cdf28c})</extracomment>
        <translation>Model ID</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="309"/>
        <location filename="../plugininfo.h" line="312"/>
        <source>Port</source>
        <extracomment>The name of the ParamType (ThingClass: modbusBatteryInverter, Type: thing, ID: {089d29e3-8ce0-42ca-93cf-463ad5a486af})
----------
The name of the ParamType (ThingClass: modbusSolarInverter, Type: thing, ID: {18ded0c1-308e-4a13-a12c-cf9a8ed5a26c})</extracomment>
        <translation>Port</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="315"/>
        <source>SMA</source>
        <extracomment>The name of the plugin sma ({b8442bbf-9d3f-4aa2-9443-b3a31ae09bac})</extracomment>
        <translation>SMA</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="318"/>
        <source>SMA Battery (Speedwire)</source>
        <extracomment>The name of the ThingClass ({b459dad2-f78b-4a87-a7f3-22f3147b83d8})</extracomment>
        <translation>SMA Batterie (Speedwire)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="321"/>
        <source>SMA Battery Inverter (Modbus)</source>
        <extracomment>The name of the ThingClass ({06bed8fd-cadb-4cef-8440-7806fb0165e6})</extracomment>
        <translation>SMA Batterie (Modbus)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="324"/>
        <source>SMA Energy Meter (Speedwire)</source>
        <extracomment>The name of the ThingClass ({0c5097af-e136-4430-9fb4-0ccbb30c3e1c})</extracomment>
        <translation>SMA Energy Meter (Speedwire)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="327"/>
        <source>SMA Inverter (Speedwire)</source>
        <extracomment>The name of the ThingClass ({b63a0669-f2ac-4769-abea-e14cafb2309a})</extracomment>
        <translation>SMA Wechselrichter (Speedwire)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="330"/>
        <source>SMA Solar Inverter (Modbus)</source>
        <extracomment>The name of the ThingClass ({12e0429e-e8ce-48bd-a11c-faaf0bd71856})</extracomment>
        <translation>SMA Solarwechselrichter (Modbus)</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="351"/>
        <location filename="../plugininfo.h" line="354"/>
        <source>Slave ID</source>
        <extracomment>The name of the ParamType (ThingClass: modbusBatteryInverter, Type: thing, ID: {081814d7-26bb-445e-bccd-7f33c0d933ea})
----------
The name of the ParamType (ThingClass: modbusSolarInverter, Type: thing, ID: {6322db2a-0554-4f83-9509-39870ad89027})</extracomment>
        <translation>Slave ID</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="357"/>
        <source>Temperature</source>
        <extracomment>The name of the StateType ({6a146a40-84da-4392-8466-4176b21280d2}) of ThingClass speedwireBattery</extracomment>
        <translation>Temperatur</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="333"/>
        <source>SMA Solar Technology AG</source>
        <extracomment>The name of the vendor ({16d5a4a3-36d5-46c0-b7dd-df166ddf5981})</extracomment>
        <translation>SMA Solar Technology AG</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="336"/>
        <source>SMA Sunny WebBox</source>
        <extracomment>The name of the ThingClass ({49304127-ce9b-45dd-8511-05030a4ac003})</extracomment>
        <translation>SMA Sunny WebBox</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="339"/>
        <location filename="../plugininfo.h" line="342"/>
        <location filename="../plugininfo.h" line="345"/>
        <location filename="../plugininfo.h" line="348"/>
        <source>Serial number</source>
        <extracomment>The name of the ParamType (ThingClass: modbusBatteryInverter, Type: thing, ID: {9e2a69a0-c62c-4c53-b9f4-a2f9cb54f02c})
----------
The name of the ParamType (ThingClass: modbusSolarInverter, Type: thing, ID: {563f2b12-b784-4a2c-856f-57a2b5ce2e9d})
----------
The name of the ParamType (ThingClass: speedwireInverter, Type: thing, ID: {e42242b4-2811-47f9-b42b-b150ed233217})
----------
The name of the ParamType (ThingClass: speedwireMeter, Type: thing, ID: {7c81a0c5-9bc6-43bb-a01a-4de5fe656bba})</extracomment>
        <translation>Seriennummer</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="360"/>
        <source>Total energy consumed</source>
        <extracomment>The name of the StateType ({4fb0a4c1-18ed-4d02-b6d0-c07e9b96a56d}) of ThingClass speedwireMeter</extracomment>
        <translation>gesamter Energieverbrauch</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="363"/>
        <location filename="../plugininfo.h" line="366"/>
        <location filename="../plugininfo.h" line="369"/>
        <location filename="../plugininfo.h" line="372"/>
        <source>Total energy produced</source>
        <extracomment>The name of the StateType ({5e0ed108-7e93-4724-a831-319109d9daf8}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({51cadd66-2cf1-485a-a2a9-191d11abfbd1}) of ThingClass speedwireInverter
----------
The name of the StateType ({76ca68d8-6781-4d2a-8663-440aec40b4de}) of ThingClass speedwireMeter
----------
The name of the StateType ({0bb4e227-7e38-49ca-9b32-ce4621c9305b}) of ThingClass sunnyWebBox</extracomment>
        <translation>gesamte Energieerzeugung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="375"/>
        <source>Voltage</source>
        <extracomment>The name of the StateType ({d2144cad-e507-433b-a9d3-2ab9cf0c1014}) of ThingClass speedwireBattery</extracomment>
        <translation>Spannung</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="378"/>
        <location filename="../plugininfo.h" line="381"/>
        <location filename="../plugininfo.h" line="384"/>
        <source>Voltage phase A</source>
        <extracomment>The name of the StateType ({5a717aff-6bdb-4679-94d6-ec1bce7fa2af}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({6ef4eb16-a3d6-4bc9-972d-5e7cb81173a5}) of ThingClass speedwireInverter
----------
The name of the StateType ({44ee2491-8376-41cd-a21d-185c736152ec}) of ThingClass speedwireMeter</extracomment>
        <translation>Spannung Phase A</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="387"/>
        <location filename="../plugininfo.h" line="390"/>
        <location filename="../plugininfo.h" line="393"/>
        <source>Voltage phase B</source>
        <extracomment>The name of the StateType ({34eb5b54-7683-42ff-8320-9b2527d6381c}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({d9a5768b-1bf5-4933-810d-84dd7a688f71}) of ThingClass speedwireInverter
----------
The name of the StateType ({56ae3555-f874-4c2d-8833-17573dce477a}) of ThingClass speedwireMeter</extracomment>
        <translation>Spannung Phase B</translation>
    </message>
    <message>
        <location filename="../plugininfo.h" line="396"/>
        <location filename="../plugininfo.h" line="399"/>
        <location filename="../plugininfo.h" line="402"/>
        <source>Voltage phase C</source>
        <extracomment>The name of the StateType ({2bd0a069-9d16-4d58-9f78-df682d92005d}) of ThingClass modbusSolarInverter
----------
The name of the StateType ({fc168dc6-eecf-40b4-b214-3e28da0dbb12}) of ThingClass speedwireInverter
----------
The name of the StateType ({51cbb29b-29f0-480a-9d7d-b8f4e6a205ae}) of ThingClass speedwireMeter</extracomment>
        <translation>Spannung Phase B</translation>
    </message>
</context>
</TS>
