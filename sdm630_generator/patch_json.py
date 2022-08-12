import json
import argparse
import uuid

def traverse_json(data, cb):
    for key,value in data.items():
        cb(key, value, data)
        if isinstance(value, dict):
            traverse_json(value, cb)
        elif isinstance(value, list):
            for val in value:
                if isinstance(val, str):
                    pass
                elif isinstance(val, list):
                    pass
                else:
                    traverse_json(val, cb)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generates a patched SDM630 JSON plugin info.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument(
        "JSON", help="Original JSON file"
    )
    parser.add_argument(
        "--output", help="Outputfile"
    )
    parser.add_argument(
        "--interfaces", help="Interfaces of new SDM630 plugin", required=True, nargs='+'
    )
#    parser.add_argument(
#        "--name", help="Name of new SDM630 thing class", required=True
#    )
    parser.add_argument(
        "--display_name", help="Display name of new SDM630 thing", required=True
    )
    args = parser.parse_args()
    with open(args.JSON, "r") as f:
        data=json.loads(f.read())

    def callback(key, value, data):
        if key=="id":
            data[key] = str(uuid.uuid4())

    traverse_json(data, callback)
    #data['vendors'][0]['thingClasses'][0]['name'] = args.name
    data['vendors'][0]['thingClasses'][0]['displayName'] = args.display_name
    data['vendors'][0]['thingClasses'][0]['interfaces'] = args.interfaces

    with open(args.output, "w") as f:
        json.dump(data,f, indent=4)

