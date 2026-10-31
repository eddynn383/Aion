#!/usr/bin/env python3
"""Verifică traducerile (ADR 0043, pasul 12): fiecare cheie qsTrId folosită în QML are text românesc
(translations/strings.json) și traducere engleză (translations/en.json), iar fișierele .ts sunt la zi.
Iese cu 1 la prima problemă (pentru CI)."""

import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TRANSLATIONS = ROOT / "translations"


def main() -> int:
    strings = json.loads((TRANSLATIONS / "strings.json").read_text(encoding="utf-8"))
    english = json.loads((TRANSLATIONS / "en.json").read_text(encoding="utf-8"))
    used = set()
    for f in list((ROOT / "app/qml").rglob("*.qml")) + list((ROOT / "app/qml").rglob("*.js")):
        used |= set(re.findall(r'qsTrId\("([^"]+)"\)', f.read_text(encoding="utf-8")))
    problems = [f"cheie fără text românesc: {k}" for k in sorted(used - strings.keys())]
    problems += [f"cheie fără traducere engleză: {k}" for k in sorted(used - english.keys())]
    # Fișierele .ts se refac din JSON; dacă diferă după refacere, cineva a uitat să ruleze scriptul.
    before = {p: p.read_text(encoding="utf-8") for p in TRANSLATIONS.glob("aion_*.ts")}
    subprocess.run([sys.executable, str(ROOT / "tools/extract_strings.py"), "--write-ts"], check=True,
                   stdout=subprocess.DEVNULL)
    problems += [f"{p.name} nu e la zi (rulează tools/extract_strings.py --write-ts)" for p, text in before.items()
                 if p.read_text(encoding="utf-8") != text]
    for p in problems:
        print(p)
    print(f"{len(used)} chei folosite, {len(problems)} probleme")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
