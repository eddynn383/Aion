# ADR 0039 – Copii unice și animații pornite din interacțiuni

- Stare: acceptat (2026-10-29)
- Context: designul aprobat [`docs/plans/copii-si-animatii-interactive.md`](../plans/copii-si-animatii-interactive.md)
  și planul [`docs/plans/copii-si-animatii-interactive-implementare.md`](../plans/copii-si-animatii-interactive-implementare.md).
  Pe `Mobila.aion`, un click pe ușile din Corp A deschidea și ușile din Corp B: Corp B era o copie făcută cu Move,
  iar `cloneDefinition` copia doar primul nivel, deci grupul „Usi” rămânea comun („×2”); în plus, animația pornită din
  interacțiune rula ca timeline-ul (una singură, globală, cu bucla ei).

## Decizii

- **Copiile sunt unice, în adâncime** (`model/Copying.h`, `copyDeep`): fiecare definiție din subarbore primește o copie,
  iar legăturile din interiorul obiectului copiat se păstrează. Așa copiază ⌘C / ⌘V și Move / Rotate cu Option
  (inclusiv seriile ×N). **Doar ⌥D și comanda Array fac copii legate** (`copyLinked`, aceeași definiție, și pentru
  grupuri și colecții). `copyInstance` și `makeUnique` au dispărut.
- **„Fă unic” e în adâncime** (`makeUniqueDeep`): copiază doar definițiile folosite și în afara obiectului (cele proprii
  își păstrează instanțele și id-urile). Apare (Inspector, lista Obiecte, meniul contextual) când `canMakeUnique`:
  obiectul are copii legate sau conține ceva legat de altceva. Fișierele vechi nu se repară singure.
- **Animațiile și regulile copiilor** (`copyAnimations`): o animație pornită de o regulă din obiectul copiat primește o
  copie („(copie)”, „(copie 2)”) cu pistele copiei, iar regulile copiei trec pe ea; o animație de timeline primește
  pistele copiei; pista obiectului de sus își mută pozițiile cu copia; căile și camera nu se copiază. La „Fă unic”, o
  regulă din afara obiectului care pornea animația separată trece pe copie dacă își conține doar copiile, rămâne dacă
  își conține doar originalele, altfel primește un bloc în plus (un întrerupător pornește ambele).
- **Clipboard-ul ține de document** (`Document::copySelection` / `pasteClipboard`): o copie a modelului de la ⌘C; se
  golește la Nou și la Deschide. ⌘V pune unealta internă `place` (ca `pivot`): copia se ține de colțul minim al cutiei
  ei, se prinde de grid și de colțurile / mijloacele muchiilor obiectului de sub cursor; click o scrie (un pas de undo
  „Lipește”), Esc renunță. Fără săgeți și VCB în prima versiune.
- **Redarea din interacțiuni e `model::AnimationPlayer`**, nu timeline-ul: fiecare animație cu ceasul ei, mai multe în
  paralel, starea ține de animație. Blocul „Pornește animația” are sensul (Înainte / Înapoi / Comută) și repetarea
  (o dată, de N ori, la infinit; `Action::direction`, `Action::repeat`). O dată, într-un sens: o animație ajunsă la
  capăt nu mai face nimic. Comută decide o dată pentru toate blocurile declanșării: toate „deschise” (la sfârșit sau în
  drum spre el) → se închid, altfel se deschid. Repetările reiau fiecare tură de la capătul de pornire; bucla infinită
  se oprește pe loc la același bloc. Cu două animații pe același obiect, câștigă ultima pornită. Playerul scrie poza în
  copia Modului interacțiune, de la poza documentului; culoarea și camera vin tot din el.
- **Formatul `.aion` v5**: `direction` și `repeat` pe acțiuni; fișierele vechi dau Înainte / O dată (bucla din
  timeline, `Animation::loop`, rămâne doar pentru previzualizarea din Animare).

## Consecințe

- O copie cu Move a unei componente nu mai rămâne legată de original; pentru legătură, ⌥D sau Array.
- O copie făcută cu Rotate a unui obiect animat el însuși își mută doar cheile de poziție; rotația din animație rămâne.
- Lipirea între documente, ⌘X, copierea geometriei libere și a căilor rămân pe mai târziu.
