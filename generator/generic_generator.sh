#!/bin/bash

# Project file of the entire project (e.g. nymea-plugins-modbus.pro)
# PROJECT_FILE='ocpp-cs.pro'
PROJECT_FILE=$(ls | grep '[.]pro$')

# Parse command line arguments given to the script
# Example:
# ./generator/generic_generator.sh --source ocpp --vendor FoxESS --integration 'Wallbox 2nd Gen' --plugin_name foxess --interfaces 'evcharger gateway' --uuid_offset 1
POSITIONAL_ARGS=()
while [[ $# -gt 0 ]]; do
  case $1 in
    --source)
      SOURCE="$2"
      shift # past argument
      shift # past value
      ;;
    --vendor)
      VENDOR="$2"
      shift # past argument
      shift # past value
      ;;
    --integration)
      INTEGRATION="$2"
      shift # past argument
      shift # past value
      ;;
    --plugin_name)
      PLUGIN_NAME="$2"
      shift # past argument
      shift # past value
      ;;
    --interfaces)
      INTERFACES="$2"
      shift # past argument
      shift # past value
      ;;
    --uuid_offset)
      UUID_OFFSET="$2"
      shift # past argument
      shift # past value
      ;;
    --optional)
      OPTIONAL_ARGUMENT="$2"
      shift # past argument
      shift # past value
      ;;
    -*|--*)
      echo "Unknown option $1"
      exit 1
      ;;
    *)
      POSITIONAL_ARGS+=("$1") # save positional arg
      shift # past argument
      ;;
  esac
done

# restore positional parameters
# set -- "${POSITIONAL_ARGS[@]}"

# copy the template plugin folder and rename the all contained files
cp -r ./${SOURCE} ./${PLUGIN_NAME}
cd ./${PLUGIN_NAME}
rename 's/'${SOURCE}'/'${PLUGIN_NAME}'/g' ./*


# patch the json file
python3 ../generator/patch_json.py --file integrationplugin${PLUGIN_NAME}.json --display_name "${INTEGRATION}" --vendor "${VENDOR}" --interfaces ${INTERFACES} --uuid_offset ${UUID_OFFSET}

# patch integration code (cpp,h)
sed 's/'${SOURCE}'\.json/'${PLUGIN_NAME}'\.json/g' *.h -i
sed 's/'${SOURCE^^}'_H/'${PLUGIN_NAME^^}'_H/g' *.h -i
sed 's/'${SOURCE}'\.h/'${PLUGIN_NAME}'\.h/g' *.cpp -i

# path plugin project file
sed 's/'${SOURCE}'\./'${PLUGIN_NAME}'\./g' ${PLUGIN_NAME}.pro -i

# rename the translation files
uuid=$(cat integrationplugin${PLUGIN_NAME}.json | grep "id" | head -n 1 | sed 's/"id": "//' | sed 's/",//g' | xargs)
mv translations/*-de.ts translations/$uuid-de.ts >/dev/null 2>&1
mv translations/*-de_DE.ts translations/$uuid-de.ts >/dev/null 2>&1
mv translations/*-en_US.ts translations/$uuid-en_US.ts >/dev/null 2>&1


cd ..
# add new plugin to project file
sed 's/'${SOURCE}'/'${SOURCE}'    \\\n    '${PLUGIN_NAME}'/g' ${PROJECT_FILE} -i


# Prepare the debian package
# Insert information about the new integrations
tee -a debian/control <<EOF >/dev/null 2>&1

Package: nymea-plugin-${PLUGIN_NAME}
Architecture: any
Section: libs
Depends: \${shlibs:Depends},
         \${misc:Depends},
Description: nymea integration to use ${INTEGRATION} from ${VENDOR} with the Consolinno HEMS.
 This package is an automatically generated copy of the ${SOURCE} integration.
EOF

# copy *install.in file and replace contents
cp debian/nymea-plugin-${SOURCE}.install.in debian/nymea-plugin-${PLUGIN_NAME}.install.in
sed 's/'${SOURCE}'/'${PLUGIN_NAME}'/g' debian/nymea-plugin-${PLUGIN_NAME}.install.in -i

# sed '/'${SOURCE}'/{p;s/'${SOURCE}'/'${PLUGIN_NAME}'/g;}' debian/rules -i
