#include "aion/core/view/Picking.h"

#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

namespace aion::view {

namespace {

std::set<model::TagId>& hiddenTagSet()
{
    static std::set<model::TagId> tags;
    return tags;
}

} // namespace

void setHiddenTags(std::set<model::TagId> tags)
{
    hiddenTagSet() = std::move(tags);
}

const std::set<model::TagId>& hiddenTags()
{
    return hiddenTagSet();
}

bool isHidden(const geom::Face& face)
{
    return face.tag && hiddenTagSet().contains(face.tag);
}

bool isHidden(const geom::Edge& edge)
{
    return edge.tag && hiddenTagSet().contains(edge.tag);
}

bool isHidden(const geom::Mesh& mesh, geom::VertexId vertex)
{
    if (hiddenTagSet().empty()) {
        return false;
    }
    const geom::Vertex& v = mesh.vertex(vertex);
    return !v.edges.empty() && std::all_of(v.edges.begin(), v.edges.end(), [&](geom::EdgeId e) {
        return isHidden(mesh.edge(e));
    });
}

namespace {

using geom::EdgeId;
using geom::FaceId;
using geom::Mesh;
using geom::VertexId;

struct FaceHit {
    FaceId face;
    double t = std::numeric_limits<double>::infinity();
};

FaceHit nearestFace(const Mesh& mesh, const math::Ray& ray)
{
    FaceHit best;
    for (FaceId f : mesh.faces().keys()) {
        if (isHidden(mesh.face(f))) {
            continue;
        }
        // Testul rapid pe cutia conturului: majoritatea fețelor nu sunt sub cursor.
        Vec3d lo { std::numeric_limits<double>::infinity() };
        Vec3d hi { -std::numeric_limits<double>::infinity() };
        for (const geom::EdgeUse& u : mesh.face(f).outer.uses) {
            const Vec3d& p = mesh.position(mesh.useStart(u));
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
        if (!rayHitsBox(ray, lo, hi, tol::kLength)) {
            continue;
        }
        const geom::FaceTriangles tri = geom::triangulateFace(mesh, f);
        for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
            const auto t = math::intersectRayTriangle(
                ray, tri.points[tri.indices[i]], tri.points[tri.indices[i + 1]], tri.points[tri.indices[i + 2]]);
            if (t && *t < best.t) {
                best = { f, *t };
            }
        }
    }
    return best;
}

// Un punct e vizibil dacă nu există o față între el și cameră (cu o mică toleranță,
// ca muchiile și vârfurile de pe fața atinsă să rămână alegibile).
bool visible(const math::Ray& ray, const FaceHit& hit, const Vec3d& p)
{
    if (!hit.face) {
        return true;
    }
    const double depth = glm::dot(p - ray.origin, ray.direction);
    return depth <= hit.t + 1.0 + hit.t * 1e-4;
}

double distanceToSegment2d(Vec2d p, Vec2d a, Vec2d b)
{
    const Vec2d d = b - a;
    const double len2 = glm::dot(d, d);
    const double t = len2 <= 1e-12 ? 0.0 : std::clamp(glm::dot(p - a, d) / len2, 0.0, 1.0);
    return glm::length(p - (a + d * t));
}

// Proiecția unei muchii; capetele din spatele camerei se aduc pe planul apropiat.
std::optional<std::pair<Vec2d, Vec2d>> projectSegment(const Camera& camera, Vec3d a, Vec3d b)
{
    const Vec3d eye = camera.eye();
    const Vec3d fwd = camera.forward();
    if (camera.projection() == Projection::Perspective) {
        const double minDepth = camera.nearPlane() * 1.01;
        double da = glm::dot(a - eye, fwd);
        double db = glm::dot(b - eye, fwd);
        if (da < minDepth && db < minDepth) {
            return std::nullopt;
        }
        if (da < minDepth) {
            a = a + (b - a) * ((minDepth - da) / (db - da));
        } else if (db < minDepth) {
            b = b + (a - b) * ((minDepth - db) / (da - db));
        }
    }
    const auto pa = camera.project(a);
    const auto pb = camera.project(b);
    if (!pa || !pb) {
        return std::nullopt;
    }
    return std::pair { *pa, *pb };
}

bool segmentsCross2d(Vec2d a, Vec2d b, Vec2d c, Vec2d d)
{
    const auto cross = [](Vec2d u, Vec2d v) { return u.x * v.y - u.y * v.x; };
    const double d1 = cross(b - a, c - a);
    const double d2 = cross(b - a, d - a);
    const double d3 = cross(d - c, a - c);
    const double d4 = cross(d - c, b - c);
    return ((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0));
}

struct Rect2 {
    Vec2d lo;
    Vec2d hi;

    [[nodiscard]] bool contains(Vec2d p) const { return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y; }

    [[nodiscard]] bool touches(Vec2d a, Vec2d b) const
    {
        if (contains(a) || contains(b)) {
            return true;
        }
        const Vec2d c[4] { lo, { hi.x, lo.y }, hi, { lo.x, hi.y } };
        for (int i = 0; i < 4; ++i) {
            if (segmentsCross2d(a, b, c[i], c[(i + 1) % 4])) {
                return true;
            }
        }
        return false;
    }
};

} // namespace

bool rayHitsBox(const math::Ray& ray, const Vec3d& lo, const Vec3d& hi, double margin)
{
    double tmin = -std::numeric_limits<double>::infinity();
    double tmax = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        const double o = ray.origin[axis];
        const double d = ray.direction[axis];
        const double a = lo[axis] - margin;
        const double b = hi[axis] + margin;
        if (std::abs(d) < 1e-15) {
            if (o < a || o > b) {
                return false;
            }
            continue;
        }
        double t0 = (a - o) / d;
        double t1 = (b - o) / d;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmin > tmax) {
            return false;
        }
    }
    return tmax >= 0.0;
}

PickResult pick(const Mesh& mesh, const Camera& camera, Vec2d screen, PickOptions options)
{
    const math::Ray ray = camera.rayAt(screen);
    const FaceHit faceHit = nearestFace(mesh, ray);

    PickResult result;
    if (faceHit.face) {
        result.kind = PickResult::Kind::Face;
        result.face = faceHit.face;
        result.point = ray.at(faceHit.t);
    }

    // Vârfuri.
    double bestVertex = options.vertexRadiusPx;
    for (std::size_t i = 0; i < mesh.vertices().size(); ++i) {
        const Vec3d& p = mesh.vertices().values()[i].position;
        const auto s = camera.project(p);
        if (!s || isHidden(mesh, mesh.vertices().keys()[i])) {
            continue;
        }
        const double d = glm::length(*s - screen);
        if (d <= bestVertex && visible(ray, faceHit, p)) {
            bestVertex = d;
            result.kind = PickResult::Kind::Vertex;
            result.vertex = mesh.vertices().keys()[i];
            result.point = p;
        }
    }
    if (result.kind == PickResult::Kind::Vertex) {
        return result;
    }

    // Muchii.
    double bestEdge = options.edgeRadiusPx;
    const math::Segment raySegment { ray.origin, ray.at(camera.farPlane() * 4.0) };
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const geom::Edge& e = mesh.edges().values()[i];
        if (isHidden(e)) {
            continue;
        }
        const Vec3d a = mesh.position(e.v0);
        const Vec3d b = mesh.position(e.v1);
        const auto projected = projectSegment(camera, a, b);
        if (!projected) {
            continue;
        }
        const double d = distanceToSegment2d(screen, projected->first, projected->second);
        if (d > bestEdge) {
            continue;
        }
        const Vec3d onEdge = math::closestPoints(raySegment, { a, b }).onSecond;
        if (!visible(ray, faceHit, onEdge)) {
            continue;
        }
        bestEdge = d;
        result.kind = PickResult::Kind::Edge;
        result.edge = mesh.edges().keys()[i];
        result.point = onEdge;
    }
    return result;
}

geom::Selection pickInRectangle(const Mesh& mesh, const Camera& camera, Vec2d corner0, Vec2d corner1, bool crossing)
{
    const Rect2 rect { glm::min(corner0, corner1), glm::max(corner0, corner1) };
    geom::Selection out;

    const auto inside = [&](VertexId v) {
        const auto s = camera.project(mesh.position(v));
        return s && rect.contains(*s);
    };
    const auto edgeSelected = [&](EdgeId id) {
        const geom::Edge& e = mesh.edge(id);
        if (!crossing) {
            return inside(e.v0) && inside(e.v1);
        }
        const auto projected = projectSegment(camera, mesh.position(e.v0), mesh.position(e.v1));
        return projected && rect.touches(projected->first, projected->second);
    };

    for (EdgeId e : mesh.edges().keys()) {
        if (!isHidden(mesh.edge(e)) && edgeSelected(e)) {
            out.edges.insert(e);
        }
    }
    for (FaceId f : mesh.faces().keys()) {
        const geom::Face& face = mesh.face(f);
        if (isHidden(face)) {
            continue;
        }
        bool selected = false;
        if (!crossing) {
            selected = std::all_of(face.outer.uses.begin(), face.outer.uses.end(),
                [&](const geom::EdgeUse& u) { return out.edges.contains(u.edge); });
        } else {
            selected = std::any_of(face.outer.uses.begin(), face.outer.uses.end(),
                [&](const geom::EdgeUse& u) { return out.edges.contains(u.edge); });
            if (!selected) {
                // Dreptunghiul poate fi complet în interiorul feței.
                std::vector<Vec2d> polygon;
                bool ok = true;
                for (const Vec3d& p : mesh.loopPoints(face.outer)) {
                    const auto s = camera.project(p);
                    ok = ok && s.has_value();
                    if (s) {
                        polygon.push_back(*s);
                    }
                }
                selected = ok
                    && math::locatePoint((rect.lo + rect.hi) * 0.5, polygon, 0.0) != math::PointLocation::Outside;
            }
        }
        if (selected) {
            out.faces.insert(f);
        }
    }
    return out;
}

} // namespace aion::view
