# 0010 – Materiale, stiluri de afișare și umbre

**Status:** acceptat
**Data:** 2026-10-26

## Context
Faza 8 cere o grafică mai bună: materiale, stiluri de afișare, umbre, tonemapping. Pipeline-ul complet (SSAO, IBL,
TAA, cascade de umbre, texturi) e mare; am ales întâi ce schimbă cel mai mult aspectul cu cel mai mic risc.

## Decizie
- **Materiale** în model: nume, culoare RGBA (alfa = opacitate), rugozitate, metalic. O față are un material (nul =
  implicit); o instanță are un material pentru fețele fără material din interior („vopsirea” unui grup). Materialul se
  moștenește la împărțirea fețelor, la Push/Pull (capacul), la copiere și la plierea fețelor neplane. Se salvează în
  `.aion` (câmpuri opționale: fișierele vechi rămân valide).
- **Biblioteca** inclusă (24 de materiale) intră în model abia la prima folosire, în aceeași comandă cu vopsirea.
- **Paint Bucket (B)**: click = față sau grup; Option = pipetă; Cmd = tot ce e conectat; Shift = toate fețele cu
  același material.
- **Randare**: fiecare vârf poartă culoarea și parametrii materialului. Iluminare în spațiu liniar (cer + soare,
  Blinn-Phong cu strălucire după rugozitate), **tonemapping ACES**, ieșire sRGB. Fețele transparente (alfa < 1) se
  desenează după cele opace, cu amestec.
- **Stiluri**: materiale, clay (alb mat), linii ascunse, wireframe, X-Ray. **Profile**: muchiile de contur (margini sau
  muchii între o față văzută din față și una din spate) sunt mai groase. **Ceață** după distanță.
- **Umbre**: o hartă de adâncime (R32F, 2048²) randată din direcția soarelui, cu o proiecție ortografică ce cuprinde
  scena; fețele o eșantionează cu PCF 3×3, iar solul (planul Z = 0, invizibil) primește umbrele printr-un pas separat.
  Soarele se calculează din oră, lună și latitudine (implicit 46° N).

## Consecințe
- Rămân de făcut: SSAO, IBL din HDRI, TAA, cascade de umbre, texturi și UV, Display P3, preset-uri de calitate,
  drag & drop de materiale pe fețe.
- O singură hartă de umbre pentru toată scena: la scene foarte mari umbrele devin mai puțin precise.
