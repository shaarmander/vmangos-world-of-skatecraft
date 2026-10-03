#!/usr/bin/env python3
"""Check extracted data headers against this checkout before deploying them.

This checks compatibility and basic truncation, not walkability. Follow with an
in-world `.mmap loc`, `.mmap stats`, and a creature chase test.
"""
import argparse
from collections import Counter
from pathlib import Path
import re
import struct
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("data", type=Path)
    parser.add_argument("--require-tile", action="append", default=[])
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    shared = (repo / "src/game/Maps/MoveMapSharedDefines.h").read_text()
    version = int(re.search(r"#define MMAP_VERSION\s+(\d+)", shared)[1])
    detour = (repo / "dep/recastnavigation/Detour/Include/DetourNavMesh.h").read_text()
    dt_version = int(re.search(r"DT_NAVMESH_VERSION\s*=\s*(\d+)", detour)[1])
    vmap_source = (repo / "src/game/vmap/VMapDefinitions.h").read_text()
    vmap_magic = re.search(r'VMAP_MAGIC\[\]\s*=\s*"([^"]+)"', vmap_source)[1].encode()
    grid = (repo / "src/game/Maps/GridMap.cpp").read_text()
    map_version = re.search(r'MAP_VERSION_MAGIC\s*=\s*"([^"]+)"', grid)[1].encode()
    errors = []
    counts = Counter()
    versions = Counter()
    for sub, pattern, magic in [
        ("maps", "*.map", b"MAPS" + map_version),
        ("vmaps", "*.vmtree", vmap_magic),
        ("vmaps", "*.vmtile", vmap_magic),
        ("vmaps", "*.vmo", vmap_magic),
    ]:
        files = list((args.data / sub).glob(pattern))
        counts[pattern] = len(files)
        if not files:
            errors.append(f"missing {sub}/{pattern}")
        for file in files:
            with file.open("rb") as stream:
                if stream.read(len(magic)) != magic:
                    errors.append(f"{file.name}: incompatible header")
    for file in (args.data / "mmaps").glob("*.mmtile"):
        counts["*.mmtile"] += 1
        with file.open("rb") as stream:
            header = stream.read(20)
        if len(header) != 20:
            errors.append(f"{file.name}: truncated header")
            continue
        magic, dt, gen, size, liquids = struct.unpack("<5I", header)
        versions[gen] += 1
        if (magic, dt, gen) != (0x4D4D4150, dt_version, version):
            errors.append(f"{file.name}: magic={magic:#x}, Detour={dt}, generator={gen}; expected Detour={dt_version}, generator={version}")
        elif file.stat().st_size != 20 + size or size == 0 or liquids > 1:
            errors.append(f"{file.name}: invalid payload size/liquid flag")
        if not file.name.startswith("go") and not (file.parent / (file.name[:3] + ".mmap")).is_file():
            errors.append(f"{file.name}: missing parent .mmap")
    if not counts["*.mmtile"]:
        errors.append("no movement tiles")
    for tile in args.require_tile:
        if not (args.data / "mmaps" / (tile + ".mmtile")).is_file():
            errors.append(f"required tile missing: {tile}")
    if not (args.data / "5875/dbc/Map.dbc").is_file():
        errors.append("missing 5875/dbc/Map.dbc")
    print(f"Files: {dict(counts)}; movement generator versions: {dict(versions)}")
    for error in errors[:15]:
        print(error, file=sys.stderr)
    if errors:
        print(f"FAIL: {len(errors)} errors. Regenerate with this checkout's extractors; do not edit version bytes.", file=sys.stderr)
        return 1
    print("PASS: compatible, non-truncated navigation headers; in-world verification still required.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
