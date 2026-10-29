#include "aion/core/math/Intersect.h"

#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace aion::math {

namespace {

constexpr double kTiny = 1e-18;

} // namespace

// Algoritmul din „Real-Time Collision Detection” (Ericson), secțiunea 5.1.9.
ClosestPoints closestPoints(const Segment& first, const Segment& second)
{
    const Vec3d d1 = first.b - first.a;
    const Vec3d d2 = second.b - second.a;
    const Vec3d r = first.a - second.a;
    const double a = glm::dot(d1, d1);
    const double e = glm::dot(d2, d2);
    const double f = glm::dot(d2, r);

    double s = 0.0;
    double t = 0.0;
    if (a <= kTiny && e <= kTiny) {
        s = 0.0;
        t = 0.0;
    } else if (a <= kTiny) {
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = glm::dot(d1, r);
        if (e <= kTiny) {
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = glm::dot(d1, d2);
            const double denom = a * e - b * b;
            // Pentru segmente paralele alegem s = 0 și calculăm t corespunzător.
            s = denom > kTiny * a * e ? std::clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }

    ClosestPoints result;
    result.s = s;
    result.t = t;
    result.onFirst = first.a + d1 * s;
    result.onSecond = second.a + d2 * t;
    result.distance = glm::length(result.onFirst - result.onSecond);
    return result;
}

PointOnSegment projectOnSegment(const Vec3d& p, const Segment& segment)
{
    const Vec3d d = segment.b - segment.a;
    const double len2 = glm::dot(d, d);
    const double t = len2 <= kTiny ? 0.0 : std::clamp(glm::dot(p - segment.a, d) / len2, 0.0, 1.0);
    return { t, glm::length(p - segment.at(t)) };
}

std::optional<double> intersectRayPlane(const Ray& ray, const Plane& plane)
{
    const double denom = glm::dot(plane.normal, ray.direction);
    if (std::abs(denom) <= tol::kAngle) {
        return std::nullopt;
    }
    const double t = (plane.d - glm::dot(plane.normal, ray.origin)) / denom;
    if (t < 0.0) {
        return std::nullopt;
    }
    return t;
}

std::optional<double> intersectRayTriangle(const Ray& ray, const Vec3d& a, const Vec3d& b, const Vec3d& c)
{
    const Vec3d e1 = b - a;
    const Vec3d e2 = c - a;
    const Vec3d p = glm::cross(ray.direction, e2);
    const double det = glm::dot(e1, p);
    if (std::abs(det) <= kTiny) {
        return std::nullopt;
    }
    // O margine relativă mică: o rază exact pe muchia comună a două triunghiuri (diagonala unui dreptunghi) nu trece
    // printre ele din cauza rotunjirii.
    constexpr double kEdge = 1e-9;
    const double inv = 1.0 / det;
    const Vec3d s = ray.origin - a;
    const double u = glm::dot(s, p) * inv;
    if (u < -kEdge || u > 1.0 + kEdge) {
        return std::nullopt;
    }
    const Vec3d q = glm::cross(s, e1);
    const double v = glm::dot(ray.direction, q) * inv;
    if (v < -kEdge || u + v > 1.0 + kEdge) {
        return std::nullopt;
    }
    const double t = glm::dot(e2, q) * inv;
    if (t < 0.0) {
        return std::nullopt;
    }
    return t;
}

double signedArea(std::span<const Vec2d> polygon)
{
    double twice = 0.0;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const Vec2d& p = polygon[i];
        const Vec2d& q = polygon[(i + 1) % polygon.size()];
        twice += p.x * q.y - q.x * p.y;
    }
    return twice * 0.5;
}

PointLocation locatePoint(const Vec2d& p, std::span<const Vec2d> polygon, double eps)
{
    const std::size_t n = polygon.size();
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2d& a = polygon[i];
        const Vec2d& b = polygon[(i + 1) % n];
        const Vec2d d = b - a;
        const double len2 = glm::dot(d, d);
        const double t = len2 <= kTiny ? 0.0 : std::clamp(glm::dot(p - a, d) / len2, 0.0, 1.0);
        if (glm::length(p - (a + d * t)) <= eps) {
            return PointLocation::OnBoundary;
        }
    }
    // Numărul de traversări, cu regula semi-deschisă (un vârf atins de rază se numără o singură dată).
    bool inside = false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const Vec2d& a = polygon[i];
        const Vec2d& b = polygon[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double x = a.x + (p.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (p.x < x) {
                inside = !inside;
            }
        }
    }
    return inside ? PointLocation::Inside : PointLocation::Outside;
}

} // namespace aion::math
