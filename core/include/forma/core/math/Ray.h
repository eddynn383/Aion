#pragma once

#include "forma/core/math/Vec.h"

namespace forma::math {

struct Ray {
    Vec3d origin;
    Vec3d direction; // normalizat

    [[nodiscard]] Vec3d at(double t) const { return origin + direction * t; }
};

} // namespace forma::math
