#pragma once

#include "forma/core/geom/Mesh.h"

#include <vector>

// Operațiile de modelare compuse din primitivele Mesh (vezi ADR 0003).
namespace forma::geom {

struct DrawLineResult {
    // Bucățile liniei, de la a la b (unele pot exista deja în model).
    std::vector<EdgeId> edges;
    std::vector<FaceId> createdFaces;
    std::vector<FaceId> removedFaces;
};

// Desenează un segment cu comportamentul „lipicios” al geometriei (PLAN.md 4.3): se unește cu
// vârfurile existente, împarte muchiile pe care le atinge sau le intersectează, nu dublează muchii
// existente, închide fețe noi în ciclurile coplanare și împarte fețele pe care le traversează.
DrawLineResult drawLine(Mesh& mesh, const Vec3d& a, const Vec3d& b);

// Desenează o polilinie închisă (de exemplu conturul unui dreptunghi).
DrawLineResult drawPolygon(Mesh& mesh, const std::vector<Vec3d>& points);

// Șterge o muchie, fețele care o folosesc și vârfurile rămase fără muchii.
void eraseEdge(Mesh& mesh, EdgeId edge);

// Șterge o față (muchiile rămân).
void eraseFace(Mesh& mesh, FaceId face);

// Regiunea unei fețe (în interior, nu pe margine și nu într-o gaură).
[[nodiscard]] bool faceContainsPoint(const Mesh& mesh, FaceId face, const Vec3d& point);

} // namespace forma::geom
