#!/usr/bin/env python3
"""Generate libmb64 theme/material lookup data from MB64 game sources."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

MATERIAL_ALIASES = {
    "MB64_MAT_C_OUTSIDE_BRICK": "MB64_MAT_C_OUTSIDEBRICK",
}

SURFACE_ALIASES = {
    "SURFACE_BURNING_BUBBLES": 1,
    "SURFACE_BURNING_ICE": 2,
    "SURFACE_HANGABLE_MESH": 5,
    "SURFACE_GRASS": 124,
    "SURFACE_SNOW": 125,
    "SURFACE_SAND": 126,
    "SURFACE_WOOD": 127,
    "SURFACE_CREAKWOOD": 127,
    "SURFACE_CRYSTAL": 128,
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


def parse_surfaces(path: Path) -> dict[str, int]:
    text = path.read_text()
    defines: dict[str, int] = {}
    for raw in text.splitlines():
        line = strip_line_comment(raw).strip()
        match = re.match(r"#define\s+(SURFACE_[A-Z0-9_]+)\s+(0x[0-9A-Fa-f]+|-?\d+)", line)
        if match:
            defines[match.group(1)] = int(match.group(2), 0)

    values: dict[str, int] = {}
    if "enum SurfaceTypes" in text:
        body = extract_initializer(text, "enum SurfaceTypes")
        current = -1
        for raw in body.splitlines():
            comment_value = re.search(r"//\s*(0x[0-9A-Fa-f]+|-?\d+)", raw)
            line = strip_line_comment(raw).strip().rstrip(",")
            if not line or line.startswith("#"):
                continue
            match = re.match(r"([A-Z0-9_]+)(?:\s*=\s*(0x[0-9A-Fa-f]+|-?\d+))?$", line)
            if not match:
                continue
            name, value = match.groups()
            if value is not None:
                current = int(value, 0)
            elif comment_value is not None:
                current = int(comment_value.group(1), 0)
            else:
                current = current + 1
            values[name] = current
    values.update(defines)
    values.update({name: value for name, value in SURFACE_ALIASES.items() if name not in values})
    if not values:
        raise ValueError(f"no surfaces parsed from {path}")
    return values


def parse_material_defs(data_text: str, surfaces: dict[str, int]) -> tuple[list[tuple[str, int]], list[tuple[str, str]], list[tuple[str, str]]]:
    body = extract_initializer(data_text, "struct mb64_material mb64_mat_table[]")
    surface_entries: list[tuple[str, int]] = []
    type_entries: list[tuple[str, str]] = []
    vertical_entries: list[tuple[str, str]] = []
    entry_re = re.compile(
        r"{[^,]+,\s*(MAT_[A-Z0-9_]+)\s*,\s*(TRUE|FALSE|0|1)\s*,\s*(SURFACE_[A-Z0-9_]+)\s*,.*//\s*(MB64_MAT_[A-Z0-9_]+)"
    )
    for raw in body.splitlines():
        match = entry_re.search(raw)
        if not match:
            continue
        material_type, vertical, surface_name, mat_name = match.groups()
        mat_name = MATERIAL_ALIASES.get(mat_name, mat_name)
        if surface_name not in surfaces:
            raise ValueError(f"unknown surface {surface_name} for {mat_name}")
        surface_entries.append((mat_name, surfaces[surface_name]))
        type_entries.append((mat_name, material_type))
        vertical_entries.append((mat_name, "1" if vertical in ("TRUE", "1") else "0"))
    if not surface_entries:
        raise ValueError("no material definitions parsed")
    return surface_entries, type_entries, vertical_entries


def parse_themes(data_text: str) -> tuple[list[list[tuple[str, str]]], list[tuple[str, str, str, str]]]:
    body = extract_initializer(data_text, "struct mb64_theme mb64_theme_table[]")
    themes: list[list[tuple[str, str]]] = []
    specials: list[tuple[str, str, str, str]] = []
    current: list[tuple[str, str]] = []
    material_re = re.compile(r"{\s*(MB64_MAT_[A-Z0-9_]+|0)\s*,\s*(MB64_MAT_[A-Z0-9_]+|0)\s*,")
    special_re = re.compile(
        r"^\s*(MB64_FENCE_[A-Z0-9_]+|0)\s*,\s*"
        r"(MB64_MAT_[A-Z0-9_]+|0)\s*,\s*"
        r"(MB64_BAR_[A-Z0-9_]+|0)\s*,\s*"
        r"(MB64_WATER_[A-Z0-9_]+|0)"
    )
    for raw in body.splitlines():
        line = strip_line_comment(raw)
        material = material_re.search(line)
        if material:
            current.append((material.group(1), material.group(2)))
            continue
        special = special_re.search(line)
        if special:
            if len(current) > 10:
                raise ValueError(f"theme has {len(current)} material slots")
            while len(current) < 10:
                current.append(("0", "0"))
            themes.append(current)
            specials.append((special.group(1), special.group(2), special.group(3), special.group(4)))
            current = []
    if len(themes) == 0:
        raise ValueError("no themes parsed")
    return themes, specials


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-c", required=True, type=Path)
    parser.add_argument("--surface-terrains", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    data_text = args.data_c.read_text()
    surfaces = parse_surfaces(args.surface_terrains)
    themes, specials = parse_themes(data_text)
    material_surfaces, material_types, material_verticals = parse_material_defs(data_text, surfaces)

    lines: list[str] = [
        "/* Generated by tools/gen_libmb64_theme_data.py; do not edit. */",
        "static const mb64_material_def_t s_theme_materials[][MB64_MATERIAL_SLOT_COUNT] = {",
    ]
    for theme in themes:
        slots = ", ".join(f"{{{side}, {top}}}" for side, top in theme)
        lines.append(f"    {{{slots}}},")
    lines.extend([
        "};",
        "",
        "static const mb64_theme_special_t s_theme_specials[] = {",
    ])
    for fence, pole, bars, water in specials:
        lines.append(f"    {{{fence}, {pole}, {bars}, {water}}},")
    lines.extend([
        "};",
        "",
        "static const int16_t s_material_surfaces[] = {",
    ])
    for mat_name, surface in material_surfaces:
        if mat_name.startswith("MB64_MAT_"):
            lines.append(f"    [{mat_name}] = {surface},")
    lines.extend([
        "};",
        "",
        "static const uint8_t s_material_types[] = {",
    ])
    for mat_name, material_type in material_types:
        if mat_name.startswith("MB64_MAT_"):
            lines.append(f"    [{mat_name}] = {material_type},")
    lines.extend([
        "};",
        "",
        "static const uint8_t s_material_verticals[] = {",
    ])
    for mat_name, vertical in material_verticals:
        if mat_name.startswith("MB64_MAT_"):
            lines.append(f"    [{mat_name}] = {vertical},")
    lines.extend([
        "};",
        "",
    ])
    args.out.write_text("\n".join(lines))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
