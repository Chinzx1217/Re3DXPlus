#!/usr/bin/env python3
"""Generate one city's LIT1 light locator file from scalar StreetLight/BuildingLight Lua assignments."""
import os
import re
import sys

from pack_lights import BUILDING_TYPES, STREET_TYPES, locators, write_bin


def lua_assignments(path):
    """Read the scene's direct string assignments, ignoring Lua line and block comments."""
    with open(path, encoding="utf-8", errors="replace") as stream:
        source = stream.read()
    source = re.sub(r"--\[\[.*?\]\]", "", source, flags=re.S)
    source = re.sub(r"--[^\n]*", "", source)
    found = {}
    for key in ("StreetLight", "BuildingLight"):
        match = re.search(
            r'^\s*' + re.escape(key) + r'\s*=\s*"([^"]+\.igb)"',
            source, flags=re.I | re.M
        )
        if match:
            found[key] = match.group(1)
    return found


def resolve(data, ref):
    clean = ref.replace("\\", "/").lstrip("/")
    if clean.startswith("data/"):
        clean = clean[5:]
    path = os.path.join(data, clean)
    if not os.path.isfile(path):
        raise FileNotFoundError("Lua light model does not exist: %s" % path)
    return path


def collect(data, city, key, types, required=False):
    lights = []
    seen = set()
    for time in ("daytime", "night"):
        scene = os.path.join(data, city, time, time + ".lua")
        if not os.path.isfile(scene):
            print("Skipping missing scene Lua: %s" % scene, flush=True)
            continue
        ref = lua_assignments(scene).get(key)
        if not ref:
            continue
        path = resolve(data, ref)
        if path in seen:
            continue
        seen.add(path)
        print("%s (%s): %s -> %s" % (key, time, ref, path), flush=True)
        lights.extend(locators(path, types))

    if required and not seen:
        raise RuntimeError("%s has no %s assignment in daytime/night Lua" % (city, key))
    print("%s %s: %d source model(s), %d light locator(s)" %
          (city, key, len(seen), len(lights)), flush=True)
    return lights


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: pack_city_lights.py <arcade data dir> <city>")
    data, city = sys.argv[1:3]
    # Hakone has no StreetLight/BuildingLight assignment in either scene;
    # the packer writes a valid LIT1 header with zero locators for that map.
    allow_empty = city.lower() == "hakone"
    street = collect(data, city, "StreetLight", STREET_TYPES, required=not allow_empty)
    building = collect(data, city, "BuildingLight", BUILDING_TYPES, required=False)
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    out = os.path.join(root, "data", "lights_%s.bin" % city)
    write_bin(out, street, building)


if __name__ == "__main__":
    main()
