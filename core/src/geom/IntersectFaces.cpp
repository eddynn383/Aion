#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>

namespace aion::geom {

namespace {

using math::Vec2d;

constexpr double kTol = tol::kLength;

// Intervalele (parametri de-a lungul dreptei p0 + t·u) în care dreapta e în interiorul feței.
std::vector<std::pair<double, double>> insideIntervals(const Mesh& mesh, FaceId id, const Vec3d& p0, const Vec3d& u)
{
    const Face& face = mesh.face(id);
    std::vector<double> ts;
    const auto addLoop = [&](const Loop& loop) {
        const std::vector<Vec3d> pts = mesh.loopPoints(loop);
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const Vec3d a = pts[i];
            const Vec3d b = pts[(i + 1) % pts.size()];
            // Cele mai apropiate puncte dintre dreaptă și segment; dacă se ating, parametrul pe dreaptă.
            const Vec3d d = b - a;
            const Vec3d w = p0 - a;
            const double bb = glm::dot(u, d);
            const double cc = glm::dot(d, d);
            const double dd = glm::dot(u, w);
            const double ee = glm::dot(d, w);
            const double denom = cc - bb * bb;
            if (std::abs(denom) < 1e-12) {
                continue; // paralele: capetele segmentului apar ca intersecții cu laturile vecine
            }
            const double s = (bb * ee - cc * dd) / denom;
            const double t = (ee - bb * dd) / denom;
            if (t < -1e-9 || t > 1.0 + 1e-9) {
                continue;
            }
            if (glm::length((p0 + u * s) - (a + d * t)) <= kTol) {
                ts.push_back(s);
            }
        }
    };
    addLoop(face.outer);
    for (const Loop& h : face.inner) {
        addLoop(h);
    }
    std::sort(ts.begin(), ts.end());
    std::vector<std::pair<double, double>> out;
    for (std::size_t i = 0; i + 1 < ts.size(); ++i) {
        if (ts[i + 1] - ts[i] <= kTol) {
            continue;
        }
        const Vec3d mid = p0 + u * ((ts[i] + ts[i + 1]) * 0.5);
        if (faceContainsPoint(mesh, id, mid)) {
            out.emplace_back(ts[i], ts[i + 1]);
        }
    }
    return out;
}

} // namespace

std::size_t intersectFaces(Mesh& mesh, const std::set<FaceId>& faces, bool withAll)
{
    std::vector<std::pair<Vec3d, Vec3d>> segments;
    const std::vector<FaceId> others = withAll ? mesh.faces().keys() : std::vector<FaceId>(faces.begin(), faces.end());
    for (FaceId a : faces) {
        if (!mesh.faces().contains(a)) {
            continue;
        }
        for (FaceId b : others) {
            if (a == b || !mesh.faces().contains(b) || (faces.contains(b) && b < a)) {
                continue;
            }
            const math::Plane& pa = mesh.face(a).plane;
            const math::Plane& pb = mesh.face(b).plane;
            const Vec3d dir = glm::cross(pa.normal, pb.normal);
            const double len2 = glm::dot(dir, dir);
            if (len2 < 1e-12) {
                continue; // plane paralele
            }
            const Vec3d p0 = (glm::cross(pb.normal, dir) * pa.d + glm::cross(dir, pa.normal) * pb.d) / len2;
            const Vec3d u = dir / std::sqrt(len2);
            const auto ia = insideIntervals(mesh, a, p0, u);
            const auto ib = insideIntervals(mesh, b, p0, u);
            for (const auto& [a0, a1] : ia) {
                for (const auto& [b0, b1] : ib) {
                    const double lo = std::max(a0, b0);
                    const double hi = std::min(a1, b1);
                    if (hi - lo > kTol) {
                        segments.emplace_back(p0 + u * lo, p0 + u * hi);
                    }
                }
            }
        }
    }
    for (const auto& [p, q] : segments) {
        drawLine(mesh, p, q);
    }
    return segments.size();
}

} // namespace aion::geom
