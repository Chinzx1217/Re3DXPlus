#!/usr/bin/env python3
"""Generate one city's LIT1 light locator file from its daytime/night scene Lua lists."""
import os
import re
import sys

from pack_lights import BUILDING_TYPES, STREET_TYPES, locators, write_bin


def lua_lists(path):
    """Read model lists using the same Lua parsing rules as pack_city.py."""
    with open(path, encoding="utf-8", errors="replace") as stream:
        source = stream.read()
    source = re.sub(r"--\\[\\[.*?\\]\\]", "", source, flags=re.S)
    source = re.sub(r"--[^\\n]*", "", source)
    lists = {}
    for match in re.finditer(r"\\b(\\w+)\\s*=\\s*\\{([^{}]*)\\}", source):
        files = re.findall(r'"([^"]+\\.igb)"', match.group(2), flags=re.I)
        if files or not match.group(2).strip():
            lists[match.group(1)] = files
    return lists


def resolve(data, scene_dir, ref):
    clean = ref.replace("\\\\", "/").lstrip("/")
    candidates = []
    if clean.startswith("data/"):
        candidates.append(os.path.join(data, clean[5:]))
    candidates.extend((os.path.join(scene_dir, clean), os.path.join(data, clean)))
    for path in candidates:
        if os.path.isfile(path):
            return path

    basename = os.path.basename(clean)
    matches = []
    for base, _, files in os.walk(data):
        if basename in files:
            matches.append(os.path.join(base, basename))
    if len(matches) == 1:
        return matches[0]
    raise FileNotFoundError("cannot uniquely resolve %s (matches: %d)" % (ref, len(matches)))


def collect(data, city, key, types):
    lights = []
    seen = set()
    for time in ("daytime", "night"):
        scene_dir = os.path.join(data, city, time)
        scene = os.path.join(scene_dir, time + ".lua")
        if not os.path.isfile(scene):
            print("Skipping missing scene Lua: %s" % scene, flush=True)
            continue
        lists = lua_lists(scene)
        refs = lists.get(key, [])
        for ref in refs:
            path = resolve(data, scene_dir, ref)
            if path in seen:
                continue
            seen.add(path)
            print("%s (%s): %s -> %s" % (key, time, ref, path), flush=True)
            lights.extend(locators(path, types))
    print("%s %s: %d source model(s), %d light locator(s)" %
          (city, key, len(seen), len(lights)), flush=True)
    return lights


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: pack_city_lights.py <arcade data dir> <city>")
    data, city = sys.argv[1:3]
    street = collect(data, city, "StreetLight", STREET_TYPES)
    building = collect(data, city, "BuildingLight", BUILDING_TYPES)
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    out = os.path.join(root, "data", "lights_%s.bin" % city)
    write_bin(out, street, building)


if __name__ == "__main__":
    main()
