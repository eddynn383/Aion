# 0011 – Ghidaje, adnotări, secțiuni, booleene și scene

**Status:** acceptat
**Data:** 2026-10-26

## Decizie
- **Entități noi în model** (cu undo și în `.aion`): ghidaje (punct sau linie infinită, într-o definiție), adnotări
  (cotă sau text, într-o definiție), planuri de secțiune (în coordonatele modelului, cel mult unul activ) și scene
  (poziția camerei și stilul). Ghidajele și adnotările unei definiții șterse dispar odată cu ea.
- **Inferența** se prinde de ghidaje: puncte de construcție, intersecții de linii de construcție, „On Guide” și
  intersecția unei direcții cu un ghidaj (`Engine::withGuides`).
- **Tape Measure**: de pe o muchie lasă o linie de construcție paralelă, altfel un punct; Option doar măsoară.
  **Protractor**: o linie de construcție la un unghi. **Dimension**: cotă cu linia deplasată. **Text**: ancorat într-un
  punct, cu textul implicit dat de ce e acolo (aria feței, lungimea muchiei, coordonatele), editabil imediat în viewport.
- **Secțiunea** activă taie fețele în shader (discard) și muchiile pe CPU; liniile de tăietură se calculează din
  triunghiurile fețelor tăiate. Planul se vede ca un dreptunghi portocaliu.
- **Intersect Faces**: segmentele în care fețele selectate taie celelalte fețe (dreapta comună a planelor, decupată
  de ambele poligoane), desenate cu `drawLine` (deci împart fețele).
- **Booleene** pe solide (fiecare muchie între exact două fețe) cu **Manifold**: reuniune, diferență, intersecție;
  rezultatul e un grup nou (geometria reconstruită cu `MeshBuilder`, fețele coplanare unite).
- **Scene**: panou cu adăugare, actualizare, redenumire, ștergere; click = tranziție animată a camerei.

## Consecințe
- Adnotările din definițiile cu mai multe instanțe se desenează o singură dată (la prima instanță).
- Secțiunea nu taie umbrele (harta de umbre vede tot modelul).
- Manifold e o dependență nouă (Apache-2.0); se compilează și pentru WebAssembly.
