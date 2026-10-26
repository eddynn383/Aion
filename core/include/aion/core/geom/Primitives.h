#pragma once

#include "aion/core/geom/Mesh.h"
#include "aion/core/math/Vec.h"

// Primitivele din grupul „Obiecte” al toolbar-ului. Se construiesc într-o geometrie goală, în coordonate locale:
// baza centrată în origine, pe planul XY, cu volumul în sus (Z). Suprafețele curbe au muchiile netede (soft).
namespace aion::geom {

enum class Primitive { Box, Cylinder, Cone, Sphere };

// `size`: lățime (X), adâncime (Y), înălțime (Z). Pentru cilindru și con, X și Y dau diametrul (elipsă dacă diferă,
// deocamdată se folosește X); pentru sferă, diametrul e X. `segments`: numărul de laturi ale cercurilor.
void buildPrimitive(Mesh& mesh, Primitive kind, const math::Vec3d& size, int segments = 24);

// Muchiile dintre fețe aproape coplanare (sub `maxAngleDegrees`) devin soft și netede.
void softenEdges(Mesh& mesh, double maxAngleDegrees);

} // namespace aion::geom
