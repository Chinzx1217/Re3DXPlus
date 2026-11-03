#!/usr/bin/env python3
"""Generate one city's LIT1 light locator file from its Lua scene references."""
import os
import re
import sys

from pack_lights import BUILDING_TYPES, STREET_TYPES, locators, write_bin


def lua_references(data, city, key):
    roots = [os.path.join(data, city), os.path.join(data, "lua_common")]
    found = []
    for root in roots:
        if not os.path.isdir(root):
            continue
        for base, _, files in os.walk(root):
            for filename in files:
                if not filename.lower().endswith(".lua"):
                    continue
                path = os.path.join(base, filename)
                try:
                    source = open(path, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                # Scene tables use assignments such as StreetLight = { "path.igb" }.
                pattern = r"\\b" + re.escape(key) + r"\\s*=\\s*\\{([^{}]*)\\}"
                for match in re.finditer(pattern, source, flags=re.S):
                    found.extend(re.findall(r'"([^"]+\\.igb)"', match.group(1), flags=re.I))
    # Preserve order while removing duplicates.
    return list(dict.fromkeys(found))


def resolve(data, city, ref):
    clean = ref.replace("\\\\", "/").lstrip("/")
    candidates = []
    if clean.startswith("data/"):
        candidates.append(os.path.join(os.path.dirname(data), clean))
        clean = clean[5:]
    candidates.extend((os.path.join(data, city, clean), os.path.join(data, clean)))
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
    refs = lua_references(data, city, key)
    if not refs:
        raise RuntimeError("%s reference not found in %s or lua_common Lua files" % (key, city))
    lights = []
    for ref in refs:
        path = resolve(data, city, ref)
        print("%s: %s -> %s" % (key, ref, path), flush=True)
        lights.extend(locators(path, types))
    return lights


def main():
    if len(sys.argv) != 3:
        raise SystemExit("Usage: pack_city_lights.py <arcade data dir> <city>")
    data, city = sys.argv[1:3]
    street = collect(data, city, "StreetLight", STREET_TYPES)
    building = collect(data, city, "BuildingLight", BUILDING_TYPES)
    out = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "..", "data",
                       "lights_%s.bin" % city)
    # The output directory is the current repository's data/ directory.
    out = os.path.abspath(out)
    write_bin(out, street, building)


if __name__ == "__main__":
    main()
