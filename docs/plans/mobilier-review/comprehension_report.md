# Stage 6 – Comprehension Test

* **Target:** `docs/plans/mobilier-pe-corp.md` (FeatureRange 1–569, DetailedRange 570–1036)
* **Reviewer:** egm-comprehension (fresh context, no document standard)
* **Date:** 2026-10-30

The reviewer read the whole target, `CLAUDE.md`, ADRs 0017/0025/0038/0039, `PLAN.md` §3 and §4.4, and the linked
`concepts_brief.md`. Every section could be paraphrased. One finding is a Blocker: a stated acceptance case cannot be
met by the mechanism the document describes.

### Paraphrase of the feature spec (lines 1–569)

**What this proposes**

Aion is a one-person SketchUp-like modeller. This document turns it into a tool for carcass furniture (cabinets,
kitchen units, shelving). Panels become parametric objects: each panel's definition carries part data (type PAL, MDF,
HDF…; nominal thickness; thickness and grain axes; edge banding on 4 local sides); length and width are numeric
generator parameters editable in the Inspector; the decor stays on the instance, so linked copies can differ. The panel
state (Matches / Modified / Detached) is computed from geometry every time, never saved. A finalizer in the command
stack takes Push/Pull on a panel end back into the parameters, whichever tool made the change.

The cut list is rewritten (v1.5): it classifies by part data, then by material category; groups identical parts across
names; shows hidden parts unticked with a banner; computes finished and cut sizes and banding metres per band type;
counts hardware per piece; the CSV header states which size it carries. Saved library components link by a UUID
`sourceKey`. Cabinet/shelf generators v2 produce parametric panels; old cabinets are rebuilt only on „Actualizează”. A
„Întinde” tool widens bodies by changing panel lengths, not by scaling. The `.aion` format moves to v6, one-way.
Translation comes last: keys, 8 languages, RO and EN complete.

**How the existing system works, per this document**

Today's cut list guesses from geometry: any group under 100 mm counts as a panel (steel handles become steel panels),
identical parts with different names stay on separate rows, grain is lost, banding is estimated from perimeters,
hidden parts vanish with their contents. `Material` has only colour and texture. The cabinet generator is a demo with
one thickness that rebuilds from scratch on every change, wiping manual edits. Every library drop creates a new
definition; „Salvează în bibliotecă” keeps only generator fields. ⌘V and Move with ⌥ make deep unique copies (ADR
0039); only ⌥D and Array link. The UI is Romanian-only.

### Paraphrase of the implementation plan (lines 570–1036)

**What this proposes**

- **Step 0 (no code):** ADRs 0043/0044, `PLAN.md`/`CLAUDE.md` updates, a session with a professional carpenter
  relative that captures a real order as reference test data.
- **Step 1:** v6 format plus `PartSpec` / `sourceKey` / `generatorVersion` on `Definition` and `category` /
  `libraryKey` on `Material`, preserved through extract, copy and boolean; Explode warns.
- **Steps 2–3:** type tables, `measurePart`, the „Fă placă” command.
- **Step 4:** cut list v1.5 with row keys, exclusions and sections.
- **Step 5:** the parametric panel (`createBoard`, `boardState`, `adoptBoardGeometry`, restore/detach), the
  `CommandStack` finalizer and the Inspector group.
- **Steps 6–7:** generators v2; per-side banding with presets, viewport highlight and allowances.
- **Steps 8–9:** linking via `findLinkedDefinition` plus the „În model” list with thumbnails; hardware resources and
  `replaceDefinition`.
- **Steps 10–11:** the „Placă pe față” tool and Stretch.
- **Step 12:** translation (`AION_TR`, `qsTrId`, `.ts` files, CI check).

**How the existing system works, per this document**

Structs are built positionally in the loader and material library, so new fields go at the end. `exportReport`
recomputes the cut list without the dialog's options. `NumberField` is integer-only. `CommandStack` has
commit/transactions and a `ChangeSet`. Commands are classified in `Document::commandModes`; model operations guard
with `require(Modeling)`. UI checks run through `AION_STARTUP` / `AION_SCREENSHOT`.

---

## Findings

### 🔴 Blocker

**1. v5 handles cannot reach „Alte componente” by the described mechanism.**
- *Where:* Goal 1 (57–59), P1 (106–109), §5 step 2 (292–295), §8 (337–338), items 14–15 (670–681), test 757–758.
- *Gap:* `category` is new and read with defaults, so materials in a v5 file have `category = ""`. By the rule at 295,
  empty category means today's ≤ 100 mm rule, so the handles become panels again – the bug Goal 1 promises to fix.
  Step 15 looks up library materials by `libraryKey` only for generators and inserts; nothing assigns a category to
  materials already stored in an opened file.
- *Fix:* add a load-time step (a v5 material whose name matches a library material takes its `category` and
  `libraryKey`), say where it runs (`AionFile` or `Document`), and test it. Otherwise change P1 and Goal 1.

### 🟡 Major

**2. „Dimensiuni” X/Y are in panel axes, but the test treats them as world axes.** (270, test 817) — Line 270 says
panel axes; the test (rotated 90° about Z, Y = 700 → length 700) only holds in world axes. Z read-only only makes sense
in panel axes. *Fix:* state the frame, give the field ↔ parameter mapping for a rotated and a standing panel, align the
test.

**3. No glossary.** (e.g. 62, 64, 97, 303, 373, 417) — „cant”, „adaos”, „dimensiune finită”, „placă brută”, „capăt”,
„fața mare” / „fața de grosime”, „latură locală”, „Desprinsă”, „componentă salvată”, lista „În model”, „deviz”,
„fibră”, PAL/MDF/HDF/ABS are undefined here. *Fix:* copy the brief's glossary (§2.0), trimmed.

**4. „Deviz” is promised but never defined as an output.** (14, Goal 6 71–72, 563) *Fix:* e.g. „deviz = the totals
block of the cut list (sheets, band metres, hardware pieces); no prices in this version”.

**5. The reference cabinet is underspecified; P5's count doesn't match the hinge rule.** (Goal 1 57, P4, P5 137–141,
items 25, 32) — Dimension order, panels, hinges and handles are not given; step 32 gives 4 hinges per door over
1600 mm, so P5's „3 piese ascunse” cannot hold; „N piese” may count rows, panels or panels + hardware. *Fix:* specify the
cabinet fully and define what N counts.

**6. Which generators get v2 and „Generator vechi — Actualizează” is unclear.** (280–282, 827–834) *Fix:* limit
`generatorVersion` and the banner explicitly to `cabinet` and `shelf`.

**7. Stretch refuses generated bodies, but the feature spec never says so.** (P7 150–155, §9 343–356, item 52) *Fix:*
add the prerequisite („Transformă în corp obișnuit”) to P7 and a „Corp generat → refuz” row to §9.

**8. How Stretch changes a panel's length is unclear.** (§9 349, P-1 370–372, item 26) — P-1 grows symmetrically
about the pivot; Stretch assumes growth from the origin. *Fix:* state that Stretch writes the parameter without P-1
compensation (or describe it), and how several planes combine.

**9. Frozen screens are excluded from translation, yet step 59 wraps them and CI demands EN.** (89–91, 992–993,
1013–1014) *Fix:* say whether frozen screens get EN and whether CI exempts them.

**10. The move of saved part names to keys is undescribed.** (365–366, 383–384, 982–983) — Files saved between steps 1
and 12 and v5 files hold Romanian names. *Fix:* the rule that tells a key from a user name, and what happens to
existing names.

**11. „Fă placă” panels have no stated state or editing behaviour.** (701–702, 252) — `makeBoard` sets only `part`, so
by line 252 the result is `Detached`. *Fix:* state the resulting state, Inspector behaviour and how a 17 mm box is
flagged.

**12. Band-highlight cache interaction with ADR 0038 is ambiguous.** (541–542, 860–866) *Fix:* state that
`appendOverlay` runs per frame outside `DocumentPass`, or that `hoveredBandSide` enters `DocumentPassCache::Key`.

**13. `PartSpec` vs `BoardSpec` naming and location contradict.** (49, diagram 181, 196, 614) *Fix:* use `PartSpec`
everywhere and remove it from the Furniture row of the diagram.

**14. The `PLAN.md` step points to an acceptance column not in this document.** (598–599) — The brief's column says
„Scale refuzat”, contradicting D24. *Fix:* use the step tests in this document.

**15. The retraction claim conflicts with automatic web publishing.** (536–538 vs 547–548) *Fix:* state when v6 reaches
users and correct the retraction sentence.

**16. Saving a single panel to the library is not covered.** (Goal 5 69–70, §6 317, test 913) *Fix:* say whether it is
blocked, wrapped in a component, or stored without a key.

### 🟢 Nit

17. „Afectează N copii” — define N as occurrences in the model, not instances of the definition (P6 147, 793–794).
18. Lemn masiv's unit („m liniari”) is outside the listed `BoardType` units (228–230).
19. The effect of `toCutter = false` is never described (228, 663).
20. No rule for `grainAxis = 255`; user-created materials (`libraryKey = ""`) cannot be band decors – unstated (202, 304).
21. The „P” labels collide: journeys P1–P8, decisions P-1…P-6, „funcțiile P2” of another plan.
22. Decision IDs D10–D24 are used without a gloss.
23. Undefined references: „EGM”, „operandul A”, `ChangeSet::meshes`, „stive pe model”; ADR 0030 (pivot) not in Resurse.
24. Unresolved choices: item 10 (`bool* lostPartData` vs `explodeLosesPartData`), item 53 (`TransformTools.cpp` vs
    `StretchTool.cpp`); item 64 listed under step 12 but done in steps 5–9.
25. The open question „legate sau independente?” undercuts Goal 5 and P6; say what changes if the answer is
    „independente”.
26. P7 „Lateralele s-au mutat” is inaccurate: only the + side panel moves.
