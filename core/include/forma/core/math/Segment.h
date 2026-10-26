#pragma once

#include "forma/core/math/Vec.h"

#include <glm/geometric.hpp>

namespace forma::math {

struct Segment {
    Vec3d a;
    Vec3d b;

    [[nodiscard]] Vec3d direction() const { return b - a; }
    [[nodiscard]] double length() const { return glm::length(b - a); }
    [[nodiscard]] Vec3d at(double t) const { return a + (b - a) * t; }
    [[nodiscard]] Vec3d midpoint() const { return (a + b) * 0.5; }
};

} // namespace forma::math
