# Plugin als Template

Dieser generator ermöglicht es ein Plugin als Template zu verwenden, um damit weitere Integrationen zu erstellen.  
Um den Generator zu starten, im Projekt Ordner folgenden Befehl ausführen:

```bash
./generator/generate_plugins.sh
```

## TODOS

- Bei CIGate Implementieren und testen
- Für modbus-nymea-plugins-testen

## Vorgehensweise

In einer Liste werden alle Plugins (Ordner) geben, die als Template für weitere Integrationen dienen sollen. Dies ist eine einfache Liste mit einem Template-Plugin pro Zeile.  
Diese Liste wird über **generate_plugins.sh** ausgelesen. Das Skript geht durch alle Ordner durch und liest die darin enthaltene **generator_list.json** Datei aus, in der die zu generierenden Integrationen angegeben sind.
Dabei ist es wichtig, dass die Namen der Plugin-Dateien im Format _'integrationplugin\<name>.{cpp,h}'_ abgelegt sind.  
Bspw: 
- Ordner: ocpp
- Dateien: integrationplugin**ocpp**.cpp, integrationplugin**ocpp**.h, integrationplugin**ocpp**.json

In dem **generate_plugins.sh** Skript wird dann das **generic_generator.sh** Skript aufgerufen.  
Dieses Skript kopiert die Dateien und ersetzt die Namen. Ebenfalls ruft es das **path_json.py** Skript auf, in dem die JSON Datei der neuen Plugins angepasst wird.

### generator_list.json

Die zu generierenden Integrationen werden im JSON Format angegeben.
Die oberste Ebene gibt dabei den Hersteller (Vendor) an, welcher in der App angezeigt wird.  
In der nächsten Ebene sind alle Integrationen angegeben, welche in generiert und in der App angezeigt werden.  
Diese müssen die Keys _"filename"_ und _"uuid\_offset"_ enthalten. Der _uuid\_offset_ darf **nicht** verändert werden.  
Sollen die Interfaces nicht verändert werden, so kann der _"interfaces"_-Key weggelassen oder mit "null" angegeben werden. Andernfalls sind die Interfaces anzugeben.

Beispielsweise:

```json
{
  "FoxESS": {
    "Wallbox 2nd Gen": {
      "filename": "foxess",
      "interfaces": "null",
      "uuid_offset": 1,
      "optional": {"connectors": 1}
    }
  },
  "ABL": {
    "eMH3": {
      "filename": "ablemh3",
      "uuid_offset": 3,
      "optional": {"connectors": 1}
    },
    "eMH3 Twin": {
      "filename": "ablemh3twin",
      "interfaces": "evcharger gateway",
      "uuid_offset": 5,
      "optional": {"connectors": 2}
    },
    "eMH4": {
      "filename": "ablemh4",
      "interfaces": "evcharger gateway",
      "uuid_offset": 7,
      "optional": {"connectors": 1}
    }
  }
}
```

Resultat: 4 Integrationen, 1 für FoxESS, 3 für ABL  

Sind in einem Plugin mehrere Integrationen enthalten, wie bspw. bei bgetech (SDM630 und SDM72), so sieht die JSON Datei folgend aus:

```json
{
  "Hersteller": {
    "SDM630 Inverter, SDM72 Inverter": {
      "filename": "sdm-inverter",
      "interfaces": "solarinverter connectable",
      "uuid_offset": 1
    }
  }
}
```

Resultat: 1 Plugin, das zwei Integrationen enthält. Selber Hersteller, mit zwei Integrationen "SDM630 Inverter" und "SDM72 Inverter"

## Dinge die zu beachten sind

Die UUID müssen konstant sein und sich zwischen builds nicht verändern.  
Die Übersetzungen sind nicht leicht abzuändern. -> Meldungen in der App sind allgemein zu halten.


## Dateien die geändert werden müssen

- Projekt Datei (bspw. OcppCs.pro):  
Datei umbenennen  
In SOURCES und HEADERS die integrationplugin*.{cpp,h} Zeilen umändern
In den großen Plugin Repos das Plugin zu der Liste aller Plugins hinzufügen

- meta.json: Den Wert für "title" umändern

- Translation files umbennen

- Debian Ordner:
  - Changelog Datei? Nur bei Integrationen, die in einem eigenen Repo sind.
  - Control: Beschreibung ist zu ändern.
  - *.install.in Datei kopieren und Inhalt umändern

- integrationplugin\<name>.h: Name der inkludierten Json Datei ist zu ändern.
- integrationplugin\<name>.cpp: Name der inkludierten .h Datei
