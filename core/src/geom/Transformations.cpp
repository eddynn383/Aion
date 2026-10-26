#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <map>

namespace aion::geom {

namespace {

using math::Plane;
using math::Vec2d;

constexpr double kTol = tol::kLength;

bool planar(const std::vector<Vec3d>& points, const Plane& plane)
{
    return std::all_of(
        points.begin(), points.end(), [&](const Vec3d& p) { return std::abs(plane.signedDistance(p)) <= kTol; });
}

// O față care nu mai e plană devine triunghiuri: diagonalele sunt muchii noi.
void foldFace(Mesh& mesh, FaceId id)
{
    const Face face = mesh.face(id);
    std::vector<VertexId> ids = mesh.loopVertices(face.outer);
    std::vector<Vec3d> outer = mesh.loopPoints(face.outer);
    const auto best = Plane::fromPolygon(outer).value_or(face.plane);
    const math::PlaneFrame frame(best);

    std::vector<std::vector<Vec2d>> rings(1);
    for (const Vec3d& p : outer) {
        rings[0].push_back(frame.toLocal(p));
    }
    for (const Loop& h : face.inner) {
        std::vector<Vec2d> ring;
        for (const Vec3d& p : mesh.loopPoints(h)) {
            ring.push_back(frame.toLocal(p));
        }
        rings.push_back(std::move(ring));
        const auto hv = mesh.loopVertices(h);
        ids.insert(ids.end(), hv.begin(), hv.end());
    }
    const std::vector<std::uint32_t> tris = triangulatePolygon(rings);

    mesh.removeFace(id);
    const auto edgeUse = [&](VertexId a, VertexId b) {
        EdgeId e = mesh.findEdge(a, b);
        if (!e) {
            e = mesh.addEdge(a, b);
        }
        return EdgeUse { e, mesh.edge(e).v0 != a };
    };
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        VertexId a = ids[tris[i]];
        VertexId b = ids[tris[i + 1]];
        VertexId c = ids[tris[i + 2]];
        auto plane = Plane::fromPoints(mesh.position(a), mesh.position(b), mesh.position(c));
        if (!plane) {
            continue; // triunghi degenerat (puncte coliniare): îl sărim
        }
        // Triunghiurile păstrează partea feței inițiale.
        if (glm::dot(plane->normal, best.normal) < 0.0) {
            std::swap(b, c);
            plane = plane->flipped();
        }
        Loop loop { { edgeUse(a, b), edgeUse(b, c), edgeUse(c, a) } };
        mesh.addFace(*plane, std::move(loop));
    }
}

} // namespace

Vec3d faceInteriorPoint(const Mesh& mesh, FaceId face)
{
    const FaceTriangles tri = triangulateFace(mesh, face);
    Vec3d best = tri.points.empty() ? Vec3d { 0.0 } : tri.points.front();
    double bestArea = -1.0;
    for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
        const Vec3d a = tri.points[tri.indices[i]];
        const Vec3d b = tri.points[tri.indices[i + 1]];
        const Vec3d c = tri.points[tri.indices[i + 2]];
        const double area = glm::length(glm::cross(b - a, c - a));
        if (area > bestArea) {
            bestArea = area;
            best = (a + b + c) / 3.0;
        }
    }
    return best;
}

std::set<VertexId> verticesOf(const Mesh& mesh, const Selection& selection)
{
    std::set<VertexId> out;
    for (EdgeId e : selection.edges) {
        if (const Edge* edge = mesh.edges().find(e)) {
            out.insert(edge->v0);
            out.insert(edge->v1);
        }
    }
    for (FaceId f : selection.faces) {
        if (const Face* face = mesh.faces().find(f)) {
            for (VertexId v : mesh.loopVertices(face->outer)) {
                out.insert(v);
            }
            for (const Loop& h : face->inner) {
                for (VertexId v : mesh.loopVertices(h)) {
                    out.insert(v);
                }
            }
        }
    }
    return out;
}

bool transformVertices(Mesh& mesh, const std::set<VertexId>& vertices, const math::Transform& transform)
{
    if (vertices.empty() || transform.isIdentity()) {
        return false;
    }
    std::map<VertexId, Vec3d> moved;
    for (VertexId v : vertices) {
        moved[v] = transform.applyToPoint(mesh.position(v));
    }
    const auto at = [&](VertexId v) {
        const auto it = moved.find(v);
        return it != moved.end() ? it->second : mesh.position(v);
    };

    // O muchie nu are voie să devină un punct.
    std::set<FaceId> faces;
    for (VertexId v : vertices) {
        for (EdgeId e : mesh.vertex(v).edges) {
            const Edge& edge = mesh.edge(e);
            if (tol::samePoint(at(edge.v0), at(edge.v1))) {
                return false;
            }
            faces.insert(edge.faces.begin(), edge.faces.end());
        }
    }

    for (const auto& [v, p] : moved) {
        mesh.setPosition(v, p);
    }
    for (FaceId f : faces) {
        const Face& face = mesh.face(f);
        std::vector<Vec3d> points = mesh.loopPoints(face.outer);
        const auto plane = Plane::fromPolygon(points);
        for (const Loop& h : face.inner) {
            const auto hp = mesh.loopPoints(h);
            points.insert(points.end(), hp.begin(), hp.end());
        }
        // Bucla exterioară dă sensul normalei (Newell), deci orientarea rămâne cea a buclelor.
        if (plane && planar(points, *plane)) {
            mesh.setFacePlane(f, *plane);
        } else {
            foldFace(mesh, f);
        }
    }
    return true;
}

Selection copySelection(Mesh& mesh, const Selection& selection, const math::Transform& transform)
{
    // Tot ce se copiază, citit înainte ca desenul să schimbe modelul.
    std::set<EdgeId> edges(selection.edges.begin(), selection.edges.end());
    struct FaceCopy {
        Vec3d sample;
        Vec3d normal;
    };
    std::vector<FaceCopy> faces;
    for (FaceId f : selection.faces) {
        const Face* face = mesh.faces().find(f);
        if (face == nullptr) {
            continue;
        }
        for (const Loop* loop : { &face->outer }) {
            for (const EdgeUse& u : loop->uses) {
                edges.insert(u.edge);
            }
        }
        for (const Loop& h : face->inner) {
            for (const EdgeUse& u : h.uses) {
                edges.insert(u.edge);
            }
        }
        faces.push_back({ transform.applyToPoint(faceInteriorPoint(mesh, f)),
            glm::normalize(transform.applyToNormal(face->plane.normal)) });
    }
    struct Segment {
        Vec3d a;
        Vec3d b;
        Edge style;
    };
    std::vector<Segment> segments;
    std::map<std::uint32_t, std::uint32_t> curves; // curba sursă → curba copiei
    std::uint32_t nextCurve = mesh.newCurveId();
    for (EdgeId e : edges) {
        if (const Edge* edge = mesh.edges().find(e)) {
            Edge style = *edge;
            if (style.curve != 0) {
                const auto [it, inserted] = curves.try_emplace(style.curve, nextCurve);
                nextCurve += inserted ? 1 : 0;
                style.curve = it->second;
            }
            segments.push_back(
                { transform.applyToPoint(mesh.position(edge->v0)), transform.applyToPoint(mesh.position(edge->v1)), style });
        }
    }

    for (const Segment& s : segments) {
        drawLine(mesh, s.a, s.b);
    }
    // Muchiile copiei: cele aflate pe segmentele desenate (unele s-au putut împărți între timp).
    Selection out;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const Edge& e = mesh.edges().values()[i];
        const Vec3d p0 = mesh.position(e.v0);
        const Vec3d p1 = mesh.position(e.v1);
        for (const Segment& s : segments) {
            if (math::projectOnSegment(p0, { s.a, s.b }).distance <= kTol
                && math::projectOnSegment(p1, { s.a, s.b }).distance <= kTol) {
                out.edges.insert(mesh.edges().keys()[i]);
                mesh.setEdgeStyle(mesh.edges().keys()[i], s.style.soft, s.style.curve, s.style.smoothCurve);
                break;
            }
        }
    }
    for (const FaceCopy& copy : faces) {
        for (FaceId f : mesh.faces().keys()) {
            if (faceContainsPoint(mesh, f, copy.sample)) {
                out.faces.insert(orientFace(mesh, f, copy.normal));
                break;
            }
        }
    }
    out.prune(mesh);
    return out;
}

} // namespace aion::geom
