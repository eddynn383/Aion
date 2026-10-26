#include "aion/core/geom/Operations.h"

#include "aion/core/geom/PlanarRegions.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <map>
#include <set>

namespace aion::geom {

namespace {

using math::Plane;
using math::Segment;
using math::Vec2d;

constexpr double kTol = tol::kLength;

bool onPlane(const Mesh& mesh, VertexId v, const Plane& plane)
{
    return std::abs(plane.signedDistance(mesh.position(v))) <= kTol;
}

bool edgeOnPlane(const Mesh& mesh, EdgeId e, const Plane& plane)
{
    const Edge& edge = mesh.edge(e);
    return onPlane(mesh, edge.v0, plane) && onPlane(mesh, edge.v1, plane);
}

// Punctul se află în interiorul muchiei (pe ea, dar nu la capete)?
bool onEdgeInterior(const Mesh& mesh, EdgeId e, const Vec3d& p)
{
    const Edge& edge = mesh.edge(e);
    const Vec3d a = mesh.position(edge.v0);
    const Vec3d b = mesh.position(edge.v1);
    const auto hit = math::projectOnSegment(p, { a, b });
    return hit.distance <= kTol && glm::length(p - a) > kTol && glm::length(p - b) > kTol;
}

// Vârful din punctul p: unul existent (în toleranță) sau unul nou, care împarte muchia pe care cade.
VertexId obtainVertex(Mesh& mesh, const Vec3d& p)
{
    if (const VertexId existing = mesh.findVertexNear(p, kTol)) {
        return existing;
    }
    const VertexId v = mesh.addVertex(p);
    const std::vector<EdgeId> edges = mesh.edges().keys();
    for (EdgeId e : edges) {
        if (mesh.edges().contains(e) && onEdgeInterior(mesh, e, p)) {
            mesh.splitEdge(e, v);
            break; // muchiile nu se intersectează, deci punctul poate fi pe cel mult una
        }
    }
    return v;
}

std::vector<EdgeId> sortedEdges(const Loop& loop)
{
    std::vector<EdgeId> out;
    for (const EdgeUse& u : loop.uses) {
        out.push_back(u.edge);
    }
    std::sort(out.begin(), out.end());
    return out;
}

// Amprenta unei regiuni sau fețe: muchiile conturului și ale fiecărei găuri, ca mulțimi.
using Signature = std::pair<std::vector<EdgeId>, std::vector<std::vector<EdgeId>>>;

Signature signatureOf(const Loop& outer, const std::vector<Loop>& holes)
{
    Signature s { sortedEdges(outer), {} };
    for (const Loop& h : holes) {
        s.second.push_back(sortedEdges(h));
    }
    std::sort(s.second.begin(), s.second.end());
    return s;
}

std::vector<Vec2d> polygon2d(const Mesh& mesh, const Loop& loop, const math::PlaneFrame& frame)
{
    std::vector<Vec2d> out;
    for (const Vec3d& p : mesh.loopPoints(loop)) {
        out.push_back(frame.toLocal(p));
    }
    return out;
}

// Un punct sigur din interiorul regiunii: centrul celui mai mare triunghi din triangulare.
Vec2d interiorPoint(const Mesh& mesh, const PlanarRegion& region, const math::PlaneFrame& frame)
{
    std::vector<std::vector<Vec2d>> rings { polygon2d(mesh, region.outer, frame) };
    for (const Loop& h : region.holes) {
        rings.push_back(polygon2d(mesh, h, frame));
    }
    std::vector<Vec2d> flat;
    for (const auto& r : rings) {
        flat.insert(flat.end(), r.begin(), r.end());
    }
    const std::vector<std::uint32_t> tris = triangulatePolygon(rings);
    Vec2d best = flat.front();
    double bestArea = -1.0;
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const Vec2d a = flat[tris[i]];
        const Vec2d b = flat[tris[i + 1]];
        const Vec2d c = flat[tris[i + 2]];
        const double area = std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y));
        if (area > bestArea) {
            bestArea = area;
            best = (a + b + c) / 3.0;
        }
    }
    return best;
}

bool regionOfFaceContains(const Mesh& mesh, const Face& face, const math::PlaneFrame& frame, const Vec2d& p)
{
    if (math::locatePoint(p, polygon2d(mesh, face.outer, frame), kTol) != math::PointLocation::Inside) {
        return false;
    }
    for (const Loop& h : face.inner) {
        if (math::locatePoint(p, polygon2d(mesh, h, frame), kTol) != math::PointLocation::Outside) {
            return false;
        }
    }
    return true;
}

// Normala unei fețe noi fără „părinte”: −Z pentru fețele orizontale (ca în SketchUp),
// altfel sensul în care componenta dominantă e pozitivă.
Vec3d defaultNormal(const Vec3d& n)
{
    if (std::abs(n.z) >= 1.0 - 1e-9) {
        return { 0.0, 0.0, -1.0 };
    }
    const Vec3d a = glm::abs(n);
    const double dominant = a.x >= a.y && a.x >= a.z ? n.x : a.y >= a.z ? n.y : n.z;
    return dominant >= 0.0 ? n : -n;
}

// Recalculează fețele dintr-un plan după ce au apărut muchiile `newEdges` (vezi ADR 0003).
void refacePlane(Mesh& mesh, const Plane& plane, const std::set<EdgeId>& newEdges, DrawLineResult& result)
{
    std::vector<EdgeId> coplanar;
    for (EdgeId e : mesh.edges().keys()) {
        if (edgeOnPlane(mesh, e, plane)) {
            coplanar.push_back(e);
        }
    }
    const bool touchesNew = std::any_of(coplanar.begin(), coplanar.end(), [&](EdgeId e) { return newEdges.contains(e); });
    if (!touchesNew) {
        return;
    }

    std::vector<FaceId> existing;
    for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
        if (mesh.faces().values()[i].plane.coincident(plane)) {
            existing.push_back(mesh.faces().keys()[i]);
        }
    }

    const math::PlaneFrame frame(plane);
    const std::vector<PlanarRegion> regions = findPlanarRegions(mesh, plane, coplanar);

    struct NewFace {
        PlanarRegion region;
        FaceId source; // fața veche din care provine (pentru orientare)
    };
    std::vector<NewFace> toCreate;
    std::set<FaceId> matched;

    for (const PlanarRegion& region : regions) {
        const Signature sig = signatureOf(region.outer, region.holes);
        const auto same = std::find_if(existing.begin(), existing.end(), [&](FaceId f) {
            const Face& face = mesh.face(f);
            return signatureOf(face.outer, face.inner) == sig;
        });
        if (same != existing.end()) {
            matched.insert(*same);
            continue;
        }

        const Vec2d sample = interiorPoint(mesh, region, frame);
        FaceId source;
        for (FaceId f : existing) {
            if (regionOfFaceContains(mesh, mesh.face(f), frame, sample)) {
                source = f;
                break;
            }
        }
        const bool closesNewLoop = std::any_of(region.outer.uses.begin(), region.outer.uses.end(),
            [&](const EdgeUse& u) { return newEdges.contains(u.edge); });
        if (source || closesNewLoop) {
            toCreate.push_back({ region, source });
        }
    }

    // Fețele vechi care nu mai corespund unei regiuni au fost împărțite sau modificate.
    std::map<FaceId, Vec3d> sourceNormals;
    for (FaceId f : existing) {
        if (!matched.contains(f)) {
            sourceNormals[f] = mesh.face(f).plane.normal;
            mesh.removeFace(f);
            result.removedFaces.push_back(f);
        }
    }

    for (NewFace& nf : toCreate) {
        const Vec3d wanted = nf.source ? sourceNormals[nf.source] : defaultNormal(plane.normal);
        Loop outer = nf.region.outer;
        std::vector<Loop> holes = nf.region.holes;
        Plane facePlane = plane;
        if (glm::dot(wanted, plane.normal) < 0.0) {
            // Regiunile sunt în sens trigonometric față de normala planului; le întoarcem.
            facePlane = plane.flipped();
            outer = reversed(outer);
            for (Loop& h : holes) {
                h = reversed(h);
            }
        }
        result.createdFaces.push_back(mesh.addFace(facePlane, std::move(outer), std::move(holes)));
    }
}

// Planurile în care muchiile noi pot forma sau împărți fețe.
std::vector<Plane> candidatePlanes(const Mesh& mesh, const std::vector<EdgeId>& newEdges)
{
    std::vector<Plane> planes;
    const auto addPlane = [&](const Plane& p) {
        if (std::none_of(planes.begin(), planes.end(), [&](const Plane& q) { return q.coincident(p); })) {
            planes.push_back(p);
        }
    };
    for (EdgeId e : newEdges) {
        const Edge& edge = mesh.edge(e);
        const Vec3d a = mesh.position(edge.v0);
        const Vec3d b = mesh.position(edge.v1);
        const Vec3d mid = (a + b) * 0.5;

        // Fețele traversate de muchia nouă.
        for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
            const Face& face = mesh.faces().values()[i];
            if (edgeOnPlane(mesh, e, face.plane) && faceContainsPoint(mesh, mesh.faces().keys()[i], mid)) {
                addPlane(face.plane);
            }
        }
        // Planele formate cu muchiile vecine.
        for (VertexId v : { edge.v0, edge.v1 }) {
            for (EdgeId n : mesh.vertex(v).edges) {
                if (n == e) {
                    continue;
                }
                const Vec3d other = mesh.position(mesh.edge(n).other(v));
                if (auto p = Plane::fromPoints(a, b, other)) {
                    addPlane(*p);
                }
            }
        }
    }
    return planes;
}

} // namespace

bool faceContainsPoint(const Mesh& mesh, FaceId id, const Vec3d& point)
{
    const Face& face = mesh.face(id);
    if (std::abs(face.plane.signedDistance(point)) > kTol) {
        return false;
    }
    const math::PlaneFrame frame(face.plane);
    return regionOfFaceContains(mesh, face, frame, frame.toLocal(point));
}

DrawLineResult drawLine(Mesh& mesh, const Vec3d& a, const Vec3d& b)
{
    DrawLineResult result;
    if (glm::length(b - a) <= kTol) {
        return result;
    }

    const VertexId va = obtainVertex(mesh, a);
    const VertexId vb = obtainVertex(mesh, b);
    if (va == vb) {
        return result;
    }
    const Segment line { mesh.position(va), mesh.position(vb) };

    // Muchiile traversate de linie se împart în punctul de intersecție.
    const std::vector<EdgeId> snapshot = mesh.edges().keys();
    for (EdgeId e : snapshot) {
        if (!mesh.edges().contains(e)) {
            continue;
        }
        const Edge edge = mesh.edge(e);
        const Segment other { mesh.position(edge.v0), mesh.position(edge.v1) };
        // Contactul într-un capăt (al liniei sau al muchiei) e tratat prin vârfuri, mai jos.
        if (math::projectOnSegment(other.a, line).distance <= kTol
            || math::projectOnSegment(other.b, line).distance <= kTol
            || math::projectOnSegment(line.a, other).distance <= kTol
            || math::projectOnSegment(line.b, other).distance <= kTol) {
            continue;
        }
        const auto cp = math::closestPoints(line, other);
        if (cp.distance > kTol) {
            continue;
        }
        const VertexId crossing = mesh.addVertex((cp.onFirst + cp.onSecond) * 0.5);
        mesh.splitEdge(e, crossing);
    }

    // Toate vârfurile de pe linie, în ordine.
    std::vector<std::pair<double, VertexId>> along;
    for (std::size_t i = 0; i < mesh.vertices().size(); ++i) {
        const auto hit = math::projectOnSegment(mesh.vertices().values()[i].position, line);
        if (hit.distance <= kTol) {
            along.emplace_back(hit.t, mesh.vertices().keys()[i]);
        }
    }
    std::sort(along.begin(), along.end());

    std::vector<EdgeId> created;
    for (std::size_t i = 0; i + 1 < along.size(); ++i) {
        const VertexId u = along[i].second;
        const VertexId v = along[i + 1].second;
        if (u == v) {
            continue;
        }
        EdgeId e = mesh.findEdge(u, v);
        if (!e) {
            e = mesh.addEdge(u, v);
            created.push_back(e);
        }
        result.edges.push_back(e);
    }

    if (!created.empty()) {
        const std::set<EdgeId> newEdges(created.begin(), created.end());
        for (const Plane& plane : candidatePlanes(mesh, created)) {
            refacePlane(mesh, plane, newEdges, result);
        }
    }
    return result;
}

DrawLineResult drawPolygon(Mesh& mesh, const std::vector<Vec3d>& points)
{
    DrawLineResult total;
    for (std::size_t i = 0; i < points.size(); ++i) {
        DrawLineResult r = drawLine(mesh, points[i], points[(i + 1) % points.size()]);
        total.edges.insert(total.edges.end(), r.edges.begin(), r.edges.end());
        total.createdFaces.insert(total.createdFaces.end(), r.createdFaces.begin(), r.createdFaces.end());
        total.removedFaces.insert(total.removedFaces.end(), r.removedFaces.begin(), r.removedFaces.end());
    }
    // O față creată de un segment și ștearsă de unul ulterior nu mai e „creată”.
    std::erase_if(total.createdFaces, [&](FaceId f) { return !mesh.faces().contains(f); });
    return total;
}

void eraseFace(Mesh& mesh, FaceId face)
{
    mesh.removeFace(face);
}

void eraseEdge(Mesh& mesh, EdgeId id)
{
    const Edge edge = mesh.edge(id);
    for (FaceId f : edge.faces) {
        mesh.removeFace(f);
    }
    mesh.removeEdge(id);
    for (VertexId v : { edge.v0, edge.v1 }) {
        if (mesh.vertex(v).edges.empty()) {
            mesh.removeVertex(v);
        }
    }
}

} // namespace aion::geom
