# ADR 0024 – Texturi și UV

- Stare: acceptat (2026-10-27)
- Context: materialele aveau doar culoare. Utilizatorul vrea texturi (lemn, cărămidă, parchet) cu poziționare,
  rămase din Faza 8.

## Decizii

- **Imaginile sunt în model** (`model::Texture`: nume, sursă, octeții PNG / JPG), cu undo, salvate în arhiva `.aion`
  ca fișiere `textures/<id>.png|jpg` (în `model.json` doar numele și sursa). Core-ul nu decodează imagini (fără Qt).
- **Materialul** are `texture` și `textureSize` (mm pe o repetiție); culoarea nuanțează textura (alb = neschimbată).
- **UV implicit = proiecție plană pe față** (`model/Texturing.h`): pardoselile pe X / Y ale definiției, pereții cu U
  orizontal și V pe Z (cărămizile și fibra stau „în picioare”). Fiecare față are o `TextureTransform` (deplasare,
  rotație, scară) pentru „Poziția texturii”, editabilă în Inspector pentru fețele selectate.
- **Randare**: un singur array de texturi (RGBA8, 1024², mipmap-uri) legat pe binding 2; vârful primește (u, v,
  strat). Imaginile se decodează în `Viewport` (cache după conținut) și se reîncarcă doar când se schimbă setul.
- **Texturi procedurale** (cărămidă, parchet, lemn, beton, gresie) desenate la cerere cu QPainter
  (`render/src/ProceduralTextures.cpp`): fără imagini binare în repo. Materialele texturate din bibliotecă intră în
  model (cu textura lor) când sunt alese. Editorul de material poate folosi și o imagine proprie.
- **Export**: glTF cu materiale PBR, `TEXCOORD_0` și imaginile în buffer (mesh separat pe material moștenit de la
  obiect); OBJ cu `vt`, `usemtl` pe material, MTL cu `map_Kd` și imaginile scrise lângă fișier.

## Limite

- Textura se aplică doar pe fața (nu și pe un verso cu material propriu, care rămâne doar cu culoare).
- Fără unealtă de poziționare direct în viewport (mânere pe față); deocamdată câmpurile din Inspector.
- Toate texturile se redimensionează la 1024² în array (raportul laturilor se păstrează prin `textureSize`).
