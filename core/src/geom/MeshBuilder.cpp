#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace aion::geom {

namespace {

constexpr double kGrid = 0.01; // mm

std::uint64_t gridKey(const Vec3d& p)
{
    const auto q = [](double v) { return static_cast<std::uint64_t>(static_cast<std::int64_t>(std::llround(v / kGrid))) & 0x1FFFFF; };
    return q(p.x) | (q(p.y) << 21) | (q(p.z) << 42);
}

} // namespace

MeshBuilder::MeshBuilder(Mesh& mesh)
    : m_mesh(mesh)
{
    for (std::size_t i = 0; i < mesh.vertices().size(); ++i) {
        m_grid.emplace(gridKey(mesh.vertices().values()[i].position), mesh.vertices().keys()[i]);
    }
}

VertexId MeshBuilder::vertex(const Vec3d& position)
{
    const std::uint64_t key = gridKey(position);
    if (const auto it = m_grid.find(key); it != m_grid.end() && tol::samePoint(m_mesh.position(it->second), position, kGrid * 2)) {
        return it->second;
    }
    const VertexId v = m_mesh.addVertex(position);
    m_grid[key] = v;
    return v;
}

std::vector<FaceId> MeshBuilder::face(const std::vector<Vec3d>& input)
{
    // Vârfurile, fără repetări consecutive.
    std::vector<VertexId> ids;
    for (const Vec3d& p : input) {
        const VertexId v = vertex(p);
        if (ids.empty() || ids.back() != v) {
            ids.push_back(v);
        }
    }
    while (ids.size() > 1 && ids.front() == ids.back()) {
        ids.pop_back();
    }
    if (ids.size() < 3) {
        return {};
    }
    std::vector<Vec3d> points;
    for (VertexId v : ids) {
        points.push_back(m_mesh.position(v));
    }
    const auto plane = math::Plane::fromPolygon(points);
    if (!plane) {
        return {};
    }
    const bool planar = std::all_of(points.begin(), points.end(),
        [&](const Vec3d& p) { return std::abs(plane->signedDistance(p)) <= tol::kLength; });
    if (!planar) {
        std::vector<FaceId> out;
        for (std::size_t i = 1; i + 1 < points.size(); ++i) {
            const auto part = face({ points[0], points[i], points[i + 1] });
            out.insert(out.end(), part.begin(), part.end());
        }
        return out;
    }
    // O muchie nu are voie să apară de două ori în buclă (poligon care se atinge pe sine).
    Loop loop;
    std::vector<EdgeId> used;
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const VertexId a = ids[i];
        const VertexId b = ids[(i + 1) % ids.size()];
        EdgeId e = m_mesh.findEdge(a, b);
        if (!e) {
            e = m_mesh.addEdge(a, b);
        }
        if (std::find(used.begin(), used.end(), e) != used.end()) {
            return {};
        }
        used.push_back(e);
        loop.uses.push_back({ e, m_mesh.edge(e).v0 != a });
    }
    // Aceeași față de două ori (de exemplu fețe „double sided” din exporturi): o singură față.
    std::sort(used.begin(), used.end());
    for (FaceId f : m_mesh.edge(used.front()).faces) {
        std::vector<EdgeId> other;
        for (const EdgeUse& u : m_mesh.face(f).outer.uses) {
            other.push_back(u.edge);
        }
        std::sort(other.begin(), other.end());
        if (other == used && m_mesh.face(f).inner.empty()) {
            return {};
        }
    }
    return { m_mesh.addFace(*plane, std::move(loop)) };
}

void MeshBuilder::finish(double smoothAngle)
{
    const double cosSmooth = std::cos(smoothAngle);
    for (bool changed = true; changed;) {
        changed = false;
        for (EdgeId e : std::vector(m_mesh.edges().keys())) {
            if (!m_mesh.edges().contains(e)) {
                continue;
            }
            const Edge& edge = m_mesh.edge(e);
            if (edge.faces.size() != 2 || edge.faces[0] == edge.faces[1]) {
                continue;
            }
            const Vec3d n0 = m_mesh.face(edge.faces[0]).plane.normal;
            const Vec3d n1 = m_mesh.face(edge.faces[1]).plane.normal;
            if (m_mesh.face(edge.faces[0]).plane.coincident(m_mesh.face(edge.faces[1]).plane) && glm::dot(n0, n1) > 0.0) {
                if (mergeFacesAcross(m_mesh, e)) {
                    changed = true;
                }
            } else if (glm::dot(n0, n1) >= cosSmooth && !edge.soft) {
                m_mesh.setEdgeStyle(e, true, edge.curve, edge.smoothCurve);
            }
        }
    }
}

} // namespace aion::geom
