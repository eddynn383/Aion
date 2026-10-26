#pragma once

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>

// Toate comparațiile numerice din core trec prin funcțiile de aici.
// Nu comparăm niciodată valori double direct cu ==.
namespace aion::tol {

// Unitatea internă este milimetrul.
inline constexpr double kLength = 1e-3;
// Pentru unghiuri și paralelism, pe vectori normalizați.
inline constexpr double kAngle = 1e-9;

using Vec3d = glm::dvec3;

[[nodiscard]] inline bool isZero(double value, double eps = kLength) noexcept
{
    return std::abs(value) <= eps;
}

[[nodiscard]] inline bool equal(double a, double b, double eps = kLength) noexcept
{
    return std::abs(a - b) <= eps;
}

[[nodiscard]] inline bool samePoint(const Vec3d& a, const Vec3d& b, double eps = kLength) noexcept
{
    const Vec3d d = a - b;
    return glm::dot(d, d) <= eps * eps;
}

// a și b trebuie să fie normalizați.
[[nodiscard]] inline bool parallel(const Vec3d& a, const Vec3d& b, double eps = kAngle) noexcept
{
    const Vec3d c = glm::cross(a, b);
    return glm::dot(c, c) <= eps * eps;
}

// a și b trebuie să fie normalizați.
[[nodiscard]] inline bool perpendicular(const Vec3d& a, const Vec3d& b, double eps = kAngle) noexcept
{
    return std::abs(glm::dot(a, b)) <= eps;
}

} // namespace aion::tol
