#include "aion/core/geom/Operations.h"
#include "aion/core/geom/PlanarRegions.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <set>

namespace aion::geom {

namespace {

using math::Plane;
using math::Vec2d;

constexpr double kTol = tol::kLength;

std::vector<FaceId> facesInPlane(const Mesh& mesh, const Plane& plane)
{
    std::vector<FaceId> out;
    for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
        if (mesh.faces().values()[i].plane.coincident(plane)) {
            out.push_back(mesh.faces().keys()[i]);
        }
    }
    return out;
}

bool insidePolygon(const std::vector<Vec3d>& polygon, const Plane& plane, const Vec3d& p)
{
    const math::PlaneFrame frame(plane);
    std::vector<Vec2d> poly;
    poly.reserve(polygon.size());
    for (const Vec3d& q : polygon) {
        poly.push_back(frame.toLocal(q));
    }
    return math::locatePoint(frame.toLocal(p), poly, kTol) == math::PointLocation::Inside;
}

// Există o față (alta decât `except`) care conține punctul?
bool materialAt(const Mesh& mesh, const Vec3d& p, FaceId except = {})
{
    for (FaceId f : mesh.faces().keys()) {
        if (f != except && faceContainsPoint(mesh, f, p)) {
            return true;
        }
    }
    return false;
}

std::vector<Vec3d> offsetPoints(const std::vector<Vec3d>& points, const Vec3d& offset)
{
    std::vector<Vec3d> out;
    out.reserve(points.size());
    for (const Vec3d& p : points) {
        out.push_back(p + offset);
    }
    return out;
}

EdgeId edgeBetween(const Mesh& mesh, const Vec3d& a, const Vec3d& b)
{
    const VertexId va = mesh.findVertexNear(a, kTol);
    const VertexId vb = mesh.findVertexNear(b, kTol);
    return va && vb && va != vb ? mesh.findEdge(va, vb) : EdgeId {};
}

void applyStyles(Mesh& mesh, const std::vector<std::vector<Vec3d>>& loops,
    const std::vector<std::vector<Edge>>& styles, const Vec3d& offset)
{
    std::map<std::uint32_t, std::uint32_t> curves; // curba sursă → curba capacului
    std::uint32_t next = mesh.newCurveId();
    for (std::size_t l = 0; l < loops.size(); ++l) {
        const auto& pts = loops[l];
        const std::size_t n = pts.size();
        for (std::size_t i = 0; i < n; ++i) {
            const Edge& style = styles[l][i];
            const Edge& previous = styles[l][(i + n - 1) % n];
            if (const EdgeId cap = edgeBetween(mesh, pts[i] + offset, pts[(i + 1) % n] + offset)) {
                std::uint32_t curve = 0;
                if (style.curve != 0) {
                    const auto [it, inserted] = curves.try_emplace(style.curve, next);
                    next += inserted ? 1 : 0;
                    curve = it->second;
                }
                mesh.setEdgeStyle(cap, style.soft, curve, style.smoothCurve);
            }
            const bool smooth = style.curve != 0 && style.curve == previous.curve && style.smoothCurve;
            if (smooth) {
                if (const EdgeId lateral = edgeBetween(mesh, pts[i], pts[i] + offset)) {
                    const Edge& e = mesh.edge(lateral);
                    mesh.setEdgeStyle(lateral, true, e.curve, e.smoothCurve);
                }
            }
        }
    }
}

} // namespace

FaceId orientFace(Mesh& mesh, FaceId id, const Vec3d& wanted)
{
    const Face face = mesh.face(id);
    if (glm::dot(face.plane.normal, wanted) >= 0.0) {
        return id;
    }
    std::vector<Loop> inner;
    for (const Loop& h : face.inner) {
        inner.push_back(reversed(h));
    }
    mesh.removeFace(id);
    return mesh.addFace(face.plane.flipped(), reversed(face.outer), std::move(inner));
}

FaceId mergeFacesAcross(Mesh& mesh, EdgeId id)
{
    const Edge edge = mesh.edge(id);
    if (edge.faces.size() != 2 || edge.faces[0] == edge.faces[1]) {
        return {};
    }
    const Face& f0 = mesh.face(edge.faces[0]);
    const Face& f1 = mesh.face(edge.faces[1]);
    if (!f0.plane.coincident(f1.plane) || glm::dot(f0.plane.normal, f1.plane.normal) < 0.0) {
        return {};
    }
    const Plane plane = f0.plane;

    // Regiunea care conține mijlocul muchiei, calculată fără muchie (înainte de orice modificare).
    std::vector<EdgeId> coplanar;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const EdgeId e = mesh.edges().keys()[i];
        const Edge& ed = mesh.edges().values()[i];
        if (e != id && std::abs(plane.signedDistance(mesh.position(ed.v0))) <= kTol
            && std::abs(plane.signedDistance(mesh.position(ed.v1))) <= kTol) {
            coplanar.push_back(e);
        }
    }
    const Vec3d mid = (mesh.position(edge.v0) + mesh.position(edge.v1)) * 0.5;
    const math::PlaneFrame frame(plane);
    const Vec2d sample = frame.toLocal(mid);
    std::optional<PlanarRegion> merged;
    for (const PlanarRegion& region : findPlanarRegions(mesh, plane, coplanar)) {
        const auto locate = [&](const Loop& loop) {
            std::vector<Vec2d> poly;
            for (const Vec3d& p : mesh.loopPoints(loop)) {
                poly.push_back(frame.toLocal(p));
            }
            return math::locatePoint(sample, poly, kTol);
        };
        if (locate(region.outer) != math::PointLocation::Inside) {
            continue;
        }
        if (std::any_of(region.holes.begin(), region.holes.end(),
                [&](const Loop& h) { return locate(h) != math::PointLocation::Outside; })) {
            continue;
        }
        merged = region;
        break;
    }
    if (!merged) {
        return {};
    }

    mesh.removeFace(edge.faces[0]);
    mesh.removeFace(edge.faces[1]);
    mesh.removeEdge(id);
    for (VertexId v : { edge.v0, edge.v1 }) {
        if (mesh.vertex(v).edges.empty()) {
            mesh.removeVertex(v);
        }
    }
    // Regiunile sunt în sens trigonometric față de normala planului, deci au deja orientarea fețelor.
    return mesh.addFace(plane, merged->outer, merged->holes);
}

bool dissolveVertex(Mesh& mesh, VertexId v)
{
    const Vertex vertex = mesh.vertex(v);
    if (vertex.edges.size() != 2) {
        return false;
    }
    const EdgeId e1 = vertex.edges[0];
    const EdgeId e2 = vertex.edges[1];
    const VertexId u = mesh.edge(e1).other(v);
    const VertexId w = mesh.edge(e2).other(v);
    if (u == w || mesh.findEdge(u, w)) {
        return false;
    }
    const Vec3d d1 = glm::normalize(vertex.position - mesh.position(u));
    const Vec3d d2 = glm::normalize(mesh.position(w) - vertex.position);
    if (glm::dot(d1, d2) <= 0.0 || !tol::parallel(d1, d2, 1e-9)) {
        return false;
    }
    std::vector<FaceId> faces1 = mesh.edge(e1).faces;
    std::vector<FaceId> faces2 = mesh.edge(e2).faces;
    std::sort(faces1.begin(), faces1.end());
    std::sort(faces2.begin(), faces2.end());
    if (faces1 != faces2 || faces1.empty()) {
        return false;
    }
    faces1.erase(std::unique(faces1.begin(), faces1.end()), faces1.end());

    // Buclele noi: perechea (e1, e2) devine o singură utilizare a muchiei noi. Le pregătim cât timp
    // muchiile vechi încă există; muchia nouă e marcată temporar cu un ID nul.
    struct Planned {
        Face face;
        std::vector<std::vector<std::pair<EdgeUse, VertexId>>> loops; // (utilizare, început)
    };
    const auto plan = [&](const Loop& loop) {
        std::vector<std::pair<EdgeUse, VertexId>> out;
        const std::size_t n = loop.uses.size();
        std::size_t k = 0;
        for (; k < n; ++k) {
            const EdgeId a = loop.uses[k].edge;
            const EdgeId b = loop.uses[(k + 1) % n].edge;
            if ((a == e1 && b == e2) || (a == e2 && b == e1)) {
                break;
            }
        }
        if (k == n) {
            for (const EdgeUse& use : loop.uses) {
                out.emplace_back(use, VertexId {});
            }
            return out;
        }
        out.emplace_back(EdgeUse {}, mesh.useStart(loop.uses[k])); // muchia nouă
        for (std::size_t j = 2; j < n; ++j) {
            out.emplace_back(loop.uses[(k + j) % n], VertexId {});
        }
        return out;
    };
    std::vector<Planned> planned;
    for (FaceId f : faces1) {
        Planned p { mesh.face(f), {} };
        p.loops.push_back(plan(p.face.outer));
        for (const Loop& h : p.face.inner) {
            p.loops.push_back(plan(h));
        }
        planned.push_back(std::move(p));
    }

    const Edge style = mesh.edge(e1);
    for (FaceId f : faces1) {
        mesh.removeFace(f);
    }
    mesh.removeEdge(e1);
    mesh.removeEdge(e2);
    mesh.removeVertex(v);
    const EdgeId merged = mesh.addEdge(u, w);
    mesh.setEdgeStyle(merged, style.soft, style.curve, style.smoothCurve);
    const auto build = [&](const std::vector<std::pair<EdgeUse, VertexId>>& uses) {
        Loop loop;
        for (const auto& [use, start] : uses) {
            loop.uses.push_back(use.edge ? use : EdgeUse { merged, start != mesh.edge(merged).v0 });
        }
        return loop;
    };
    for (const Planned& p : planned) {
        Loop outer = build(p.loops.front());
        std::vector<Loop> inner;
        for (std::size_t i = 1; i < p.loops.size(); ++i) {
            inner.push_back(build(p.loops[i]));
        }
        mesh.addFace(p.face.plane, std::move(outer), std::move(inner));
    }
    return true;
}

PushPullResult pushPull(Mesh& mesh, FaceId faceId, double distance, bool keepOriginal)
{
    PushPullResult result;
    if (!mesh.faces().contains(faceId) || std::abs(distance) <= kTol) {
        return result;
    }
    const Face face = mesh.face(faceId);
    const Vec3d n = face.plane.normal;
    const Vec3d offset = n * distance;

    std::vector<std::vector<Vec3d>> loops { mesh.loopPoints(face.outer) };
    for (const Loop& h : face.inner) {
        loops.push_back(mesh.loopPoints(h));
    }
    // Stilul muchiilor fiecărei bucle (muchia i merge de la punctul i la i + 1): capacul îl moștenește,
    // iar laturile dintre două segmente ale aceleiași curbe netede devin soft (un cilindru e neted).
    std::vector<std::vector<Edge>> loopStyles;
    for (const Loop* loop : { &face.outer }) {
        std::vector<Edge> styles;
        for (const EdgeUse& u : loop->uses) {
            styles.push_back(mesh.edge(u.edge));
        }
        loopStyles.push_back(std::move(styles));
    }
    for (const Loop& h : face.inner) {
        std::vector<Edge> styles;
        for (const EdgeUse& u : h.uses) {
            styles.push_back(mesh.edge(u.edge));
        }
        loopStyles.push_back(std::move(styles));
    }

    // Fața e „liberă” dacă nicio muchie a ei nu are alte fețe.
    bool standalone = true;
    for (const Loop* loop : { &face.outer }) {
        for (const EdgeUse& u : loop->uses) {
            standalone = standalone && mesh.edge(u.edge).faces.size() == 1;
        }
    }
    for (const Loop& h : face.inner) {
        for (const EdgeUse& u : h.uses) {
            standalone = standalone && mesh.edge(u.edge).faces.size() == 1;
        }
    }
    const bool keep = keepOriginal || standalone;
    // Adaugă material (extrudare) sau îl scoate (împingere în solid). Normala feței arată în afara solidului.
    const bool adding = distance > 0.0 || keep;
    const Vec3d capNormal = (distance > 0.0 || !adding) ? n : -n;

    // Starea dinainte, pentru curățenie.
    const std::set<FaceId> facesBefore(mesh.faces().keys().begin(), mesh.faces().keys().end());
    std::map<EdgeId, bool> edgeHadFaces;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        edgeHadFaces[mesh.edges().keys()[i]] = !mesh.edges().values()[i].faces.empty();
    }

    // Laturile: câte un patrulater pe fiecare muchie a fiecărei bucle. Dacă patrulaterul cade pe
    // material existent (o față coplanară), e o tăietură; altfel devine o față nouă.
    struct Side {
        std::vector<Vec3d> points;
        Plane plane;
        Vec3d outward;
        bool cut = false;
    };
    std::vector<Side> sides;
    for (const auto& loop : loops) {
        for (std::size_t i = 0; i < loop.size(); ++i) {
            const Vec3d a = loop[i];
            const Vec3d b = loop[(i + 1) % loop.size()];
            const Vec3d lateral = glm::normalize(glm::cross(b - a, n));
            Side side { { a, b, b + offset, a + offset }, Plane::fromPointNormal(a, lateral),
                adding ? lateral : -lateral, false };
            side.cut = materialAt(mesh, (a + b) * 0.5 + offset * 0.5, faceId);
            sides.push_back(std::move(side));
        }
    }

    // Capacul: dacă ajunge pe material existent (o față opusă), cele două se anulează.
    const Plane capPlane = Plane::fromPointNormal(loops.front().front() + offset, n);
    const bool capCut = materialAt(mesh, faceInteriorPoint(mesh, faceId) + offset, faceId);
    std::vector<std::vector<Vec3d>> capLoops;
    for (const auto& loop : loops) {
        capLoops.push_back(offsetPoints(loop, offset));
    }

    // Desenul: muchiile noi împart și unesc geometria ca orice linie desenată.
    for (const Side& side : sides) {
        drawPolygon(mesh, side.points);
    }
    for (const auto& loop : capLoops) {
        drawPolygon(mesh, loop);
    }

    // Laturile: tăieturile dispar, fețele noi primesc orientarea spre exteriorul solidului.
    for (const Side& side : sides) {
        for (FaceId f : facesInPlane(mesh, side.plane)) {
            if (!mesh.faces().contains(f) || !insidePolygon(side.points, side.plane, faceInteriorPoint(mesh, f))) {
                continue;
            }
            if (side.cut) {
                mesh.removeFace(f);
            } else {
                orientFace(mesh, f, side.outward);
            }
        }
    }

    // Capacul și eventualele fețe închise de găurile lui.
    for (FaceId f : facesInPlane(mesh, capPlane)) {
        const Vec3d s = faceInteriorPoint(mesh, f);
        if (!insidePolygon(capLoops.front(), capPlane, s)) {
            continue;
        }
        const bool inHole = std::any_of(capLoops.begin() + 1, capLoops.end(),
            [&](const std::vector<Vec3d>& h) { return insidePolygon(h, capPlane, s); });
        if (inHole) {
            if (!facesBefore.contains(f)) {
                mesh.removeFace(f);
            }
        } else if (capCut) {
            mesh.removeFace(f);
        } else {
            orientFace(mesh, f, capNormal);
        }
    }

    // Fața inițială: rămâne (ca bază, cu normala spre exterior) sau dispare.
    if (mesh.faces().contains(faceId)) {
        if (keep) {
            orientFace(mesh, faceId, distance > 0.0 ? -n : n);
        } else {
            mesh.removeFace(faceId);
        }
    }

    // Curățenie în jurul operației: muchii rămase fără fețe, fețe coplanare de unit, vârfuri de prisos.
    std::vector<Vec3d> touched;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        touched.insert(touched.end(), loops[i].begin(), loops[i].end());
        touched.insert(touched.end(), capLoops[i].begin(), capLoops[i].end());
    }
    const auto isTouched = [&](VertexId v) {
        const Vec3d& p = mesh.position(v);
        return std::any_of(touched.begin(), touched.end(), [&](const Vec3d& q) { return tol::samePoint(p, q); });
    };
    const auto touchedEdges = [&] {
        std::vector<EdgeId> out;
        for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
            const Edge& e = mesh.edges().values()[i];
            if (isTouched(e.v0) || isTouched(e.v1)) {
                out.push_back(mesh.edges().keys()[i]);
            }
        }
        return out;
    };

    for (EdgeId e : touchedEdges()) {
        if (!mesh.edges().contains(e) || !mesh.edge(e).faces.empty()) {
            continue;
        }
        const auto before = edgeHadFaces.find(e);
        if (before == edgeHadFaces.end() || before->second) {
            eraseEdge(mesh, e);
        }
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (EdgeId e : touchedEdges()) {
            if (mesh.edges().contains(e) && mergeFacesAcross(mesh, e)) {
                changed = true;
                break;
            }
        }
    }
    std::vector<VertexId> candidates;
    for (VertexId v : mesh.vertices().keys()) {
        if (isTouched(v)) {
            candidates.push_back(v);
        }
    }
    for (VertexId v : candidates) {
        if (mesh.vertices().contains(v)) {
            dissolveVertex(mesh, v);
        }
    }

    applyStyles(mesh, loops, loopStyles, offset);

    for (FaceId f : mesh.faces().keys()) {
        if (!facesBefore.contains(f)) {
            result.createdFaces.push_back(f);
        }
    }
    result.done = true;
    return result;
}

} // namespace aion::geom
