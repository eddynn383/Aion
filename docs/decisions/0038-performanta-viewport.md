# ADR 0038 – Performanța viewport-ului: cadre pe orbit, desenul modelului păstrat între cadre

- Stare: acceptat (2026-10-29)
- Context: utilizatorul vedea sacadări și 8–15 fps la orbit / pan pe un fișier mic (`Mobila.aion`: 7 plăci, 13
  obiecte). Măsurat cu `navbench:` (mouse în timp real, la ~120 Hz, prin coada evenimentelor de sistem), în debug:
  orbit 9–11 fps; pe un model de 10 ori mai mare (200 de corpuri, ~1 400 de plăci) 3 fps la orbit și 0 la hover.

## Cauze găsite (profilare cu `sample`)

1. **`cameraChanged` la fiecare eveniment de mouse.** `viewport.orthographic` avea ca notificare tot `cameraChanged`,
   iar lista de comenzi din `Main.qml` (palette, scurtături, meniul nativ) depinde de ea: la fiecare mișcare se
   reconstruia toată, cu scurtăturile și meniurile. Firul principal ajungea saturat și nu mai livra cadre.
2. **Desenul modelului refăcut la fiecare cadru.** `frameForRendering` parcurgea tot documentul (normale netede,
   o inversare de matrice pe fiecare vârf, muchii) chiar dacă se mișca doar camera.
3. **Costuri pătratice în model.** `Model::childrenOf` trecea prin toate instanțele la fiecare apel, iar
   `model::bounds` și alegerea obiectelor (`pickInstance`) îl chemau recursiv pentru fiecare obiect; cutia scenei se
   recalcula la fiecare schimbare a documentului, deci și la hover.
4. **Hover-ul atingea documentul la fiecare mișcare**, chiar fără nimic nou de desenat.
5. **Scripturile** anunțau Outliner-ul, Inspector-ul și selecția după fiecare operație (300 de cutii: peste 15 s).

## Decizii

- `Viewport::cameraChanged` se emite cel mult o dată pe cadru (`updatePolish`); `orthographic` are semnalul lui,
  `projectionChanged`.
- **`DocumentPass`** (în `Viewport.cpp`): triunghiurile, muchiile, punctele și overlay-ul documentului, păstrate cât
  timp cheia nu se schimbă (revizia documentului și a interacțiunilor, aspectul, stilul, texturile, secțiunea, vederea
  curată, cutia demo). Singurul lucru dependent de cameră, profilele (muchia dintre o față văzută din față și una din
  spate), se alege la fiecare cadru dintr-o listă cu normalele celor două fețe. Triunghiurile ajung la renderer printr-un
  `shared_ptr`, fără copie, iar `meshRevision` e generația trecerii (reîncărcarea exact când s-au refăcut).
- `Model` ține un index al copiilor (construit la nevoie, golit la orice schimbare a instanțelor, inclusiv undo / redo
  și copiere). `model::bounds` primește un `BoundsCache` (o definiție folosită de multe instanțe se măsoară o dată);
  `pickInstance` îl folosește pentru toate obiectele. Viewport-ul păstrează cutia scenei după
  `Document::modelRevision()` (nu se schimbă la hover; cât e deschisă o tranzacție, urmează orice atingere).
- `Document::pointerMove` compară ce se vede din unealtă (`ToolLook`: hover-ul și overlay-ul, plus revizia modelului)
  înainte și după: fără nicio diferență, fără tranzacție și fără schimbare de selecție, nu mai atinge documentul.
- Într-o macro (script, `beginMacro`), `runOnModel` nu mai anunță nimic; `endMacro` anunță o singură dată.

## Rezultate (debug, `navbench`)

| Caz | Înainte | După |
|---|---|---|
| `Mobila.aion`, orbit | 9–11 fps | ~117 fps (ecran 120 Hz) |
| `Mobila.aion`, hover | — | ~105 fps |
| 200 de corpuri, orbit | 3 fps | 60 / 115 fps (ecran 60 / 120 Hz) |
| 200 de corpuri, hover | 0 fps | 16–70 fps (release: ~115) |
| 300 de cutii dintr-un script | oprit după 15 s | 0,4 s |
| `pickInstance` + `bounds`, 2 100 de instanțe | 48,6 ms | 3,5 ms |

## Consecințe

- Orice stare nouă care schimbă desenul modelului trebuie să atingă documentul (`touch`) sau să intre în cheia
  `DocumentPassCache::Key`; altfel viewport-ul arată desenul vechi până la următoarea schimbare.
- Hover-ul pe alt element (altă față, alt obiect) reface încă tot desenul modelului; pe modele foarte mari, pasul
  următor ar fi un desen păstrat pe definiție (în coordonatele ei), refolosit de toate instanțele.
- Unelte de măsurare: `navbench:orbit|pan|hover,secunde` și `fpslog:secunde` (fps-ul în fiecare secundă), testul
  `aion_tests "[.perf][model]"`.
