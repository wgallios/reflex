#!/usr/bin/env python3
"""Generate original, dependency-free Phase 6 placeholder campaign assets."""

from __future__ import annotations

import math
import struct
import wave
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def tone(path: Path, frequency: float, duration: float, volume: float = 0.15) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    sample_rate = 22_050
    frames = int(sample_rate * duration)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(sample_rate)
        for sample in range(frames):
            envelope = min(1.0, sample / 100.0, (frames - sample) / 200.0)
            value = int(32767 * volume * envelope * math.sin(
                2.0 * math.pi * frequency * sample / sample_rate
            ))
            output.writeframesraw(struct.pack("<h", value))


def animated_glb(path: Path, clips: list[str]) -> None:
    """Write a tiny original skinned triangle with named rotation clips."""
    binary = bytearray()
    views: list[dict[str, int]] = []
    accessors: list[dict[str, object]] = []

    def add(data: bytes, component: int, value_type: str, count: int,
            target: int | None = None) -> int:
        while len(binary) % 4:
            binary.append(0)
        offset = len(binary)
        binary.extend(data)
        view: dict[str, int] = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
        if target is not None:
            view["target"] = target
        view_index = len(views)
        views.append(view)
        accessors.append({"bufferView": view_index, "componentType": component,
                          "count": count, "type": value_type})
        return len(accessors) - 1

    positions = add(struct.pack("<9f", -0.35, 0.0, 0.0, 0.35, 0.0, 0.0,
                                0.0, 1.0, 0.0), 5126, "VEC3", 3, 34962)
    normals = add(struct.pack("<9f", *([0.0, 0.0, 1.0] * 3)), 5126, "VEC3", 3, 34962)
    uvs = add(struct.pack("<6f", 0.0, 0.0, 1.0, 0.0, 0.5, 1.0), 5126, "VEC2", 3, 34962)
    joints = add(bytes([0, 1, 0, 0] * 3), 5121, "VEC4", 3, 34962)
    weights = add(struct.pack("<12f", *([0.25, 0.75, 0.0, 0.0] * 3)), 5126, "VEC4", 3, 34962)
    indices = add(struct.pack("<3H", 0, 1, 2), 5123, "SCALAR", 3, 34963)
    identity = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)
    inverse_binds = add(struct.pack("<32f", *(identity + identity)), 5126, "MAT4", 2)
    times = add(struct.pack("<3f", 0.0, 0.5, 1.0), 5126, "SCALAR", 3)
    accessors[times]["min"] = [0.0]
    accessors[times]["max"] = [1.0]
    animations = []
    for clip_index, name in enumerate(clips):
        angle = 0.08 + clip_index * 0.03
        rotations = add(struct.pack("<12f", 0.0, 0.0, 0.0, 1.0,
                                    0.0, math.sin(angle), 0.0, math.cos(angle),
                                    0.0, 0.0, 0.0, 1.0), 5126, "VEC4", 3)
        animations.append({"name": name, "samplers": [{"input": times,
            "output": rotations, "interpolation": "LINEAR"}],
            "channels": [{"sampler": 0, "target": {"node": 2, "path": "rotation"}}]})
    document = {
        "asset": {"version": "2.0", "generator": "Reflex Phase 6 asset generator"},
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": views,
        "accessors": accessors,
        "meshes": [{"name": "placeholder", "primitives": [{"attributes": {
            "POSITION": positions, "NORMAL": normals, "TEXCOORD_0": uvs,
            "JOINTS_0": joints, "WEIGHTS_0": weights}, "indices": indices}]}],
        "nodes": [{"name": "skinned_mesh", "mesh": 0, "skin": 0},
                  {"name": "root", "children": [2]},
                  {"name": "animated_joint", "translation": [0.0, 0.5, 0.0]}],
        "skins": [{"name": "placeholder_rig", "joints": [1, 2],
                   "skeleton": 1, "inverseBindMatrices": inverse_binds}],
        "animations": animations,
        "scenes": [{"nodes": [0, 1]}], "scene": 0,
    }
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * ((4 - len(encoded) % 4) % 4)
    binary += b"\0" * ((4 - len(binary) % 4) % 4)
    total = 12 + 8 + len(encoded) + 8 + len(binary)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(struct.pack("<III", 0x46546C67, 2, total) +
                     struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
                     struct.pack("<II", len(binary), 0x004E4942) + binary)


def campaign_level(source: Path, destination: Path, groups: list[str]) -> None:
    data = source.read_bytes()
    json_length, json_type = struct.unpack_from("<II", data, 12)
    if json_type != 0x4E4F534A:
        raise ValueError("source GLB has no JSON chunk")
    document = json.loads(data[20:20 + json_length].decode("utf-8").rstrip(" \0"))
    enemy_index = 0
    for node in document.get("nodes", []):
        extras = node.get("extras", {})
        if extras.get("gameplay_type") == "enemy_spawn":
            extras["starts_active"] = False
            extras["group"] = groups[min(enemy_index, len(groups) - 1)]
            enemy_index += 1
    binary_offset = 20 + json_length
    binary_chunk = data[binary_offset:]
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * ((4 - len(encoded) % 4) % 4)
    total = 12 + 8 + len(encoded) + len(binary_chunk)
    destination.write_bytes(struct.pack("<III", 0x46546C67, 2, total) +
                            struct.pack("<II", len(encoded), 0x4E4F534A) +
                            encoded + binary_chunk)


def main() -> None:
    levels = ROOT / "assets" / "levels"
    source = levels / "test_scene.glb"
    if not source.exists():
        raise SystemExit("run tools/generate_test_scene.py first")
    campaign_level(source, levels / "phase6_level01.glb", ["training_wave"])
    campaign_level(source, levels / "phase6_level02.glb",
                   ["tunnel_wave_01", "tunnel_wave_01", "tunnel_wave_02"])
    audio = ROOT / "assets" / "audio" / "generated"
    tone(audio / "weapon.wav", 180.0, 0.09)
    tone(audio / "enemy.wav", 110.0, 0.18)
    tone(audio / "objective.wav", 660.0, 0.20)
    tone(audio / "music.wav", 82.0, 2.0, 0.05)
    animated_glb(ROOT / "assets" / "models" / "grunt.glb",
                 ["idle", "walk", "attack", "pain", "death"])
    animated_glb(ROOT / "assets" / "models" / "pistol_viewmodel.glb",
                 ["equip", "idle", "fire", "reload", "holster"])
    print("Generated Phase 6 level copies and placeholder WAV files.")


if __name__ == "__main__":
    main()
