"""Writes the map color test packs (L-324) into tests/packs/map-color/BP and RP.

Run from anywhere: python tests/packs/map-color/tools/make_packs.py
Everything (JSON, PNG) is generated here; nothing is copied from elsewhere.
Raise VERSION and change the UUIDs when the game must not reuse an
installed copy.
"""
import json
import shutil
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
VERSION = [2, 0, 0]
ENGINE = [1, 26, 0]
BLOCK_FORMAT = "1.26.0"
BP_UUID, BP_MODULE = "4dc102d1-c827-4c60-a4eb-e0902365f95d", "fb958616-f275-4cdd-8d13-e2d150925273"
RP_UUID, RP_MODULE = "2af74d3a-3058-4faa-8240-5b3cb7243734", "5c996b7f-451d-4047-8df6-2c11babc587e"
MAGENTA = "#FF00FF"
# Deliberately not a key of terrain_texture.json: the game shows its missing texture.
MISSING = "lamiumtest_not_in_terrain_texture"

# name, texture key, RGB of its texture (None: no image), map_color, English name
BLOCKS = [
    ("look_cyan", "lamiumtest_look_cyan", (0x00, 0xE5, 0xFF), MAGENTA, "Map test A: cyan texture, magenta map_color"),
    ("look_yellow", "lamiumtest_look_yellow", (0xFF, 0xE0, 0x00), None, "Map test B: yellow texture, no map_color"),
    ("missing_magenta", MISSING, None, MAGENTA, "Map test C: missing texture, magenta map_color"),
    ("missing_plain", MISSING, None, None, "Map test D: missing texture, no map_color"),
]


def png(width, height, rgb):
    """A solid RGBA8 PNG."""
    row = b"\x00" + bytes((*rgb, 255)) * width
    raw = row * height

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")


def manifest(name, description, uuid, module_type, module_uuid, dependency=None):
    value = {
        "format_version": 2,
        "header": {
            "name": name,
            "description": description,
            "uuid": uuid,
            "version": VERSION,
            "min_engine_version": ENGINE,
        },
        "modules": [{"type": module_type, "uuid": module_uuid, "version": VERSION}],
    }
    if dependency:
        value["dependencies"] = [{"uuid": dependency, "version": VERSION}]
    return value


def block(name, texture, map_color):
    components = {
        # Likely why the first version showed the missing texture: without a
        # geometry the game seems to use blocks.json, which the pack lacks.
        "minecraft:geometry": "minecraft:geometry.full_block",
        "minecraft:material_instances": {"*": {"texture": texture, "render_method": "opaque"}},
        "minecraft:destructible_by_mining": {"seconds_to_destroy": 0.5},
    }
    if map_color:
        components["minecraft:map_color"] = map_color
    return {
        "format_version": BLOCK_FORMAT,
        "minecraft:block": {
            "description": {"identifier": f"lamiumtest:{name}", "menu_category": {"category": "construction"}},
            "components": components,
        },
    }


def main():
    bp, rp = ROOT / "BP", ROOT / "RP"
    for pack in (bp, rp):
        if pack.exists():
            shutil.rmtree(pack)
    write_json(bp / "manifest.json", manifest("Lamium map color test BP", "Four test blocks for Lamium's map colors",
                                              BP_UUID, "data", BP_MODULE, RP_UUID))
    write_json(rp / "manifest.json", manifest("Lamium map color test RP", "Textures for Lamium's map color test blocks",
                                              RP_UUID, "resources", RP_MODULE))
    texture_data = {}
    lang = []
    for name, texture, rgb, map_color in ((b[0], b[1], b[2], b[3]) for b in BLOCKS):
        write_json(bp / "blocks" / f"{name}.json", block(name, texture, map_color))
        if rgb:
            image = rp / "textures" / "blocks" / f"{texture}.png"
            image.parent.mkdir(parents=True, exist_ok=True)
            image.write_bytes(png(16, 16, rgb))
            texture_data[texture] = {"textures": f"textures/blocks/{texture}"}
    for name, *_rest, title in BLOCKS:
        lang.append(f"tile.lamiumtest:{name}.name={title}")
    write_json(rp / "textures" / "terrain_texture.json", {
        "resource_pack_name": "lamiumtest",
        "texture_name": "atlas.terrain",
        "padding": 8,
        "num_mip_levels": 4,
        "texture_data": texture_data,
    })
    texts = rp / "texts"
    texts.mkdir(parents=True, exist_ok=True)
    for code in ("en_US", "ja_JP"):
        (texts / f"{code}.lang").write_text("\n".join(lang) + "\n", encoding="utf-8", newline="\n")
    write_json(texts / "languages.json", ["en_US", "ja_JP"])


if __name__ == "__main__":
    main()
