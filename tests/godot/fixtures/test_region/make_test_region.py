#!/usr/bin/env python3
"""Writes test_region.glb, the first cooker proof's test asset (GDT-4, slice 9).

It stands in for a mesh made in Blender: what Blender's glTF exporter writes for a few objects, with the
same conventions (metres, Y up, counter-clockwise triangles seen from outside, a node per object, one mesh per
node, flat normals), so Godot imports it as it would import an artist's file. No Blender runs in CI, so the
file is generated: run this script again after changing it, and commit both.

    python3 make_test_region.py test_region.glb

The scene, in metres:
  Ground       a 30 m square, its top at y = 0
  Ramp         a wedge rising from y = 0 at x = -9 to y = 1 at x = -5, 4 m wide (14 degrees)
  Platform     a 10 m square box from y = 0 to y = 1, centred on the origin
  Rock         an eight-sided rock on the platform at (2, 1, 3), 1.2 m high
    Rock-rcol  its collision under the naming convention: a 1.4 m box, never drawn
"""

import json
import struct
import sys


def box(lo, hi):
    """A box as 6 faces of 2 triangles with flat normals (24 vertices)."""
    (x0, y0, z0), (x1, y1, z1) = lo, hi
    faces = [
        ((1, 0, 0), [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)]),
        ((-1, 0, 0), [(x0, y0, z1), (x0, y1, z1), (x0, y1, z0), (x0, y0, z0)]),
        ((0, 1, 0), [(x0, y1, z0), (x0, y1, z1), (x1, y1, z1), (x1, y1, z0)]),
        ((0, -1, 0), [(x0, y0, z1), (x0, y0, z0), (x1, y0, z0), (x1, y0, z1)]),
        ((0, 0, 1), [(x1, y0, z1), (x1, y1, z1), (x0, y1, z1), (x0, y0, z1)]),
        ((0, 0, -1), [(x0, y0, z0), (x0, y1, z0), (x1, y1, z0), (x1, y0, z0)]),
    ]
    return quads(faces)


def quads(faces):
    positions, normals, indices = [], [], []
    for normal, corners in faces:
        base = len(positions)
        positions += corners
        normals += [normal] * len(corners)
        indices += [base, base + 1, base + 2] if len(corners) == 3 else [base, base + 1, base + 2, base, base + 2, base + 3]
    return positions, normals, indices


def normal_of(a, b, c):
    u = [b[i] - a[i] for i in range(3)]
    v = [c[i] - a[i] for i in range(3)]
    n = [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]]
    length = sum(x * x for x in n) ** 0.5
    return tuple(x / length for x in n)


def ramp():
    """A wedge: x from -9 to -5, z from -2 to 2, the slope rising to y = 1 at x = -5."""
    a, b = (-9, 0, -2), (-9, 0, 2)          # the low edge
    c, d = (-5, 1, 2), (-5, 1, -2)          # the high edge
    e, f = (-5, 0, 2), (-5, 0, -2)          # under the high edge
    faces = [
        (normal_of(a, b, c), [a, b, c, d]),  # the slope
        ((1, 0, 0), [f, d, c, e]),           # the back, against the platform
        ((0, -1, 0), [b, a, f, e]),          # the bottom
        ((0, 0, 1), [b, e, c]),              # the sides
        ((0, 0, -1), [a, d, f]),
    ]
    return quads(faces)


def rock():
    """An eight-sided rock: a square ring at 0.4 m between a base and a top point, 1.2 m high."""
    ring = [(0.6, 0.4, 0), (0, 0.4, 0.6), (-0.6, 0.4, 0), (0, 0.4, -0.6)]
    top, bottom = (0.1, 1.2, 0.05), (0, 0, 0)
    faces = []
    for i in range(4):
        p, q = ring[i], ring[(i + 1) % 4]
        faces.append((normal_of(top, q, p), [top, q, p]))
        faces.append((normal_of(bottom, p, q), [bottom, p, q]))
    return quads(faces)


def main(out):
    meshes = {
        "Ground": box((-15, -0.2, -15), (15, 0, 15)),
        "Ramp": ramp(),
        "Platform": box((-5, 0, -5), (5, 1, 5)),
        "Rock": rock(),
        "Rock-rcol": box((-0.7, 0, -0.7), (0.7, 1.1, 0.7)),
    }
    binary = bytearray()
    views, accessors, gltf_meshes = [], [], []

    def add_view(data, target):
        while len(binary) % 4:
            binary.append(0)
        views.append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(data), "target": target})
        binary.extend(data)
        return len(views) - 1

    for name, (positions, normals, indices) in meshes.items():
        lo = [min(p[i] for p in positions) for i in range(3)]
        hi = [max(p[i] for p in positions) for i in range(3)]
        accessors.append({"bufferView": add_view(b"".join(struct.pack("<3f", *p) for p in positions), 34962),
                          "componentType": 5126, "count": len(positions), "type": "VEC3", "min": lo, "max": hi})
        accessors.append({"bufferView": add_view(b"".join(struct.pack("<3f", *n) for n in normals), 34962),
                          "componentType": 5126, "count": len(normals), "type": "VEC3"})
        accessors.append({"bufferView": add_view(b"".join(struct.pack("<H", i) for i in indices), 34963),
                          "componentType": 5123, "count": len(indices), "type": "SCALAR"})
        base = len(accessors) - 3
        gltf_meshes.append({"name": name, "primitives": [{"attributes": {"POSITION": base, "NORMAL": base + 1},
                                                          "indices": base + 2, "mode": 4}]})

    names = list(meshes)
    nodes = [{"name": n, "mesh": i} for i, n in enumerate(names)]
    nodes[names.index("Rock")]["translation"] = [2, 1, 3]
    nodes[names.index("Rock")]["children"] = [names.index("Rock-rcol")]
    roots = [names.index(n) for n in ("Ground", "Ramp", "Platform", "Rock")]
    gltf = {
        "asset": {"version": "2.0", "generator": "RunicEngine make_test_region.py (stands in for Blender's exporter)"},
        "scene": 0,
        "scenes": [{"name": "test_region", "nodes": roots}],
        "nodes": nodes,
        "meshes": gltf_meshes,
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(binary)}],
    }
    text = json.dumps(gltf, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    while len(binary) % 4:
        binary.append(0)
    total = 12 + 8 + len(text) + 8 + len(binary)
    with open(out, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(text), b"JSON") + text)
        f.write(struct.pack("<I4s", len(binary), b"BIN\0") + bytes(binary))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "test_region.glb")
