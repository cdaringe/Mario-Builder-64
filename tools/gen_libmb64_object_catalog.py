#!/usr/bin/env python3
"""Generate libmb64's portable object catalog from the editor source table."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


TYPE_ALIASES = {
    "OBJECT_TYPE_SPAWN": "MB64_OBJECT_TYPE_MARIO_SPAWN",
    "OBJECT_TYPE_TIMED_BLOCK": "MB64_OBJECT_TYPE_TIMEDBLOCK",
}

FLAG_VALUES = {
    "OBJ_TYPE_BILLBOARD": 1 << 0,
    "OBJ_TYPE_TRAJECTORY": 1 << 1,
    "OBJ_TYPE_STAR": 1 << 2,
    "OBJ_TYPE_HAS_DIALOG": 1 << 3,
    "OBJ_TYPE_IMBUABLE": 1 << 4,
    "OBJ_TYPE_IMBUABLE_COINS": 1 << 5,
    "OBJ_TYPE_IMBUABLE_TRIGGER": 1 << 6,
}

OCCUPANCY_VALUES = {
    "OBJ_OCCUPY_OUTER": 1 << 0,
    "OBJ_OCCUPY_INNER": 1 << 1,
    "OBJ_OCCUPY_FULL": (1 << 0) | (1 << 1),
}


def split_fields(text: str) -> list[str]:
    fields: list[str] = []
    current: list[str] = []
    depth = 0
    in_string = False
    escaped = False
    for char in text:
        if in_string:
            current.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif char == '"':
            in_string = True
            current.append(char)
        elif char in "([{":
            depth += 1
            current.append(char)
        elif char in ")]}":
            depth -= 1
            current.append(char)
        elif char == "," and depth == 0:
            fields.append("".join(current).strip())
            current = []
        else:
            current.append(char)
    fields.append("".join(current).strip())
    return fields


def token_string(value: str) -> str:
    return "" if value in {"", "NULL"} else value


def parse_or_expression(value: str, values: dict[str, int]) -> int:
    compact = value.replace(" ", "")
    if compact == "0":
        return 0
    result = 0
    for token in compact.split("|"):
        if token not in values:
            raise ValueError(f"unsupported catalog flag token: {token}")
        result |= values[token]
    return result


def parse_y_offset(value: str) -> float:
    compact = value.replace(" ", "")
    if compact == "TILE_SIZE/2":
        return 128.0
    if compact == "TILE_SIZE":
        return 256.0
    return float(compact.rstrip("f"))


def public_types(path: Path) -> list[str]:
    source = path.read_text()
    block = re.search(r"typedef enum \{(.*?)MB64_OBJECT_TYPE_COUNT,\s*\}", source, re.S)
    if block is None:
        raise ValueError("could not find mb64_object_type_t enum")
    return re.findall(r"\b(MB64_OBJECT_TYPE_[A-Z0-9_]+)\b", block.group(1))


def source_rows(path: Path) -> dict[str, list[str]]:
    rows: dict[str, list[str]] = {}
    pattern = re.compile(r"/\*\s*(OBJECT_TYPE_[A-Z0-9_]+)\s*\*/\s*\{(.*)\},")
    for line in path.read_text().splitlines():
        match = pattern.search(line)
        if match is None:
            continue
        source_type, initializer = match.groups()
        public_type = TYPE_ALIASES.get(
            source_type, "MB64_" + source_type
        )
        fields = split_fields(initializer)
        if len(fields) != 13:
            raise ValueError(
                f"{source_type}: expected 13 object fields, found {len(fields)}"
            )
        rows[public_type] = fields
    return rows


def generate(data_c: Path, object_types_h: Path) -> str:
    types = public_types(object_types_h)
    rows = source_rows(data_c)
    missing = [type_name for type_name in types if type_name not in rows]
    extra = sorted(set(rows) - set(types))
    if missing or extra:
        raise ValueError(f"catalog mismatch: missing={missing} extra={extra}")

    output = [
        "/* Generated from src/mb64/data.c. Do not edit. */",
        "static const mb64_object_spec_t sMb64ObjectCatalog[MB64_OBJECT_TYPE_COUNT] = {",
    ]
    for type_name in types:
        fields = rows[type_name]
        name = json.loads(fields[0])
        flags = parse_or_expression(fields[5], FLAG_VALUES)
        occupancy = parse_or_expression(fields[6], OCCUPANCY_VALUES)
        output.extend([
            f"    [{type_name}] = {{",
            f"        {type_name}, {json.dumps(name)},",
            f"        {json.dumps(token_string(fields[2]))},",
            f"        {json.dumps(token_string(fields[4]))},",
            f"        {json.dumps(token_string(fields[10]))},",
            f"        {json.dumps(token_string(fields[11]))},",
            f"        {json.dumps(token_string(fields[12]))},",
            f"        {parse_y_offset(fields[3]):.6f}f, {float(fields[9].rstrip('f')):.6f}f,",
            f"        {flags}, {occupancy}, {int(fields[7])}, {int(fields[8])}",
            "    },",
        ])
    output.extend(["};", ""])
    return "\n".join(output)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--data-c", type=Path, required=True)
    parser.add_argument("--object-types-h", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    args.out.write_text(generate(args.data_c, args.object_types_h))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
