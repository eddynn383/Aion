#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"

#include <set>
#include <string>

// Subdivide (Catmull-Clark): fiecare față se împarte în patrulatere, iar vârfurile se mută spre o suprafață netedă,
// așa că obiectul chiar se rotunjește (un cub devine o „pernă”, o sferă cu puține segmente devine mai fină).
// Patrulaterele care nu sunt plane se împart în două triunghiuri; muchiile noi dintre fețe aproape coplanare
// devin netede (soft + smooth). Muchiile cu o singură față (margini) sau cu mai mult de două se păstrează ca margini.
namespace aion::geom {

struct SubdivideResult {
    Selection selection; // fețele noi
    std::string error;

    [[nodiscard]] bool ok() const { return error.empty(); }
};

// Cele mai multe fețe pe care le poate crea o subdivizare (altfel geometria devine prea grea pentru viewport).
inline constexpr std::size_t kMaxSubdivideFaces = 60000;

// Subdivizează fețele date de `levels` ori (1–4). Fețele cu găuri nu se pot subdiviza. Muchiile noi cu unghiul dintre
// fețe cel mult `softAngleDegrees` devin netede.
SubdivideResult subdivide(Mesh& mesh, const std::set<FaceId>& faces, int levels, double softAngleDegrees = 60.0);

} // namespace aion::geom
