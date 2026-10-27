#include "aion/core/model/Texturing.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace aion::model {

std::pair<math::Vec3d, math::Vec3d> textureAxes(const math::Vec3d& normal)
{
    const math::Vec3d n = glm::length(normal) > 1e-12 ? glm::normalize(normal) : math::Vec3d { 0, 0, 1 };
    if (std::abs(n.z) > 0.7071) {
        // Pardoseli și tavane: X și Y ale lumii (textura nu se oglindește văzută de sus).
        return { { 1, 0, 0 }, { 0, n.z > 0.0 ? 1.0 : -1.0, 0 } };
    }
    const math::Vec3d u = glm::normalize(glm::cross(math::Vec3d { 0, 0, 1 }, n));
    return { u, glm::normalize(glm::cross(n, u)) };
}

math::Vec2d textureCoordinates(const math::Vec3d& normal, const math::Vec3d& p,
    const geom::TextureTransform& transform, const math::Vec2d& textureSize)
{
    const auto [u, v] = textureAxes(normal);
    // În mm pe plan, apoi poziționarea feței: deplasare, rotație, scară.
    math::Vec2d mm { glm::dot(p, u), glm::dot(p, v) };
    mm -= transform.offset;
    const double a = glm::radians(-transform.rotation);
    mm = { mm.x * std::cos(a) - mm.y * std::sin(a), mm.x * std::sin(a) + mm.y * std::cos(a) };
    const math::Vec2d scale { std::max(std::abs(transform.scale.x), 1e-6), std::max(std::abs(transform.scale.y), 1e-6) };
    const math::Vec2d size { std::max(textureSize.x, 1e-6), std::max(textureSize.y, 1e-6) };
    return mm / (size * scale);
}

} // namespace aion::model
