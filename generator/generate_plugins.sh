#!/bin/bash

main() {
  # Iterate over the plugin_list.txt which includes a list of all template plugins
  while IFS= read -r plugin
  do
    echo "--- Creating copies of the '$plugin' integration. ---"
    iterate_integrations $plugin
  done < ./generator/plugin_list.txt
}

iterate_integrations() {
  # Iterate over the json file in the template folder
  DEVICE_LIST="./$1/generator_list.json"
  VENDORS=$(jq '. | keys[]' ${DEVICE_LIST})

  # Iterate over vendors
  while IFS= read -r vendor
  do
    echo "-- Generating plugins for $vendor vendor. --"
    INTEGRATIONS=$(jq '.'"${vendor}"' | keys[]' ${DEVICE_LIST})
    # Iterate over devices
    while IFS= read -r device
    do
      echo "- Generating plugin for the $device device."
      integration=$(jq -c '.'"${vendor}"'.'"${device}" ${DEVICE_LIST})
      create_integration "$1" "${vendor}" "${device}" "${integration}"
    done <<< "${INTEGRATIONS}"
  done <<< "${VENDORS}"
}

create_integration()
{
  current_vendor=${2//\"}
  current_device=${3//\"}
  filename=$(echo $4 | jq '.filename')
  # echo "Filename of the new integration will be: $filename"
  filename=${filename//\"}
  interfaces=$(echo $4 | jq '.interfaces')
  # echo "Interfaces of the new integration: $interfaces (null if the interfaces are not changed)"
  interfaces=${interfaces//\"}
  uuid_offset=$(echo $4 | jq '.uuid_offset')
  # echo "UUID Offset of the new integration: $uuid_offset"

  ./generator/generic_generator.sh --source $1 --vendor "$current_vendor" --integration "$current_device" --plugin_name $filename --interfaces "$interfaces" --uuid_offset $uuid_offset
}

main
