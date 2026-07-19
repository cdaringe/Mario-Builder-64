#!/usr/bin/env python3
"""Export MB64-owned target actor assets into a CoopDx source tree."""

from __future__ import annotations

import argparse
import re
import shutil
from dataclasses import dataclass
from pathlib import Path


MANIFEST = Path(__file__).with_name("mb64_coopdx_actor_assets.tsv")
TEXT_SUFFIXES = {".c", ".h"}
ASSET_SUFFIXES = TEXT_SUFFIXES | {".png"}
FULL_ROOT_MODES = {
    "relocated-root",
    "coin-namespace",
    "tree-namespace",
    "checkerboard-platform-namespace",
}


@dataclass(frozen=True)
class Rule:
    kind: str
    destination: Path
    source: Path
    mode: str


MODE_OVERRIDES = {
    Path("actors/mb64_chicken/anims/anim_ArmatureAction.inc.c"): "coopdx-animation-abi",
    Path("actors/mb64_metalstar/model.inc.c"): "metalstar-coopdx-symbols",
    Path("actors/mb64_blaster/geo_header.h"): "const-gfx-header",
    Path("actors/mb64_onoffblock1/geo_header.h"): "const-gfx-header",
    Path("actors/mb64_onoffblock2/geo_header.h"): "const-gfx-header",
    Path("actors/mb64_onoffswitch/geo_header.h"): "const-gfx-header",
}


def parse_manifest() -> list[Rule]:
    rules: list[Rule] = []
    for line_number, raw_line in enumerate(MANIFEST.read_text().splitlines(), 1):
        if not raw_line or raw_line.startswith("#"):
            continue
        fields = raw_line.split("\t")
        if len(fields) != 4:
            raise SystemExit(f"{MANIFEST}:{line_number}: expected four tab-separated fields")
        kind, destination, source, mode = fields
        if kind not in {"actor-root", "texture-root", "extra-asset"}:
            raise SystemExit(f"{MANIFEST}:{line_number}: unsupported record type {kind}")
        destination_path = Path(destination)
        source_path = Path(source or destination)
        for value in (destination_path, source_path):
            if value.is_absolute() or ".." in value.parts:
                raise SystemExit(f"{MANIFEST}:{line_number}: path must stay inside its repository")
        rules.append(Rule(kind, destination_path, source_path, mode or "verbatim"))
    return rules


def clean_text(text: str) -> str:
    lines = text.splitlines()
    while lines and not lines[-1].strip():
        lines.pop()
    normalized: list[str] = []
    for line in lines:
        if not line.strip():
            normalized.append("")
            continue
        leading = line[: len(line) - len(line.lstrip())]
        normalized.append(leading.expandtabs(4) + line[len(leading) :].rstrip())
    return "\n".join(normalized) + "\n"


def adapt_text(text: str, source: Path, destination: Path, mode: str) -> str:
    mode = MODE_OVERRIDES.get(destination, mode)
    if mode == "coopdx-animation-abi":
        text = re.sub(r"\bstatic const s16 ([A-Za-z0-9_]+_values\[\])", r"static const u16 \1", text)
        text = re.sub(
            r"\bstatic const struct Animation ([A-Za-z0-9_]+) = \{",
            r"static const struct Animation \1[] = {",
            text,
        )
    elif mode == "const-gfx-header":
        text = text.replace("extern const Gfx ", "extern Gfx ")
    elif mode == "metalstar-coopdx-symbols":
        text = re.sub(
            r"const Gfx metalstar_seg3_sub_dl_(?:body|eyes)\[\] = \{.*?\n\};\n",
            "",
            text,
            flags=re.DOTALL,
        )
        replacements = {
            "actors/star/custom_metalstartex.rgba16.inc.c": "actors/metalstar/custom_metalstartex.rgba16.inc.c",
            "actors/star/star_eye.rgba16.inc.c": "actors/metalstar/star_eye.rgba16.inc.c",
            "star_seg3_vertex_body": "star_seg3_vertex_0302B6F0",
            "star_seg3_sub_dl_body": "star_seg3_dl_0302B7B0",
            "metalstar_seg3_dl_0302B7B0": "metalstar_seg3_sub_dl_body",
            "star_seg3_vertex_eyes": "star_seg3_vertex_0302B920",
            "star_seg3_sub_dl_eyes": "star_seg3_dl_0302B9C0",
            "metalstar_seg3_dl_0302B9C0": "metalstar_seg3_sub_dl_eyes",
        }
        for old, new in replacements.items():
            text = text.replace(old, new)
    elif mode == "checkerboard-platform-namespace":
        text = re.sub(
            r"\bcheckerboard_platform_([A-Za-z0-9_]+)\b",
            r"mb64_checkerboard_platform_\1",
            text,
        )
    elif mode == "coin-namespace":
        text = re.sub(r"\bcoin_([A-Za-z0-9_]+)\b", r"mb64_coin_\1", text)
        text = re.sub(
            r"\b(yellow|blue|red|silver|green)_coin(_no_shadow)?_geo\b",
            lambda match: f"mb64_{match.group(0)}",
            text,
        )
        text = text.replace("actors/coin/mb64_coin_", "actors/coin/coin_")
    elif mode == "tree-namespace":
        text = re.sub(r"\btree_seg3_([A-Za-z0-9_]+)\b", r"mb64_tree_seg3_\1", text)
        text = re.sub(
            r"\b(bubbly|farm|spiky|snow|palm|dead)_tree_geo\b",
            lambda match: f"mb64_{match.group(0)}",
            text,
        )
    elif mode not in {"verbatim", "relocated-root", "renamed-root", "strip-trailing-blank-lines"}:
        raise SystemExit(f"unsupported adaptation mode {mode} for {destination}")

    if source.parent != destination.parent:
        text = text.replace(
            f'include "{source.parent.as_posix()}/',
            f'include "{destination.parent.as_posix()}/',
        )
    if mode == "checkerboard-platform-namespace":
        text = text.replace(
            "actors/mb64_checkerboard_platform/mb64_checkerboard_platform_",
            "actors/mb64_checkerboard_platform/checkerboard_platform_",
        )
    return clean_text(text)


def export_file(mb64_root: Path, coop_root: Path, source: Path, destination: Path, mode: str) -> None:
    source_path = mb64_root / source
    destination_path = coop_root / destination
    destination_path.parent.mkdir(parents=True, exist_ok=True)
    if source_path.suffix in TEXT_SUFFIXES:
        output = adapt_text(source_path.read_text(), source, destination, mode)
        if not destination_path.exists() or destination_path.read_text() != output:
            destination_path.write_text(output)
        return
    if not destination_path.exists() or source_path.read_bytes() != destination_path.read_bytes():
        shutil.copyfile(source_path, destination_path)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--coop-root", required=True, type=Path)
    args = parser.parse_args()
    mb64_root = MANIFEST.parent.parent
    exported = 0
    for rule in parse_manifest():
        if rule.kind == "extra-asset":
            export_file(mb64_root, args.coop_root, rule.source, rule.destination, rule.mode)
            exported += 1
            continue
        source_root = mb64_root / rule.source
        if not source_root.is_dir():
            raise SystemExit(f"missing MB64 actor root: {source_root}")
        suffixes = {".png"} if rule.kind == "texture-root" or rule.mode == "renamed-root" else ASSET_SUFFIXES
        for source_path in sorted(path for path in source_root.rglob("*") if path.suffix in suffixes):
            relative = source_path.relative_to(source_root)
            if (rule.kind == "actor-root" and rule.mode not in FULL_ROOT_MODES
                    and not (args.coop_root / rule.destination / relative).exists()):
                continue
            export_file(
                mb64_root,
                args.coop_root,
                rule.source / relative,
                rule.destination / relative,
                rule.mode,
            )
            exported += 1
    print(f"exported {exported} normalized MB64 actor assets to {args.coop_root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
