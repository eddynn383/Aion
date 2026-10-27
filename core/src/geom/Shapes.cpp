#include "aion/core/geom/Shapes.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/math/Transform.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <numbers>
#include <set>

namespace aion::geom {

namespace {

// Vârfurile unui lanț de muchii, în ordine; `closed` dacă lanțul se închide. Gol dacă muchiile nu formează un lanț.
std::vector<VertexId> orderChain(const Mesh& mesh, const std::vector<EdgeId>& edges, bool& closed)
{
    std::map<VertexId, std::vector<EdgeId>> around;
    for (EdgeId e : edges) {
        around[mesh.edge(e).v0].push_back(e);
        around[mesh.edge(e).v1].push_back(e);
    }
    VertexId start = around.begin()->first;
    closed = true;
    for (const auto& [v, list] : around) {
        if (list.size() > 2) {
            return {};
        }
        if (list.size() == 1) {
            start = v;
            closed = false;
        }
    }
    std::vector<VertexId> out { start };
    std::set<EdgeId> used;
    VertexId current = start;
    while (used.size() < edges.size()) {
        EdgeId next;
        for (EdgeId e : around[current]) {
            if (!used.contains(e)) {
                next = e;
                break;
            }
        }
        if (!next) {
            return {};
        }
        used.insert(next);
        current = mesh.edge(next).other(current);
        if (current != start) {
            out.push_back(current);
        }
    }
    return out;
}

Vec3d circumcenter(const Vec3d& a, const Vec3d& b, const Vec3d& c)
{
    const Vec3d ab = b - a;
    const Vec3d ac = c - a;
    const Vec3d n = glm::cross(ab, ac);
    const double d = 2.0 * glm::dot(n, n);
    if (d < 1e-18) {
        return (a + c) * 0.5;
    }
    return a + (glm::cross(n, ab) * glm::dot(ac, ac) + glm::cross(ac, n) * glm::dot(ab, ab)) / d;
}

// Fața formată exact de muchiile date (bucla ei exterioară), fără găuri; nulă dacă nu există.
FaceId faceOf(const Mesh& mesh, const std::vector<EdgeId>& edges)
{
    const std::set<EdgeId> set(edges.begin(), edges.end());
    for (FaceId f : mesh.edge(edges.front()).faces) {
        const Face& face = mesh.face(f);
        if (!face.inner.empty() || face.outer.uses.size() != set.size()) {
            continue;
        }
        if (std::all_of(face.outer.uses.begin(), face.outer.uses.end(), [&](const EdgeUse& u) { return set.contains(u.edge); })) {
            return f;
        }
    }
    return {};
}

std::optional<ShapeInfo> curveShape(const Mesh& mesh, const std::vector<EdgeId>& edges, bool smooth)
{
    ShapeInfo s;
    bool closed = false;
    s.vertices = orderChain(mesh, edges, closed);
    if (s.vertices.size() < 2) {
        return std::nullopt;
    }
    s.edges = edges;
    std::vector<Vec3d> p;
    for (VertexId v : s.vertices) {
        p.push_back(mesh.position(v));
    }
    for (EdgeId e : edges) {
        s.length += glm::length(mesh.position(mesh.edge(e).v1) - mesh.position(mesh.edge(e).v0));
    }
    s.segments = static_cast<int>(edges.size());
    if (closed) {
        if (p.size() < 3) {
            return std::nullopt;
        }
        Vec3d c { 0.0 };
        for (const Vec3d& q : p) {
            c += q;
        }
        c /= static_cast<double>(p.size());
        s.center = c;
        s.radius = glm::length(p.front() - c);
        // Regulat: toate vârfurile la aceeași distanță de centru.
        for (const Vec3d& q : p) {
            if (std::abs(glm::length(q - c) - s.radius) > std::max(tol::kLength, s.radius * 1e-4)) {
                return std::nullopt;
            }
        }
        s.normal = glm::normalize(glm::cross(p[0] - c, p[1] - c));
        s.kind = smooth ? ShapeInfo::Kind::Circle : ShapeInfo::Kind::Polygon;
        s.face = faceOf(mesh, edges);
    } else {
        if (p.size() < 3) {
            return std::nullopt;
        }
        s.center = circumcenter(p.front(), p[p.size() / 2], p.back());
        s.radius = glm::length(p.front() - s.center);
        const Vec3d n = glm::cross(p[0] - s.center, p[1] - s.center);
        if (glm::length(n) < 1e-12) {
            return std::nullopt;
        }
        s.normal = glm::normalize(n);
        double total = 0.0;
        for (std::size_t i = 0; i + 1 < p.size(); ++i) {
            const Vec3d a = glm::normalize(p[i] - s.center);
            const Vec3d b = glm::normalize(p[i + 1] - s.center);
            total += std::atan2(glm::dot(glm::cross(a, b), s.normal), glm::dot(a, b));
        }
        s.angle = glm::degrees(std::abs(total));
        s.kind = ShapeInfo::Kind::Arc;
    }
    // Segmentele: doar o formă plană de sine stătătoare (muchiile au cel mult fața formei).
    s.segmentsEditable = std::all_of(edges.begin(), edges.end(), [&](EdgeId e) {
        const Edge& edge = mesh.edge(e);
        return edge.faces.empty() || (edge.faces.size() == 1 && edge.faces.front() == s.face && s.face);
    });
    return s;
}

} // namespace

std::optional<ShapeInfo> recognizeShape(const Mesh& mesh, const Selection& selection)
{
    if (!selection.instances.empty()) {
        return std::nullopt;
    }
    // O față: muchiile buclei ei exterioare (cerc, poligon, dreptunghi).
    std::vector<EdgeId> edges;
    FaceId face;
    if (selection.faces.size() == 1) {
        face = *selection.faces.begin();
        const Face* f = mesh.faces().find(face);
        if (f == nullptr || !f->inner.empty()) {
            return std::nullopt;
        }
        for (const EdgeUse& u : f->outer.uses) {
            edges.push_back(u.edge);
        }
        // Muchiile selectate în plus trebuie să fie tot ale feței (un dublu-click selectează fața cu marginea ei).
        for (EdgeId e : selection.edges) {
            if (std::find(edges.begin(), edges.end(), e) == edges.end()) {
                return std::nullopt;
            }
        }
    } else if (selection.faces.empty() && !selection.edges.empty()) {
        edges.assign(selection.edges.begin(), selection.edges.end());
    } else {
        return std::nullopt;
    }
    for (EdgeId e : edges) {
        if (!mesh.edges().contains(e)) {
            return std::nullopt;
        }
    }

    // O singură muchie dreaptă: o linie.
    const std::uint32_t curve = mesh.edge(edges.front()).curve;
    if (edges.size() == 1 && curve == 0) {
        ShapeInfo s;
        s.kind = ShapeInfo::Kind::Line;
        s.edges = edges;
        s.vertices = { mesh.edge(edges[0]).v0, mesh.edge(edges[0]).v1 };
        s.length = glm::length(mesh.position(s.vertices[1]) - mesh.position(s.vertices[0]));
        s.center = (mesh.position(s.vertices[0]) + mesh.position(s.vertices[1])) * 0.5;
        return s;
    }
    // O curbă întreagă (cerc, arc, poligon): toate muchiile au același identificator.
    if (curve != 0 && std::all_of(edges.begin(), edges.end(), [&](EdgeId e) { return mesh.edge(e).curve == curve; })) {
        auto s = curveShape(mesh, edges, mesh.edge(edges.front()).smoothCurve);
        if (s && face && s->face != face) {
            return std::nullopt;
        }
        return s;
    }
    // Un dreptunghi: 4 muchii drepte, unghiuri drepte.
    if (edges.size() == 4) {
        bool closed = false;
        std::vector<VertexId> v = orderChain(mesh, edges, closed);
        if (!closed || v.size() != 4) {
            return std::nullopt;
        }
        // Lățimea = latura „orizontală” (cea mai puțin înclinată pe Z; la egalitate, cea mai apropiată de X).
        const auto horizontal = [](const Vec3d& d) {
            const Vec3d n = glm::normalize(d);
            return std::pair { -std::abs(n.z), std::abs(n.x) };
        };
        if (horizontal(mesh.position(v[3]) - mesh.position(v[0])) > horizontal(mesh.position(v[1]) - mesh.position(v[0]))) {
            v = { v[0], v[3], v[2], v[1] };
        }
        const Vec3d p0 = mesh.position(v[0]), p1 = mesh.position(v[1]), p2 = mesh.position(v[2]), p3 = mesh.position(v[3]);
        const Vec3d a = p1 - p0, b = p3 - p0;
        if (std::abs(glm::dot(glm::normalize(a), glm::normalize(b))) > 1e-6 || glm::length(p2 - (p1 + b)) > tol::kLength) {
            return std::nullopt;
        }
        ShapeInfo s;
        s.kind = ShapeInfo::Kind::Rectangle;
        s.vertices = v;
        s.edges = edges;
        s.width = glm::length(a);
        s.height = glm::length(b);
        s.center = (p0 + p2) * 0.5;
        s.normal = glm::normalize(glm::cross(a, b));
        s.face = faceOf(mesh, edges);
        return s;
    }
    return std::nullopt;
}

bool setLineLength(Mesh& mesh, const ShapeInfo& shape, double length)
{
    if (shape.kind != ShapeInfo::Kind::Line || length <= tol::kLength) {
        return false;
    }
    const Vec3d a = mesh.position(shape.vertices[0]);
    const Vec3d b = mesh.position(shape.vertices[1]);
    const Vec3d dir = glm::normalize(b - a);
    return transformVertices(mesh, { shape.vertices[1] }, math::Transform::translation(dir * (length - glm::length(b - a))));
}

bool setShapeRadius(Mesh& mesh, const ShapeInfo& shape, double radius)
{
    if (shape.radius <= tol::kLength || radius <= tol::kLength
        || (shape.kind != ShapeInfo::Kind::Arc && shape.kind != ShapeInfo::Kind::Circle && shape.kind != ShapeInfo::Kind::Polygon)) {
        return false;
    }
    const double k = radius / shape.radius;
    const std::set<VertexId> vertices(shape.vertices.begin(), shape.vertices.end());
    return transformVertices(mesh, vertices, math::Transform::scaling(shape.center, Vec3d { k }));
}

bool setRectangleSize(Mesh& mesh, const ShapeInfo& shape, double width, double height)
{
    if (shape.kind != ShapeInfo::Kind::Rectangle || width <= tol::kLength || height <= tol::kLength) {
        return false;
    }
    const auto& v = shape.vertices;
    const Vec3d p0 = mesh.position(v[0]);
    const Vec3d u = glm::normalize(mesh.position(v[1]) - p0);
    const Vec3d w = glm::normalize(mesh.position(v[3]) - p0);
    bool ok = true;
    if (std::abs(width - shape.width) > tol::kLength) {
        ok = transformVertices(mesh, { v[1], v[2] }, math::Transform::translation(u * (width - shape.width))) && ok;
    }
    if (std::abs(height - shape.height) > tol::kLength) {
        ok = transformVertices(mesh, { v[2], v[3] }, math::Transform::translation(w * (height - shape.height))) && ok;
    }
    return ok;
}

std::optional<Selection> setShapeSegments(Mesh& mesh, const ShapeInfo& shape, int segments)
{
    const bool closed = shape.kind == ShapeInfo::Kind::Circle || shape.kind == ShapeInfo::Kind::Polygon;
    if (!shape.segmentsEditable || (!closed && shape.kind != ShapeInfo::Kind::Arc) || segments < (closed ? 3 : 1)
        || segments > 720 || segments == shape.segments) {
        return std::nullopt;
    }
    const Vec3d first = mesh.position(shape.vertices.front());
    const Vec3d u = glm::normalize(first - shape.center);
    Vec3d v = glm::cross(shape.normal, u);
    // Sensul curbei: spre al doilea vârf.
    if (glm::dot(mesh.position(shape.vertices[1]) - shape.center, v) < 0.0) {
        v = -v;
    }
    const double total = closed ? 2.0 * std::numbers::pi : glm::radians(shape.angle);
    std::vector<Vec3d> points;
    const int count = closed ? segments : segments + 1;
    for (int k = 0; k < count; ++k) {
        const double a = total * k / segments;
        points.push_back(shape.center + (u * std::cos(a) + v * std::sin(a)) * shape.radius);
    }
    // Materialele feței (dacă există) trec pe fața nouă.
    model::MaterialId material, backMaterial;
    if (shape.face && mesh.faces().contains(shape.face)) {
        material = mesh.face(shape.face).material;
        backMaterial = mesh.face(shape.face).backMaterial;
    }
    const bool smooth = mesh.edge(shape.edges.front()).smoothCurve;
    for (EdgeId e : shape.edges) {
        if (mesh.edges().contains(e)) {
            eraseEdge(mesh, e);
        }
    }
    const DrawLineResult drawn = drawCurve(mesh, points, closed, smooth);
    Selection out;
    out.edges.insert(drawn.edges.begin(), drawn.edges.end());
    for (FaceId f : drawn.createdFaces) {
        mesh.setFaceMaterial(f, material);
        mesh.setFaceBackMaterial(f, backMaterial);
        if (shape.face) {
            out.faces.insert(f);
        }
    }
    return out;
}

} // namespace aion::geom
