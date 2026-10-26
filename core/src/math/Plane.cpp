#include "aion/core/math/Plane.h"

#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <cmath>

namespace aion::math {

namespace {

// Două normale sunt „aceeași direcție” dacă unghiul dintre ele e sub ~1.4e-6 rad
// (la 100 m, abaterea rămâne sub 0.15 mm).
constexpr double kNormalDotTolerance = 1e-12;

} // namespace

Plane Plane::fromPointNormal(const Vec3d& point, const Vec3d& normal)
{
    const Vec3d n = glm::normalize(normal);
    return { n, glm::dot(n, point) };
}

std::optional<Plane> Plane::fromPoints(const Vec3d& a, const Vec3d& b, const Vec3d& c)
{
    const Vec3d n = glm::cross(b - a, c - a);
    const double len = glm::length(n);
    // Aria minimă: un triunghi mai „subțire” decât toleranța nu definește un plan.
    if (len <= tol::kLength * std::max({ glm::length(b - a), glm::length(c - a), 1.0 })) {
        return std::nullopt;
    }
    return fromPointNormal(a, n / len);
}

std::optional<Plane> Plane::fromPolygon(std::span<const Vec3d> points)
{
    if (points.size() < 3) {
        return std::nullopt;
    }
    Vec3d n { 0.0 };
    Vec3d centroid { 0.0 };
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec3d& p = points[i];
        const Vec3d& q = points[(i + 1) % points.size()];
        n.x += (p.y - q.y) * (p.z + q.z);
        n.y += (p.z - q.z) * (p.x + q.x);
        n.z += (p.x - q.x) * (p.y + q.y);
        centroid += p;
    }
    const double len = glm::length(n);
    if (len <= tol::kLength * tol::kLength) {
        return std::nullopt;
    }
    centroid /= static_cast<double>(points.size());
    return fromPointNormal(centroid, n / len);
}

double Plane::signedDistance(const Vec3d& p) const
{
    return glm::dot(normal, p) - d;
}

Vec3d Plane::project(const Vec3d& p) const
{
    return p - normal * signedDistance(p);
}

bool Plane::coincident(const Plane& other) const
{
    const double dot = glm::dot(normal, other.normal);
    if (std::abs(dot) < 1.0 - kNormalDotTolerance) {
        return false;
    }
    const double otherD = dot > 0.0 ? other.d : -other.d;
    return tol::equal(d, otherD);
}

PlaneFrame::PlaneFrame(const Plane& plane)
    : m_plane(plane)
    , m_origin(plane.origin())
{
    // Axa globală cea mai puțin aliniată cu normala dă un u stabil.
    const Vec3d& n = plane.normal;
    const Vec3d helper = std::abs(n.x) < 0.9 ? Vec3d { 1, 0, 0 } : Vec3d { 0, 1, 0 };
    m_u = glm::normalize(glm::cross(helper, n));
    m_v = glm::cross(n, m_u);
}

Vec2d PlaneFrame::toLocal(const Vec3d& p) const
{
    const Vec3d d = p - m_origin;
    return { glm::dot(d, m_u), glm::dot(d, m_v) };
}

Vec3d PlaneFrame::toWorld(const Vec2d& p) const
{
    return m_origin + m_u * p.x + m_v * p.y;
}

} // namespace aion::math
