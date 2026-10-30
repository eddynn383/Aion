# Scripturi și plugin-uri

Aion rulează JavaScript (motorul QJSEngine din Qt, la fel pe macOS, Windows și Web). Consola: palette (⌘K) →
„Consola de scripturi” sau ⌘⌥J. ⌘↵ rulează.

- O rulare e **un singur pas de undo** (⌘Z anulează tot ce a făcut scriptul).
- O **eroare** anulează tot ce făcuse scriptul până atunci.
- Un script care durează peste **15 s** se oprește (și se anulează).
- Unitățile sunt **mm**; coordonatele sunt în contextul editat (în rădăcină = lumea; în interiorul unui grup = ale lui).
- Id-urile sunt ale obiectelor (instanțelor) din context.

## API: obiectul global `aion`

| Funcție | Ce face |
|---|---|
| `box(x, y, z, lățime, adâncime, înălțime, nume?)` | Cutie ca obiect, cu colțul minim în (x, y, z). Întoarce id-ul. |
| `cylinder(x, y, z, diametru, înălțime, nume?)`, `cone(…)` | Cilindru / con cu cutia începând în (x, y, z). |
| `sphere(x, y, z, diametru, nume?)` | Sferă. |
| `object(generator, parametri, x, y, z)` | Obiect dinamic: `"door"`, `"window"`, `"table"`, `"shelf"`, `"cabinet"`, `"stairs"` cu parametrii lor (de exemplu `{ width: 900, height: 2100 }`); (x, y, z) = colțul minim, ca la `box`. |
| `move(id, dx, dy, dz)` | Mută obiectul. |
| `rotate(id, grade, axa = "z", pivot?)` | Rotește în jurul pivotului obiectului (implicit centrul) sau al punctului `[x, y, z]`. |
| `place(id, { x, y, z, rx, ry, rz, sx, sy, sz, width, depth, height })` | Plasarea ca în Inspector (oricare chei); x, y, z = poziția pivotului. |
| `material(id, nume sau culoare)` | Material din model / bibliotecă după nume („Lemn de stejar”) sau o culoare (`"#c0392b"`). |
| `remove(id)` | Șterge obiectul. |
| `line([x, y, z], [x, y, z])` | O muchie liberă (geometrie, ca unealta Line). |
| `face([[x, y, z], …])` | O față plană din puncte (ca un poligon desenat). |
| `objects()` | Obiectele din context: `{ id, name, x, y, z, width, depth, height }`. |
| `cutList(options)` | Lista de debitare (doar citire): `{ parts, sheets, bands, hardware, hiddenPieces }`; `options` ca în fereastră (`excluded`, `cutDimensions`, `sheetLength`…). |
| `board(type, thickness, length, width, x, y, z)` | O placă parametrică (`"pal"`, `"mdf"`, `"hdf"`, `"plywood"`, `"solid"`) cu colțul minim în (x, y, z); întoarce id-ul (0 la un tip necunoscut). |
| `setBoardValue(id, key, value)` | Schimbă `"length"`, `"width"`, `"thickness"` sau `"type"` pe placa `id`. |
| `selection()` / `select([id…])` | Obiectele selectate / selectează. |
| `command(id)` | O comandă din palette (de exemplu `"view.top"`, `"view.extents"`). |
| `message(text)` | Un mesaj scurt (toast). |
| `log(valoare)`, `print(…)` | Text în rezultatul consolei. |

## Plugin-uri

Un plugin e un dosar în dosarul plugin-urilor (consola → 📁; pe macOS
`~/Library/Application Support/Aion/Aion/Plugins`):

```
gard/
  manifest.json
  main.js
```

`manifest.json`:

```json
{
    "name": "Gard",
    "version": "1.0",
    "description": "…",
    "commands": [ { "title": "Gard de 6 m", "function": "gard6" } ]
}
```

Fiecare comandă apare în palette (categoria „Plugin-uri”) și în consolă; la rulare se încarcă `main.js` și se apelează
funcția. Un exemplu complet: `examples/plugins/gard/`.
