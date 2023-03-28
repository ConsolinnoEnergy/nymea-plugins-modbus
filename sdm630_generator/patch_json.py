import json
import argparse
import uuid


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


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generates a patched SDM630 JSON plugin info.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument("JSON", help="Original JSON file")
    parser.add_argument("--output", help="Outputfile")
    parser.add_argument(
        "--interfaces", help="Interfaces of new SDM630 plugin", required=True, nargs="+"
    )
    #    parser.add_argument(
    #        "--name", help="Name of new SDM630 thing class", required=True
    #    )
    parser.add_argument(
        "--display_name_sdm630", help="Display name of new SDM630 thing", required=True
    )
     parser.add_argument(
        "--display_name_sdm72", help="Display name of new SDM72 thing", required=True
    )
    parser.add_argument(
        "--uuid_offset", type=int, help="Offset of uuids of new plugin ", default=1
    )
    args = parser.parse_args()
    with open(args.JSON, "r") as f:
        data = json.loads(f.read())

    def callback(key, value, data, uuid_offset):
        if key == "id":
            tmp = list(data[key])
            # Adding offset to ascii int of last character of uuid
            new = f"%03d" % (ord(tmp[-1]) + 1)
            tmp[-3] = new[0]
            tmp[-2] = new[1]
            tmp[-1] = new[2]
            data[key] = "".join(tmp)

    traverse_json(data, callback, args.uuid_offset)
    # data['vendors'][0]['thingClasses'][0]['name'] = args.name
    data["vendors"][0]["thingClasses"][0]["displayName"] = args.display_name_sdm630
    data["vendors"][0]["thingClasses"][0]["interfaces"] = args.interfaces
    data["vendors"][0]["thingClasses"][1]["displayName"] = args.display_name_sdm72
    data["vendors"][0]["thingClasses"][1]["interfaces"] = args.interfaces

    with open(args.output, "w") as f:
        json.dump(data, f, indent=4)
