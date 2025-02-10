#!/bin/bash
#Name of the generator
GENERATORNAME=schneiderIEM_generator

#SOURCE Integration you would like to copy
SOURCE_INTEGRATION=schneiderIEM

# Target files, meaning the needed integration files.
TARGET=schneiderIEM_inverter
TARGETJSON=integrationpluginschneiderIEM_inverter.json
TARGETH=integrationpluginschneiderIEM_inverter.h
TARGETCPP=integrationpluginschneiderIEM_inverter.cpp
TARGETPRO=schneiderIEM_inverter.pro

# JSON file variables to replace
DISPLAYNAME="SchneiderIEM Inverter"
INTERFACE1=solarinverter
INTERFACE2=connectable

# Name of the package (debian package variables)
SOURCEPACKAGENAME=nymea-plugin-schneider-iem
PACKAGENAME=nymea-plugin-schneider-iem-inverter
DEVICENAME=schneiderIEM


cp -r ../$SOURCE_INTEGRATION ../$TARGET
cd ../$TARGET
rename "s/${SOURCE_INTEGRATION}/${TARGET}/g" ./*
python3 ../$GENERATORNAME/patch_json.py $TARGETJSON --interfaces $INTERFACE1 $INTERFACE2 --output $TARGETJSON  --display_name "${DISPLAYNAME}" --vendor_uuid "75bc5b91-0983-4db3-a829-04fe04e63531"
uuid=$(cat $TARGETJSON | grep "id" | head -n 1 | sed 's/"id": "//' | sed 's/",//g' | xargs)
mv translations/*-de.ts translations/$uuid-de.ts 
mv translations/*-de_DE.ts translations/$uuid-de_DE.ts 
mv translations/*-en_US.ts translations/$uuid-en_US.ts 

sed "s/${SOURCE_INTEGRATION}\.json/${TARGET}\.json/g" $TARGETH -i
sed "s/${SOURCE_INTEGRATION}\.h/${TARGET}\.h/g" $TARGETCPP -i
sed "s/${SOURCE_INTEGRATION}\./${TARGET}\./g" $TARGETPRO -i
cd ..
sed "s/${SOURCE_INTEGRATION}/${SOURCE_INTEGRATION} \\\ \n    ${TARGET}/g" nymea-plugins-modbus.pro -i

tee -a debian/control <<EOF

Package: $PACKAGENAME
Architecture: any
Section: libs
Depends: \${shlibs:Depends},
         \${misc:Depends},
Description: nymea integration to use $DEVICENAME as solar inverter metering device
 This package contains the nymea integration plugin to use a  $DEVICENAME as a solar inverter metering device.
EOF

cp debian/$SOURCEPACKAGENAME.install.in debian/$PACKAGENAME.install.in
sed "s/${SOURCE_INTEGRATION}/${TARGET}/g" debian/$PACKAGENAME.install.in -i
