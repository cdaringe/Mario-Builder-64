#!/usr/bin/env python3
"""Generate libmb64 mesh lookup data from MB64 game terrain sources."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

DIRECTION_MAP = {
    "MB64_DIRECTION_UP": "MB64_MESH_FACE_TOP",
    "MB64_DIRECTION_DOWN": "MB64_MESH_FACE_BOTTOM",
    "MB64_DIRECTION_POS_X": "MB64_MESH_FACE_POS_X",
    "MB64_DIRECTION_NEG_X": "MB64_MESH_FACE_NEG_X",
    "MB64_DIRECTION_POS_Z": "MB64_MESH_FACE_POS_Z",
    "MB64_DIRECTION_NEG_Z": "MB64_MESH_FACE_NEG_Z",
}

SPECIAL_TERRAIN = {
    "TILE_TYPE_FENCE": "mb64_terrain_fence",
    "TILE_TYPE_POLE": "mb64_terrain_pole",
}

SPECIAL_COLLISION_TERRAIN = {
    "TILE_TYPE_FENCE": "mb64_terrain_fence_col",
}

SPECIAL_FACE_ARRAYS = {
    "TILE_TYPE_BARS": [
        "mb64_terrain_bars_unconnected_quad",
        "mb64_terrain_bars_connected_quads",
        "mb64_terrain_bars_center_quads",
    ],
}


def strip_line_comment(line: str) -> str:
    return line.split("//", 1)[0]


def split_top_level(text: str) -> list[str]:
    out: list[str] = []
    cur: list[str] = []
    depth = 0
    in_string = False
    escape = False
    for ch in text:
        if in_string:
            cur.append(ch)
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
            cur.append(ch)
        elif ch == "{":
            depth += 1
            cur.append(ch)
        elif ch == "}":
            depth -= 1
            cur.append(ch)
        elif ch == "," and depth == 0:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    tail = "".join(cur).strip()
    if tail:
        out.append(tail)
    return out


def extract_initializer(text: str, needle: str) -> str:
    start = text.index(needle)
    brace = text.index("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[brace + 1:i]
    raise ValueError(f"unterminated initializer: {needle}")


def clean_initializer(text: str) -> str:
    return "\n".join(strip_line_comment(line) for line in text.splitlines())


def unwrap_braces(text: str) -> str:
    text = text.strip()
    if text.startswith("{") and text.endswith("}"):
        return text[1:-1].strip()
    return text


def parse_vertex(text: str) -> tuple[int, int, int]:
    values = [int(part.strip(), 0) for part in unwrap_braces(text).split(",") if part.strip()]
    if len(values) != 3:
        raise ValueError(f"bad vertex: {text}")
    return values[0], values[1], values[2]


def parse_uv(text: str) -> tuple[int, int]:
    values = [int(part.strip(), 0) for part in unwrap_braces(text).split(",") if part.strip()]
    if len(values) != 2:
        raise ValueError(f"bad uv: {text}")
    return values[0], values[1]


def parse_uv_arrays(data_text: str) -> dict[str, list[tuple[int, int]]]:
    arrays: dict[str, list[tuple[int, int]]] = {}
    pattern = re.compile(r"s8\s+(\w+)\[(?:3|4)\]\[2\]\s*=\s*\{", re.M)
    for match in pattern.finditer(data_text):
        name = match.group(1)
        body = extract_initializer(data_text, match.group(0)[:-1])
        arrays[name] = [parse_uv(uv) for uv in split_top_level(body)]
    return arrays


def parse_poly_entries(data_text: str) -> dict[str, list[dict[str, object]]]:
    entries: dict[str, list[dict[str, object]]] = {}
    uv_arrays = parse_uv_arrays(data_text)
    pattern = re.compile(r"struct\s+mb64_terrain_poly\s+(\w+)\[\]\s*=\s*\{", re.M)
    for match in pattern.finditer(data_text):
        name = match.group(1)
        body = extract_initializer(data_text, match.group(0)[:-1])
        polys: list[dict[str, object]] = []
        for raw_entry in split_top_level(clean_initializer(body)):
            if not raw_entry:
                continue
            fields = split_top_level(unwrap_braces(raw_entry))
            if len(fields) < 3:
                continue
            vertex_fields = split_top_level(unwrap_braces(fields[0]))
            vertices = [parse_vertex(v) for v in vertex_fields]
            if len(vertices) not in (3, 4):
                raise ValueError(f"{name}: expected 3 or 4 vertices, got {len(vertices)}")
            direction = DIRECTION_MAP[fields[1].strip()]
            faceshape = fields[2].strip()
            growth_type = fields[3].strip() if len(fields) >= 4 else "0"
            altuvs: list[tuple[int, int]] | None = None
            if len(fields) >= 5:
                altuv_field = fields[4].strip()
                altuv_name = altuv_field[1:] if altuv_field.startswith("&") else altuv_field
                if altuv_name != "NULL":
                    if altuv_name not in uv_arrays:
                        raise ValueError(f"{name}: unknown altuv array {altuv_name}")
                    altuvs = uv_arrays[altuv_name]
            polys.append({
                "vertices": vertices,
                "direction": direction,
                "faceshape": faceshape,
                "growth_type": growth_type,
                "altuvs": altuvs,
            })
        entries[name] = polys
    return entries


def parse_terrains(data_text: str) -> dict[str, tuple[str | None, str | None]]:
    terrains: dict[str, tuple[str | None, str | None]] = {}
    pattern = re.compile(r"struct\s+mb64_terrain\s+(\w+)\s*=\s*\{", re.M)
    for match in pattern.finditer(data_text):
        name = match.group(1)
        body = extract_initializer(data_text, match.group(0)[:-1])
        fields = split_top_level(clean_initializer(body))
        if len(fields) < 4:
            raise ValueError(f"{name}: bad terrain initializer")
        quads = fields[2].strip()
        tris = fields[3].strip()
        terrains[name] = (
            None if quads == "NULL" else quads,
            None if tris == "NULL" else tris,
        )
    return terrains


def parse_terrain_info(data_text: str) -> list[str | None]:
    body = extract_initializer(data_text, "struct mb64_terrain_info mb64_terrain_info_list[]")
    out: list[str | None] = []
    terrain_re = re.compile(r"&(\w+)|NULL\s*\}")
    for raw_entry in split_top_level(clean_initializer(body)):
        if not raw_entry:
            continue
        match = terrain_re.search(raw_entry)
        out.append(match.group(1) if match and match.group(1) else None)
    return out


def emit_poly_array(name: str, entries: list[dict[str, object]], lines: list[str]) -> None:
    lines.append(f"static const mb64_shape_face_t {name}[] = {{")
    for entry in entries:
        vertices = entry["vertices"]
        vertex_count = len(vertices)
        padded_vertices = list(vertices)
        while len(padded_vertices) < 4:
            padded_vertices.append((0, 0, 0))
        verts = "{" + ", ".join(f"{{{x}, {y}, {z}}}" for x, y, z in padded_vertices[:4]) + "}"
        altuvs = entry["altuvs"]
        if altuvs is None:
            altuv_literal = "{{0, 0}, {0, 0}, {0, 0}, {0, 0}}"
            has_altuvs = "0"
        else:
            padded = list(altuvs)
            while len(padded) < 4:
                padded.append((0, 0))
            altuv_literal = "{" + ", ".join(f"{{{u}, {v}}}" for u, v in padded[:4]) + "}"
            has_altuvs = "1"
        lines.append(
            f"    {{{verts}, {entry['direction']}, {entry['faceshape']}, "
            f"{vertex_count}, {entry['growth_type']}, {has_altuvs}, {altuv_literal}}},"
        )
    lines.append("};")
    lines.append("")


def collect_terrain_faces(
    terrain: str,
    terrains: dict[str, tuple[str | None, str | None]],
    polys: dict[str, list[dict[str, object]]],
) -> list[dict[str, object]]:
    quads, tris = terrains[terrain]
    faces: list[dict[str, object]] = []
    if quads is not None:
        faces.extend(polys[quads])
    if tris is not None:
        faces.extend(polys[tris])
    return faces


def parse_boundary_quads(boundary_text: str, name: str) -> list[list[tuple[int, int]]]:
    body = extract_initializer(boundary_text, f"struct mb64_boundary_quad {name}[]")
    quads: list[list[tuple[int, int]]] = []
    for raw_entry in split_top_level(clean_initializer(body)):
        fields = split_top_level(unwrap_braces(raw_entry))
        vertex_fields = split_top_level(unwrap_braces(fields[0]))
        quad: list[tuple[int, int]] = []
        for vertex in vertex_fields:
            x, _y, z = parse_vertex(vertex)
            quad.append((x, z))
        quads.append(quad)
    return quads


def parse_boundary_table(data_text: str) -> list[str]:
    body = extract_initializer(data_text, "u8 mb64_boundary_table[]")
    entries: list[str] = []
    for raw in split_top_level(clean_initializer(body)):
        value = raw.strip()
        if value:
            entries.append(value)
    return entries


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-c", required=True, type=Path)
    parser.add_argument("--boundary-c", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    data_text = args.data_c.read_text()
    boundary_text = args.boundary_c.read_text()
    polys = parse_poly_entries(data_text)
    terrains = parse_terrains(data_text)
    terrain_info = parse_terrain_info(data_text)

    shape_entries: dict[str, list[dict[str, object]]] = {}
    shape_by_tile: dict[str, str] = {}
    collision_shape_entries: dict[str, list[dict[str, object]]] = {}
    collision_shape_by_tile: dict[str, str] = {}
    for tile_id, terrain in enumerate(terrain_info):
        if terrain is None:
            continue
        shape_name = f"s_shape_tile_{tile_id}"
        shape_entries[shape_name] = collect_terrain_faces(terrain, terrains, polys)
        shape_by_tile[str(tile_id)] = shape_name
        collision_shape_by_tile[str(tile_id)] = shape_name

    for tile_macro, terrain in SPECIAL_TERRAIN.items():
        shape_name = f"s_shape_{tile_macro.lower()}"
        shape_entries[shape_name] = collect_terrain_faces(terrain, terrains, polys)
        shape_by_tile[tile_macro] = shape_name
        collision_shape_by_tile[tile_macro] = shape_name

    for tile_macro, terrain in SPECIAL_COLLISION_TERRAIN.items():
        shape_name = f"s_collision_shape_{tile_macro.lower()}"
        collision_shape_entries[shape_name] = collect_terrain_faces(terrain, terrains, polys)
        collision_shape_by_tile[tile_macro] = shape_name

    for tile_macro, arrays in SPECIAL_FACE_ARRAYS.items():
        shape_name = f"s_shape_{tile_macro.lower()}"
        faces: list[dict[str, object]] = []
        for array in arrays:
            faces.extend(polys[array])
        shape_entries[shape_name] = faces
        shape_by_tile[tile_macro] = shape_name
        collision_shape_by_tile[tile_macro] = shape_name

    lines: list[str] = [
        "/* Generated by tools/gen_libmb64_mesh_data.py; do not edit. */",
        "static const uint8_t s_boundary_table[] = {",
    ]
    for entry in parse_boundary_table(data_text):
        lines.append(f"    {entry},")
    lines.extend(["};", ""])

    for symbol, source in (
        ("s_boundary_inner_floor", "floor_boundary"),
        ("s_boundary_outer_floor", "floor_edge_boundary"),
    ):
        lines.append(f"static const mb64_boundary_floor_quad_t {symbol}[] = {{")
        for quad in parse_boundary_quads(boundary_text, source):
            verts = ", ".join(f"{{{x:3d}, {z:3d}}}" for x, z in quad)
            lines.append(f"    {{{{{verts}}}}},")
        lines.extend(["};", ""])

    all_shape_entries = {**shape_entries, **collision_shape_entries}
    for name in sorted(all_shape_entries):
        emit_poly_array(name, all_shape_entries[name], lines)

    lines.append("static const mb64_shape_t s_shapes[32] = {")
    for tile, shape_name in sorted(shape_by_tile.items(), key=lambda item: item[0]):
        lines.append(f"    [{tile}] = {{{shape_name}, sizeof({shape_name}) / sizeof({shape_name}[0])}},")
    lines.extend(["};", ""])

    lines.append("static const mb64_shape_t s_collision_shapes[32] = {")
    for tile, shape_name in sorted(collision_shape_by_tile.items(), key=lambda item: item[0]):
        lines.append(f"    [{tile}] = {{{shape_name}, sizeof({shape_name}) / sizeof({shape_name}[0])}},")
    lines.extend(["};", ""])

    args.out.write_text("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
