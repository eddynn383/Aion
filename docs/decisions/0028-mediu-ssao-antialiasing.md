# ADR 0028 – Mediu de iluminare, SSAO și antialiasing la cameră oprită

- Stare: acceptat (2026-10-27)
- Context: rămase din Faza 8: iluminare din mediu (IBL), umbrire ambientală (SSAO), antialiasing temporal (TAA) și
  preset-uri de calitate.

## Decizii

- **Medii procedurale, nu HDRI.** Un mediu e un gradient cer (zenit → orizont) și sol, cu intensitate și tăria
  reflexiilor: Simplu (lumina de până acum, fără reflexii), Studio, Exterior, Înnorat (soarele atenuat). Lumina
  ambientală se citește după normală, iar reflexiile după direcția reflectată (Fresnel Schlick, estompate spre lumina
  ambientală pe suprafețele aspre). Fără fișiere HDR în repo și fără precalculări; arată bine pe modele arhitecturale.
- **SSAO în două treceri suplimentare** (doar cu opțiunea activă): `gbuffer.frag` scrie normala (în spațiul camerei)
  și adâncimea liniară în RGBA16F; `ssao.frag` caută 12 puncte într-o emisferă (rotită aleator pe pixel), cu raza
  proporțională cu distanța până la țintă (merge la orice scară). `mesh.frag` citește rezultatul netezit 4 × 4 și
  atenuează lumina ambientală (și 45% din cea directă, ca să se vadă contactele).
- **Antialiasing „progresiv” = MSAA 8× cât camera stă pe loc.** După 0,4 s fără mișcare, viewport-ul trece la 8
  eșantioane; la prima mișcare revine la valoarea din Preferințe. Un TAA adevărat (cadre cu jitter acumulate) ar fi
  cerut randarea în texturi intermediare și reproiectare; MSAA 8× dă margini aproape la fel de fine, cu cod mult mai
  puțin. Schimbarea refă țintele de randare (un cadru mai lent la începutul unei mișcări).
- **Preset-uri** în Preferințe → Viewport (Performanță, Echilibrat, Calitate) peste MSAA, calitatea umbrelor, SSAO și
  antialiasing-ul la cameră oprită. Toate se salvează acum în preferințe (MSAA și umbrele nu se salvau înainte).

## Limite

- Fără HDRI încărcate de utilizator, fără reflexii ale altor obiecte (doar ale mediului).
- SSAO lucrează doar pe fețele opace (nu pe sticlă).
