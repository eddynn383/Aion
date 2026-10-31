#!/usr/bin/env python3
"""Textele vizibile din QML devin chei de traducere (qsTrId), iar fișierele .ts se refac (ADR 0043, pasul 12).

Folosire:
    tools/extract_strings.py app/qml/overlays/QuantitiesDialog.qml ...   # transformă fișierele date
    tools/extract_strings.py --write-ts                                   # doar refacerea fișierelor .ts

Cheia = `<fișierul fără extensie>.<textul românesc fără diacritice, cu „_”>`, scurtată la 60 de caractere, cu sufix
numeric la ciocnire. `translations/strings.json` ține cheile cu textul românesc (sursa de adevăr); traducerile sunt în
`translations/<limbă>.json` (cheie → text). Din ele se scriu `translations/aion_<limbă>.ts`; o cheie fără traducere
apare `type="unfinished"` și cade la rulare pe engleză, apoi pe română.

Ce se consideră text vizibil: un literal care conține un spațiu, o literă cu diacritice sau începe cu majusculă, în
afara comentariilor, a comparațiilor (`=== "…"`), a `console.log` și a proprietăților tehnice (font, scurtături,
surse, chei).
"""

import json
import re
import sys
import unicodedata
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parent.parent
TRANSLATIONS = ROOT / "translations"
STRINGS = TRANSLATIONS / "strings.json"
LANGUAGES = ["ro", "en", "de", "pl", "it", "fr", "es", "pt"]

TECHNICAL = re.compile(
    r"(font\.family|shortcut|defaultShortcut|alias|key|source|objectName|iconName|icon|category|id|format|"
    r"sequence|sequences|kind|variant)\s*:\s*$")
COMPARISON = re.compile(r"(===|!==|==|!=|case)\s*$")


def ascii_words(text: str) -> str:
    plain = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode()
    words = re.findall(r"[A-Za-z0-9]+", plain.lower())
    return "_".join(words) or "text"


def is_visible(text: str) -> bool:
    if not any(c.isalpha() for c in text) or text.startswith(("qrc:", "image:", "#", "http")):
        return False
    if re.fullmatch(r"[A-Za-z]+\+[A-Za-z0-9+]+", text):  # scurtături („Ctrl+G”)
        return False
    if re.fullmatch(r"Space|Return|Enter|Escape|Esc|Tab|Backspace|Delete|Left|Right|Up|Down|Home|End|PgUp|PgDown|"
                    r"F[0-9]{1,2}", text):  # numele tastelor (secvențe de scurtături)
        return False
    if re.fullmatch(r"[a-z0-9_.:\-/]+", text):  # id-uri, chei, nume de iconițe
        return False
    if re.search(r"\d+px\b|\.qml$|\.js$", text):  # fonturi pentru Canvas, fișiere încărcate dinamic
        return False
    return " " in text or any(ord(c) > 127 for c in text) or text[0].isupper()


class Keys:
    def __init__(self):
        self.strings: dict[str, str] = json.loads(STRINGS.read_text(encoding="utf-8")) if STRINGS.exists() else {}

    def key_for(self, stem: str, text: str) -> str:
        for k, v in self.strings.items():
            if v == text and k.startswith(stem + "."):
                return k
        base = (stem + "." + ascii_words(text))[:60]
        key = base
        n = 2
        while key in self.strings and self.strings[key] != text:
            key = f"{base[:57]}_{n}"
            n += 1
        self.strings[key] = text
        return key

    def save(self):
        TRANSLATIONS.mkdir(exist_ok=True)
        STRINGS.write_text(json.dumps(self.strings, ensure_ascii=False, indent=1, sort_keys=True) + "\n",
                           encoding="utf-8")


def literal_spans(source: str):
    """Pozițiile (început, sfârșit) ale literalelor cu ghilimele duble, citind textul ca un parser: comentariile și
    literalele cu ghilimele simple sau `…` se sar, iar „\\"” nu închide un literal."""
    spans = []
    i = 0
    n = len(source)
    while i < n:
        c = source[i]
        if source.startswith("//", i):
            i = source.find("\n", i)
            i = n if i < 0 else i
            continue
        if source.startswith("/*", i):
            i = source.find("*/", i + 2)
            i = n if i < 0 else i + 2
            continue
        if c in "\"'`":
            start = i
            i += 1
            while i < n and source[i] != c:
                if source[i] == "\\":
                    i += 1
                elif c != "`" and source[i] == "\n":
                    break  # literal neterminat pe rând: nu e un literal
                i += 1
            if c == '"' and i < n and source[i] == '"':
                spans.append((start, i + 1))
            i += 1
            continue
        i += 1
    return spans


def join_literals(source: str) -> str:
    """Două literale lipite cu „+” (o propoziție ruptă pe două rânduri) devin unul, ca să aibă o singură cheie."""
    while True:
        spans = literal_spans(source)
        for (a0, a1), (b0, b1) in zip(spans, spans[1:]):
            if re.fullmatch(r"\s*\+\s*", source[a1:b0]):
                source = source[:a1 - 1] + source[b0 + 1:]
                break
        else:
            return source


def convert(path: Path, keys: Keys) -> int:
    stem = path.stem
    source = join_literals(path.read_text(encoding="utf-8"))
    pieces = []
    last = 0
    count = 0
    for start, end in literal_spans(source):
        line_start = source.rfind("\n", 0, start) + 1
        line_end = source.find("\n", end)
        line = source[line_start:len(source) if line_end < 0 else line_end]
        before = source[line_start:start]
        text = source[start + 1:end - 1].replace('\\"', '"')
        if (line.lstrip().startswith(("import ", ".import", ".pragma")) or "console." in line or "Qt.Key_" in line
                or re.search(r"sequences?\s*:\s*\[[^\]]*$", before)
                or not is_visible(text) or TECHNICAL.search(before) or COMPARISON.search(before)
                or re.search(r"(startsWith|endsWith|includes|indexOf|setStandardView|runCommand|command)\($",
                             before.rstrip())):
            continue
        key = keys.key_for(stem, text)
        pieces.append(source[last:start] + f'qsTrId("{key}")')
        last = end
        count += 1
    path.write_text("".join(pieces) + source[last:], encoding="utf-8")
    return count


CPP_SOURCES = ["core/src", "render/src"]
CPP_SKIP = ("MeshValidate.cpp", "MaterialLibrary.cpp", "AionFile.cpp", "ObjIo.cpp", "Gltf", "Recovery")
CORE = TRANSLATIONS / "core.json"


def collect_cpp() -> dict:
    """Textele vizibile din C++ (nucleul și documentul): sursa e textul românesc însuși; la rulare, documentul le
    traduce cu QCoreApplication::translate("aion", text). Numele materialelor incluse nu se traduc (se trimit înapoi
    după nume)."""
    from pathlib import Path as P
    materials = set(re.findall(r'entry\("([^"]+)"', (ROOT / "core/src/model/MaterialLibrary.cpp").read_text(encoding="utf-8")))
    found = {}
    for base in CPP_SOURCES:
        for f in sorted((ROOT / base).rglob("*.cpp")) + sorted((ROOT / base).rglob("*.h")):
            if any(x in f.name for x in CPP_SKIP):
                continue
            src = f.read_text(encoding="utf-8")
            for start, end in literal_spans(src):
                line_start = src.rfind("\n", 0, start) + 1
                line_end = src.find("\n", end)
                line = src[line_start:len(src) if line_end < 0 else line_end]
                if line.lstrip().startswith("#") or re.search(r"qInfo|qWarning|qDebug|console|assert\(|QLatin1String|"
                                                               r"json\b|\.value\(|\.contains\(|find\(|startsWith", line):
                    continue
                text = src[start + 1:end - 1]
                if "\\" in text.replace("\\n", "") or not is_visible(text) or text in materials:
                    continue
                text = text.replace("\\n", "\n")
                found.setdefault(text, f.name)
    for unit in ("buc.",):  # unitățile scurte (litere mici) nu trec de is_visible
        found.setdefault(unit, "Parametric.cpp")
    return found


def write_ts(keys: Keys):
    for lang in LANGUAGES:
        table_path = TRANSLATIONS / f"{lang}.json"
        table = json.loads(table_path.read_text(encoding="utf-8")) if table_path.exists() else {}
        lines = ['<?xml version="1.0" encoding="utf-8"?>', "<!DOCTYPE TS>",
                 f'<TS version="2.1" language="{lang}">', "<context>", "    <name></name>"]
        for key, ro in sorted(keys.strings.items()):
            text = ro if lang == "ro" else table.get(key, "")
            state = "" if text else ' type="unfinished"'
            lines += [f'    <message id="{escape(key)}">', f"        <source>{escape(ro)}</source>",
                      f"        <translation{state}>{escape(text)}</translation>", "    </message>"]
        lines += ["</context>"]
        # Textele din C++ (context „aion”): sursa e textul românesc.
        core = json.loads(CORE.read_text(encoding="utf-8")) if CORE.exists() else {}
        core_table_path = TRANSLATIONS / f"core_{lang}.json"
        core_table = json.loads(core_table_path.read_text(encoding="utf-8")) if core_table_path.exists() else {}
        lines += ["<context>", "    <name>aion</name>"]
        for ro in sorted(core):
            text = ro if lang == "ro" else core_table.get(ro, "")
            state = "" if text else ' type="unfinished"'
            lines += ["    <message>", f"        <source>{escape(ro)}</source>",
                      f"        <translation{state}>{escape(text)}</translation>", "    </message>"]
        lines += ["</context>", "</TS>", ""]
        (TRANSLATIONS / f"aion_{lang}.ts").write_text("\n".join(lines), encoding="utf-8")


def main(argv):
    keys = Keys()
    if "--cpp" in argv:
        found = collect_cpp()
        CORE.write_text(json.dumps(dict(sorted(found.items())), ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
        print(f"C++: {len(found)} texte")
    files = [a for a in argv if not a.startswith("--")]
    for f in files:
        n = convert(ROOT / f, keys)
        print(f"{f}: {n} texte")
    keys.save()
    write_ts(keys)


if __name__ == "__main__":
    main(sys.argv[1:])
