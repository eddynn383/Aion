#pragma once

#include "forma/core/geom/Id.h"
#include "forma/core/math/Vec.h"

#include <cstdint>
#include <span>
#include <vector>

namespace forma::geom {

class Mesh;

// Triangulează un poligon 2D cu găuri (earcut). rings[0] e conturul exterior, restul sunt găuri.
// Indicii se referă la punctele inelelor puse cap la cap, în ordine.
[[nodiscard]] std::vector<std::uint32_t> triangulatePolygon(std::span<const std::vector<math::Vec2d>> rings);

struct FaceTriangles {
    std::vector<math::Vec3d> points;
    std::vector<std::uint32_t> indices; // câte 3 pe triunghi, în sens trigonometric față de normala feței
};

[[nodiscard]] FaceTriangles triangulateFace(const Mesh& mesh, FaceId face);

} // namespace forma::geom
