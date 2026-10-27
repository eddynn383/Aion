# ADR 0021 – Interacțiuni cu blocuri și Modul Interacțiune

- Stare: acceptat (2026-10-27)
- Context: utilizatorul vrea interacțiuni definite logic („click stânga → mută obiectul din X în Y”), cu evenimente
  diverse (click, click dreapta, drag, apăsare lungă, hover, tastă), înlănțuite, editate cu blocuri de tip puzzle
  (ca Sketchware), și un mod în care obiectele nu mai pot fi modificate, iar cursorul arată interacțiunea disponibilă.

## Decizii

- **Model:** fiecare instanță are `rules` (`model::Rule` = eveniment + tastă + listă de `model::Action`). Acțiunile:
  mută cu / la, rotește, scalează, arată / ascunde / comută, schimbă materialul, pornește o animație, mergi la scenă,
  așteaptă, alternează (două ramuri, pe rând la fiecare declanșare). Regulile se salvează în `.aion` și au undo.
- **Runtime-ul e în core** (`model::RuleRuntime`, fără Qt, cu teste): `fire` pornește o secvență, `update(dt)` o
  avansează (tween-uri pe transformare, așteptări), iar cererile pentru gazdă (animație, scenă) ies prin
  `takeRequests`. Contoarele „Alternează” sunt per instanță și cale.
- **Modul Interacțiune rulează pe o copie a modelului** (`Document::m_runtime`). Ce fac regulile nu intră în
  istoric și dispare la ieșire (Esc / F5 / butonul ▶ din bara viewport-ului). Uneltele de editare nu primesc input.
- **Cursorul** (în `Viewport`): mână deschisă pentru drag, mână cu deget pentru click / dublu-click / apăsare lungă,
  semnul întrebării pentru click dreapta, altfel săgeată. Ținta e obiectul de nivel superior care are reguli.
- **Editorul** (`RulesEditor.qml`): paletă pe categorii colorate; un eveniment tras în dreapta începe o regulă,
  acțiunile se pun înainte / după un bloc sau în ramurile „Alternează”; un bloc tras înapoi pe paletă se șterge.
  Parametrii se editează direct în bloc. Fiecare schimbare trece prin `Document::setRules` (un pas de undo).
- „Balama” și „Glisare” (ADR 0018) rămân ca prescurtări în Inspector și funcționează și în Modul Interacțiune.

## Limite și urmări

- Nu există încă bucle („repetă”), condiții pe stări arbitrare sau variabile; „Alternează” acoperă cazul ușă
  deschisă / închisă. Lipsesc schimbarea culorii (doar materialul) și acțiuni pe camere / lumini (Faza 19).
- Drag-ul declanșează regula o dată la început; mișcarea liberă a obiectului cu mouse-ul nu e încă o acțiune.
