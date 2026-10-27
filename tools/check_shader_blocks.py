#!/usr/bin/env python3
"""Verifică faptul că blocul de uniforme `Frame` e declarat identic în toate shaderele.

Pe WebGL (GLSL ES) un program nu se leagă dacă vertex și fragment shader declară blocul diferit („Field numbers of
uniform 'frame' differ”), iar viewport-ul rămâne gol. Pe Metal / Vulkan / D3D diferența trece neobservată.
Cu --fix, copiază blocul din mesh.frag (cel complet) în celelalte shadere.
"""
import pathlib
import re
import sys

SHADERS = pathlib.Path(__file__).resolve().parent.parent / "render" / "shaders"
BLOCK = re.compile(r"layout\(std140, binding = 0\) uniform Frame \{.*?\} frame;", re.S)


def fields(block: str) -> list[str]:
    body = block.split("{", 1)[1].rsplit("}", 1)[0]
    lines = [re.sub(r"//.*", "", line).strip() for line in body.splitlines()]
    return [line for line in lines if line]


def main() -> int:
    reference_text = BLOCK.search((SHADERS / "mesh.frag").read_text()).group(0)
    reference = fields(reference_text)
    fix = "--fix" in sys.argv
    bad = []
    for path in sorted(SHADERS.glob("*.*")):
        text = path.read_text()
        match = BLOCK.search(text)
        if match is None or fields(match.group(0)) == reference:
            continue
        if fix:
            path.write_text(text[: match.start()] + reference_text + text[match.end() :])
            print(f"actualizat: {path.name}")
        else:
            bad.append(path.name)
    if bad:
        print("Blocul Frame diferă de cel din mesh.frag în: " + ", ".join(bad))
        print("Rulează: python3 tools/check_shader_blocks.py --fix")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
