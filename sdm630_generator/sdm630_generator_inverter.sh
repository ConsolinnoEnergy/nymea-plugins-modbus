#!/bin/bash
cp -r ../bgetech ../bgetech_inverter
cd ../bgetech_inverter
rename 's/bgetech/bgetech_inverter/g' ./*
python3 ../sdm630_generator/patch_json.py integrationpluginbgetech_inverter.json --interfaces solarinverter connectable --output integrationpluginbgetech_inverter.json  --display_name_sdm630 "SDM630 Inverter" --display_name_sdm72 "SDM72 Inverter" --vendor_uuid "215035fe-95e8-43d8-a52e-0a31b787d902"
uuid=$(cat integrationpluginbgetech_inverter.json | grep "id" | head -n 1 | sed 's/"id": "//' | sed 's/",//g' | xargs)
mv translations/*-de.ts translations/$uuid-de.ts 
mv translations/*-de_DE.ts translations/$uuid-de_DE.ts 
mv translations/*-en_US.ts translations/$uuid-en_US.ts 

sed 's/bgetech\.json/bgetech_inverter\.json/g' integrationpluginbgetech_inverter.h -i
sed 's/bgetech\.h/bgetech_inverter\.h/g' integrationpluginbgetech_inverter.cpp -i
sed 's/bgetech\./bgetech_inverter\./g' bgetech_inverter.pro -i
cd ..
sed 's/bgetech /bgetech      \\\n    bgetech_inverter/' nymea-plugins-modbus.pro -i

tee -a debian/control <<EOF 

Package: nymea-plugin-bgetech-inverter
Architecture: any
Section: libs
Depends: \${shlibs:Depends},
         \${misc:Depends},
Description: nymea integration to use SDM630 as solar inverter metering device 
 This package contains the nymea integration plugin to use a SDM630 meter as a solar inverter metering device.
EOF

cp debian/nymea-plugin-bgetech.install.in debian/nymea-plugin-bgetech-inverter.install.in
sed 's/bgetech/bgetech_inverter/g' debian/nymea-plugin-bgetech-inverter.install.in -i
