import json
import argparse
import uuid

# Function to traverse plugin json file
# Used for adding offset
def traverse_json(data, cb, uuid_offset):
    for key, value in data.items():
        cb(key, value, data, uuid_offset)
        if isinstance(value, dict):
            traverse_json(value, cb, uuid_offset)
        elif isinstance(value, list):
            for val in value:
                if isinstance(val, str):
                    pass
                elif isinstance(val, list):
                    pass
                else:
                    traverse_json(val, cb, uuid_offset)

# Example:
# python3 ./generator/patch_json.py --file integrationpluginOcppCs.json \
#                                   --display_name 'Fox ESS WB' --vendor 'FoxESS' --uuid_offset 1
if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate a patched JSON plugin info.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    # Parse arguments
    parser.add_argument("--file",
                        required=True,
                        help="JSON file")
    parser.add_argument("--display_name",
                        help="Display name of the new integration",
                        required=True)
    parser.add_argument("--vendor",
                        help="Vendor info for the integration",
                        required=True)
    parser.add_argument("--uuid_offset",
                        help="Offset of uuids of new plugins",
                        required=True)
    parser.add_argument("--interfaces", 
                        help="Interfaces of the new integration (optional)",
                        nargs="+")
    parser.add_argument("--name", 
                        help="Name of the new thing class")
    args = parser.parse_args()

    # load plugin json file
    with open(args.file, "r") as json_file:
        data = json.loads(json_file.read())
    
    # define callback function to add uuid_offset
    def callback(key, value, data, uuid_offset):
        if key == "id":
            tmp = list(data[key])
            new = f"%03d" % (ord(tmp[-1]) + int(uuid_offset))
            tmp[-3] = new[0]
            tmp[-2] = new[1]
            tmp[-1] = new[2]
            data[key] = "".join(tmp)
    
    traverse_json(data, callback, args.uuid_offset)
    # print("Vendor will be replace with", args.vendor)
    data["vendors"][0]["displayName"] = args.vendor

    # print("DisplayName will be replace with", args.display_name)
    for i in range(len(args.display_name.split(','))):
        data["vendors"][0]["thingClasses"][i]["displayName"] = args.display_name.split(',')[i].strip()
        if args.interfaces != ['null']:
            # print("Vendor will be replace with", args.interfaces)
            data["vendors"][0]["thingClasses"][i]["interfaces"] = args.interfaces

    # overwrite json file with new values
    with open(args.file, "w") as json_file:
        json.dump(data, json_file, indent=4)
