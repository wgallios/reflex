#!/usr/bin/env python3
"""Generate Reflex Engine's original, dependency-free Phase 3 collision test GLB."""

import json
import pathlib
import struct
import zlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "assets" / "levels" / "test_scene.glb"


def align(data: bytearray, alignment: int = 4) -> None:
    data.extend(b"\0" * ((-len(data)) % alignment))


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))


def checker_png() -> bytes:
    width, height = 2, 2
    rows = (
        b"\0" + bytes((210, 190, 135, 255, 75, 85, 95, 255))
        + b"\0" + bytes((75, 85, 95, 255, 210, 190, 135, 255))
    )
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(rows, 9))
        + png_chunk(b"IEND", b"")
    )


def pack_vertices(vertices: list[tuple[float, ...]]) -> bytes:
    return b"".join(struct.pack("<8f", *vertex) for vertex in vertices)


floor_vertices = [
    (-12, 0, -12, 0, 1, 0, 0, 0),
    (12, 0, -12, 0, 1, 0, 12, 0),
    (12, 0, 12, 0, 1, 0, 12, 12),
    (-12, 0, 12, 0, 1, 0, 0, 12),
]
floor_indices = (0, 2, 1, 0, 3, 2)

# Twenty-four vertices give each cube face independent normals and UVs.
cube_vertices: list[tuple[float, ...]] = []
faces = [
    ((0, 0, 1), ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1))),
    ((0, 0, -1), ((1, -1, -1), (-1, -1, -1), (-1, 1, -1), (1, 1, -1))),
    ((1, 0, 0), ((1, -1, 1), (1, -1, -1), (1, 1, -1), (1, 1, 1))),
    ((-1, 0, 0), ((-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1))),
    ((0, 1, 0), ((-1, 1, 1), (1, 1, 1), (1, 1, -1), (-1, 1, -1))),
    ((0, -1, 0), ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1))),
]
uvs = ((0, 0), (1, 0), (1, 1), (0, 1))
for normal, corners in faces:
    for corner, uv in zip(corners, uvs):
        cube_vertices.append((*corner, *normal, *uv))
cube_indices = tuple(index for face in range(6) for index in
                     (face * 4, face * 4 + 1, face * 4 + 2,
                      face * 4, face * 4 + 2, face * 4 + 3))

binary = bytearray()
views: list[dict[str, int]] = []


def add_view(payload: bytes, target: int | None = None, stride: int | None = None) -> int:
    align(binary)
    offset = len(binary)
    binary.extend(payload)
    view: dict[str, int] = {"buffer": 0, "byteOffset": offset, "byteLength": len(payload)}
    if target is not None:
        view["target"] = target
    if stride is not None:
        view["byteStride"] = stride
    views.append(view)
    return len(views) - 1


floor_vertex_view = add_view(pack_vertices(floor_vertices), 34962, 32)
floor_index_view = add_view(struct.pack("<6H", *floor_indices), 34963)
cube_vertex_view = add_view(pack_vertices(cube_vertices), 34962, 32)
cube_index_view = add_view(bytes(cube_indices), 34963)
image_view = add_view(checker_png())
align(binary)

accessors = [
    {"bufferView": floor_vertex_view, "byteOffset": 0, "componentType": 5126,
     "count": 4, "type": "VEC3", "min": [-12, 0, -12], "max": [12, 0, 12]},
    {"bufferView": floor_vertex_view, "byteOffset": 12, "componentType": 5126,
     "count": 4, "type": "VEC3"},
    {"bufferView": floor_vertex_view, "byteOffset": 24, "componentType": 5126,
     "count": 4, "type": "VEC2"},
    {"bufferView": floor_index_view, "componentType": 5123, "count": 6, "type": "SCALAR"},
    {"bufferView": cube_vertex_view, "byteOffset": 0, "componentType": 5126,
     "count": 24, "type": "VEC3", "min": [-1, -1, -1], "max": [1, 1, 1]},
    {"bufferView": cube_vertex_view, "byteOffset": 12, "componentType": 5126,
     "count": 24, "type": "VEC3"},
    {"bufferView": cube_vertex_view, "byteOffset": 24, "componentType": 5126,
     "count": 24, "type": "VEC2"},
    {"bufferView": cube_index_view, "componentType": 5121, "count": 36, "type": "SCALAR"},
]

nodes: list[dict] = [
    {"name": "Textured Floor", "mesh": 0},
    {"name": "player_spawn", "translation": [0, 0.02, 8]},
]


def box(name: str, translation: list[float], scale: list[float],
        rotation: list[float] | None = None, extras: dict | None = None) -> None:
    node: dict = {"name": name, "mesh": 1, "translation": translation, "scale": scale}
    if rotation is not None:
        node["rotation"] = rotation
    if extras is not None:
        node["extras"] = extras
    nodes.append(node)


# Enclosure, doorway, corridor, low ceiling, and corner tests.
box("wall_west", [-11.5, 2, 0], [0.5, 2, 12])
box("wall_east", [11.5, 2, 0], [0.5, 2, 12])
box("wall_north", [0, 2, -11.5], [12, 2, 0.5])
box("wall_south_left", [-7, 2, 11.5], [5, 2, 0.5])
box("wall_south_right", [7, 2, 11.5], [5, 2, 0.5])
box("door_header", [0, 3.5, 11.5], [2, 0.5, 0.5])
box("low_ceiling", [-7, 2.25, 5], [3, 0.2, 3])
box("corridor_left", [5, 1.5, 5], [0.25, 1.5, 3])
box("corridor_right", [7, 1.5, 5], [0.25, 1.5, 3])
box("inside_corner_a", [8.5, 1.5, -7], [2.5, 1.5, 0.25])
box("inside_corner_b", [8.5, 1.5, -9.25], [0.25, 1.5, 2])

# Four 0.3 m steps. Cube scale is half extent, so each top rises by 0.3 m.
for step in range(4):
    height = 0.3 * (step + 1)
    box(f"step_{step + 1}", [-6, height / 2, 1.5 - step * 0.55],
        [1.5, height / 2, 0.275])

# A low step, a deliberately too-tall ledge, and a fall platform.
box("low_step", [1.5, 0.15, 4], [1.5, 0.15, 1])
box("high_ledge", [1.5, 0.5, 0.5], [1.5, 0.5, 1])
box("elevated_platform", [0, 2.0, -8], [2.5, 0.2, 2])

# Thin rotated boxes form approximately 22-degree and 58-degree ramps.
box("walkable_ramp", [-4, 0.75, -5], [1.5, 0.12, 2.0],
    [-0.190809, 0, 0, 0.981627])
box("steep_ramp", [3, 1.25, -5], [1.5, 0.12, 1.5],
    [-0.48481, 0, 0, 0.87462])

# Exercises extras-based collision exclusion.
box("visual_marker", [0, 1.5, 7], [0.25, 0.25, 0.25],
    extras={"collision": False})

document = {
    "asset": {"version": "2.0", "generator": "Reflex Engine test scene generator"},
    "scene": 0,
    "scenes": [{"name": "Phase 3 Collision Course", "nodes": list(range(len(nodes)))}],
    "nodes": nodes,
    "meshes": [
        {"name": "Floor", "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1,
                                                               "TEXCOORD_0": 2},
                                             "indices": 3, "material": 0}]},
        {"name": "Cube", "primitives": [{"attributes": {"POSITION": 4, "NORMAL": 5,
                                                              "TEXCOORD_0": 6},
                                            "indices": 7, "material": 1}]},
    ],
    "materials": [
        {"name": "Embedded Checker", "pbrMetallicRoughness": {
            "baseColorTexture": {"index": 0}, "roughnessFactor": 1}},
        {"name": "Untextured Rust", "pbrMetallicRoughness": {
            "baseColorFactor": [0.65, 0.16, 0.07, 1], "roughnessFactor": 1}},
    ],
    "textures": [{"source": 0, "sampler": 0}],
    "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}],
    "images": [{"name": "Embedded Checker", "bufferView": image_view, "mimeType": "image/png"}],
    "accessors": accessors,
    "bufferViews": views,
    "buffers": [{"byteLength": len(binary)}],
}

json_bytes = json.dumps(document, separators=(",", ":")).encode("utf-8")
json_bytes += b" " * ((-len(json_bytes)) % 4)
total_length = 12 + 8 + len(json_bytes) + 8 + len(binary)
glb = (
    struct.pack("<III", 0x46546C67, 2, total_length)
    + struct.pack("<II", len(json_bytes), 0x4E4F534A) + json_bytes
    + struct.pack("<II", len(binary), 0x004E4942) + bytes(binary)
)

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_bytes(glb)
print(f"Wrote {OUTPUT} ({len(glb)} bytes)")
