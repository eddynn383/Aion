#pragma once

#include "aion/core/geom/Topology.h"
#include "aion/core/math/Vec.h"

#include <utility>

// Coordonatele de textură (UV) ale fețelor: proiecție plană după normala feței, la mărimea reală a texturii
// (`textureSize` mm pe o repetiție), apoi poziționarea feței (deplasare, rotație, scară). Vezi ADR 0024.
namespace aion::model {

// Axele planului de proiecție pentru o normală: pentru fețe orizontale U = X și V = Y; pentru celelalte, U e
// orizontală (de-a lungul feței) și V urcă pe Z, ca textura (cărămizile, fibra) să stea „în picioare” pe pereți.
[[nodiscard]] std::pair<math::Vec3d, math::Vec3d> textureAxes(const math::Vec3d& normal);

// UV-ul punctului `p` (în coordonatele definiției) pe o față cu normala dată. 1 unitate = o repetiție a texturii.
[[nodiscard]] math::Vec2d textureCoordinates(const math::Vec3d& normal, const math::Vec3d& p,
    const geom::TextureTransform& transform, const math::Vec2d& textureSize);

} // namespace aion::model
