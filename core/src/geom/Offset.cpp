#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <map>

namespace aion::geom {

namespace {

using math::Vec2d;

constexpr double kTol = tol::kLength;

double cross(Vec2d a, Vec2d b)
{
    return a.x * b.y - a.y * b.x;
}

bool segmentsIntersect(Vec2d a, Vec2d b, Vec2d c, Vec2d d)
{
    const double d1 = cross(b - a, c - a);
    const double d2 = cross(b - a, d - a);
    const double d3 = cross(d - c, a - c);
    const double d4 = cross(d - c, b - c);
    return ((d1 > kTol) != (d2 > kTol) || std::abs(d1) <= kTol || std::abs(d2) <= kTol)
        && ((d3 > kTol) != (d4 > kTol) || std::abs(d3) <= kTol || std::abs(d4) <= kTol)
        && std::min(a.x, b.x) <= std::max(c.x, d.x) + kTol && std::min(c.x, d.x) <= std::max(a.x, b.x) + kTol
        && std::min(a.y, b.y) <= std::max(c.y, d.y) + kTol && std::min(c.y, d.y) <= std::max(a.y, b.y) + kTol;
}

bool simple(const std::vector<Vec2d>& poly)
{
    const std::size_t n = poly.size();
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = i + 1; j < n; ++j) {
            if (j == i + 1 || (i == 0 && j == n - 1)) {
                continue; // laturi vecine
            }
            if (segmentsIntersect(poly[i], poly[(i + 1) % n], poly[j], poly[(j + 1) % n])) {
                return false;
            }
        }
    }
    return true;
}

bool crosses(const std::vector<Vec2d>& a, const std::vector<Vec2d>& b)
{
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t j = 0; j < b.size(); ++j) {
            if (segmentsIntersect(a[i], a[(i + 1) % a.size()], b[j], b[(j + 1) % b.size()])) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

std::optional<std::vector<Vec3d>> offsetOutline(const Mesh& mesh, FaceId id, double distance)
{
    if (!mesh.faces().contains(id) || std::abs(distance) <= kTol) {
        return std::nullopt;
    }
    const Face& face = mesh.face(id);
    const math::PlaneFrame frame(face.plane);
    std::vector<Vec2d> poly;
    for (const Vec3d& p : mesh.loopPoints(face.outer)) {
        poly.push_back(frame.toLocal(p));
    }
    const std::size_t n = poly.size();
    // Bucla exterioară e în sens trigonometric, deci interiorul e la stânga fiecărei laturi.
    std::vector<Vec2d> out;
    for (std::size_t i = 0; i < n; ++i) {
        const Vec2d prev = poly[(i + n - 1) % n];
        const Vec2d cur = poly[i];
        const Vec2d next = poly[(i + 1) % n];
        const Vec2d d0 = glm::normalize(cur - prev);
        const Vec2d d1 = glm::normalize(next - cur);
        const Vec2d n0 { -d0.y, d0.x };
        const Vec2d n1 { -d1.y, d1.x };
        const double denom = cross(d0, d1);
        if (std::abs(denom) < 1e-9) {
            out.push_back(cur + n1 * distance); // laturi coliniare
            continue;
        }
        // Intersecția dreptelor deplasate: (prev + n0·d) + d0·t = (cur + n1·d) + d1·s
        const Vec2d p0 = cur + n0 * distance;
        const Vec2d p1 = cur + n1 * distance;
        const double t = cross(p1 - p0, d1) / denom;
        out.push_back(p0 + d0 * t);
    }

    // Verificări: același sens (nu s-a întors pe dos), fără autointersecții, fără contact cu găurile sau cu conturul.
    if (math::signedArea(out) <= kTol * kTol || !simple(out) || crosses(out, poly)) {
        return std::nullopt;
    }
    for (const Loop& h : face.inner) {
        std::vector<Vec2d> hole;
        for (const Vec3d& p : mesh.loopPoints(h)) {
            hole.push_back(frame.toLocal(p));
        }
        if (crosses(out, hole) || math::locatePoint(hole.front(), out, kTol) != math::PointLocation::Inside) {
            if (distance > 0.0) {
                return std::nullopt;
            }
        }
    }
    std::vector<Vec3d> result;
    for (const Vec2d& p : out) {
        result.push_back(frame.toWorld(p));
    }
    return result;
}

bool offsetFace(Mesh& mesh, FaceId id, double distance)
{
    const auto outline = offsetOutline(mesh, id, distance);
    if (!outline) {
        return false;
    }
    // Stilul laturilor conturului: segmentele unei curbe dau tot o curbă (cu identificator nou).
    std::vector<Edge> styles;
    for (const EdgeUse& u : mesh.face(id).outer.uses) {
        styles.push_back(mesh.edge(u.edge));
    }
    drawPolygon(mesh, *outline);
    std::map<std::uint32_t, std::uint32_t> curves;
    std::uint32_t next = mesh.newCurveId();
    const std::size_t n = outline->size();
    for (std::size_t i = 0; i < n; ++i) {
        const Edge& style = styles[i];
        if (style.curve == 0) {
            continue;
        }
        const VertexId a = mesh.findVertexNear((*outline)[i], kTol);
        const VertexId b = mesh.findVertexNear((*outline)[(i + 1) % n], kTol);
        if (const EdgeId e = a && b ? mesh.findEdge(a, b) : EdgeId {}) {
            const auto [it, inserted] = curves.try_emplace(style.curve, next);
            next += inserted ? 1 : 0;
            mesh.setEdgeStyle(e, false, it->second, style.smoothCurve);
        }
    }
    return true;
}

} // namespace aion::geom
