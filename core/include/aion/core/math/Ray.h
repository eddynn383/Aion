#pragma once

#include "aion/core/math/Vec.h"

namespace aion::math {

struct Ray {
    Vec3d origin;
    Vec3d direction; // normalizat

    [[nodiscard]] Vec3d at(double t) const { return origin + direction * t; }
};

} // namespace aion::math
