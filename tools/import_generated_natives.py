#!/usr/bin/env python3
"""Import YimMenu-style generated native headers into Direct_Menu.

Usage:
    python tools/import_generated_natives.py Natives.hpp Crossmap.hpp

The script writes project-compatible headers into src/natives/generated and
creates a bootstrap helper that binds the generated crossmap to the existing
smf::natives::NativeInvoker instance.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def import_natives(source: Path, destination: Path) -> None:
    text = source.read_text(encoding="utf-8")
    text = text.replace(
        '#include "types/script/types.hpp"',
        '#include "types/script/types.hpp"',
    )
    destination.write_text(text, encoding="utf-8")


def import_crossmap(source: Path, destination: Path) -> int:
    text = source.read_text(encoding="utf-8")
    match = re.search(
        r"g_Crossmap\s*=\s*\{(?P<body>.*?)\};",
        text,
        flags=re.DOTALL,
    )
    if match is None:
        raise RuntimeError("Could not find g_Crossmap initializer.")

    hashes = re.findall(r"0x[0-9A-Fa-f]+", match.group("body"))
    if not hashes:
        raise RuntimeError("The crossmap contains no hashes.")

    lines = [
        "#pragma once",
        "",
        '#include "natives/NativeInvoker.hpp"',
        "",
        "#include <array>",
        "",
        "namespace smf::natives::generated {",
        "",
        f"inline constexpr std::array<NativeHash, {len(hashes)}> Crossmap{{{{",
    ]

    for offset in range(0, len(hashes), 6):
        chunk = ", ".join(f"{value}ULL" for value in hashes[offset : offset + 6])
        lines.append(f"    {chunk},")

    lines.extend(
        [
            "}};",
            "",
            "inline void Bind(NativeInvoker& invoker) noexcept {",
            "    BindGeneratedNativeRuntime(invoker, Crossmap);",
            "}",
            "",
            "} // namespace smf::natives::generated",
            "",
        ]
    )
    destination.write_text("\n".join(lines), encoding="utf-8")
    return len(hashes)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("natives", type=Path)
    parser.add_argument("crossmap", type=Path)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("src/natives/generated"),
    )
    arguments = parser.parse_args()

    output = arguments.output
    output.mkdir(parents=True, exist_ok=True)

    import_natives(arguments.natives, output / "Natives.hpp")
    count = import_crossmap(arguments.crossmap, output / "Crossmap.hpp")

    print(f"Imported {count} generated natives into {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
