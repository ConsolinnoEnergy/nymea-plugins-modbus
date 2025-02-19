#!/bin/bash
cp -r ../bgetech ../bgetech_consumer
cd ../bgetech_consumer
rename 's/bgetech/bgetech_consumer/g' ./*
python3 ../sdm630_generator/patch_json.py integrationpluginbgetech_consumer.json --interfaces smartmeterconsumer connectable --output integrationpluginbgetech_consumer.json  --display_name_sdm630 "SDM630 Consumer" --display_name_sdm72 "SDM72 Consumer" --uuid_offset 2 --vendor_uuid "215035fe-95e8-43d8-a52e-0a31b787d902"
uuid=$(cat integrationpluginbgetech_consumer.json | grep "id" | head -n 1 | sed 's/"id": "//' | sed 's/",//g' | xargs)
mv translations/*-de.ts translations/$uuid-de.ts
mv translations/*-de_DE.ts translations/$uuid-de_DE.ts
mv translations/*-en_US.ts translations/$uuid-en_US.ts

sed 's/bgetech\.json/bgetech_consumer\.json/g' integrationpluginbgetech_consumer.h -i
sed 's/bgetech\.h/bgetech_consumer\.h/g' integrationpluginbgetech_consumer.cpp -i
sed 's/bgetech\./bgetech_consumer\./g' bgetech_consumer.pro -i
cd ..
sed 's/bgetech /bgetech      \\\n    bgetech_consumer/' nymea-plugins-modbus.pro -i

tee -a debian/control <<EOF

Package: nymea-plugin-bgetech-consumer
Architecture: any
Section: libs
Depends: \${shlibs:Depends},
         \${misc:Depends},
Description: nymea integration to use SDM630 as a consumer metering device
 This package contains the nymea integration plugin to use a SDM630 meter as a consumer metering device.
EOF

cp debian/nymea-plugin-bgetech.install.in debian/nymea-plugin-bgetech-consumer.install.in
sed 's/bgetech/bgetech_consumer/g' debian/nymea-plugin-bgetech-consumer.install.in -i
