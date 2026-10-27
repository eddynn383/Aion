#include "aion/core/geom/Bevel.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace aion::geom {

namespace {

// Atributele unei fețe, păstrate când e refăcută.
struct FaceStyle {
    model::MaterialId material;
    model::MaterialId backMaterial;
    model::TagId tag;
    TextureTransform texture;
};

FaceStyle styleOf(const Face& f)
{
    return { f.material, f.backMaterial, f.tag, f.texture };
}

// O față nouă de desenat: poligonul, normala dorită (spre exterior) și stilul.
struct NewFace {
    std::vector<Vec3d> points;
    Vec3d normal { 0.0, 0.0, 1.0 };
    FaceStyle style;
};

// Direcția din muchie spre interiorul feței, în planul ei, perpendiculară pe muchie.
Vec3d inwardDirection(const Mesh& mesh, FaceId face, const Vec3d& a, const Vec3d& b)
{
    const Vec3d e = glm::normalize(b - a);
    Vec3d u = glm::normalize(glm::cross(mesh.face(face).plane.normal, e));
    // Spre punctul din interiorul feței.
    if (glm::dot(faceInteriorPoint(mesh, face) - a, u) < 0.0) {
        u = -u;
    }
    return u;
}

// Profilul secțiunii unei muchii (în jurul punctului `origin` de pe ea): punctele de la fața 1 (k = 0) la fața 2
// (k = segments), pe un arc tangent la ambele (sau o dreaptă, pentru chamfer). `u1`, `u2`: direcțiile spre fețe.
std::vector<Vec3d> profile(const Vec3d& origin, const Vec3d& u1, const Vec3d& u2, double distance, int segments)
{
    const double phi = std::acos(std::clamp(glm::dot(u1, u2), -1.0, 1.0)); // unghiul interior dintre fețe
    const Vec3d a = origin + u1 * distance;
    const Vec3d b = origin + u2 * distance;
    std::vector<Vec3d> out;
    if (segments <= 1) {
        return { a, b };
    }
    // Centrul cercului tangent la ambele fețe în a și b.
    const double r = distance * std::tan(phi / 2.0);
    const Vec3d bisector = glm::normalize(u1 + u2);
    const Vec3d c = origin + bisector * (r / std::sin(phi / 2.0));
    const Vec3d ra = a - c;
    const Vec3d rb = b - c;
    const double sweep = std::acos(std::clamp(glm::dot(glm::normalize(ra), glm::normalize(rb)), -1.0, 1.0));
    for (int k = 0; k <= segments; ++k) {
        const double t = static_cast<double>(k) / segments;
        // Slerp pe cerc (ra și rb au aceeași lungime r).
        const double s = std::sin(sweep);
        const Vec3d p = s < 1e-9 ? ra : (ra * std::sin((1.0 - t) * sweep) + rb * std::sin(t * sweep)) / s;
        out.push_back(c + p);
    }
    return out;
}

// Punctul de pe dreapta (p, dir) din planul dat; nullopt dacă dreapta e paralelă cu planul.
std::optional<Vec3d> onPlane(const Vec3d& p, const Vec3d& dir, const math::Plane& plane)
{
    const double denom = glm::dot(plane.normal, dir);
    if (std::abs(denom) < 1e-9) {
        return std::nullopt;
    }
    const double t = (plane.d - glm::dot(plane.normal, p)) / denom;
    return p + dir * t;
}

// Cele mai apropiate puncte ale două drepte (mijlocul lor).
Vec3d closestBetween(const Vec3d& p1, const Vec3d& d1, const Vec3d& p2, const Vec3d& d2)
{
    const Vec3d w = p1 - p2;
    const double a = glm::dot(d1, d1), b = glm::dot(d1, d2), c = glm::dot(d2, d2), d = glm::dot(d1, w), e = glm::dot(d2, w);
    const double den = a * c - b * b;
    if (std::abs(den) < 1e-12) {
        return (p1 + p2) * 0.5;
    }
    const double s = (b * e - c * d) / den;
    const double t = (a * e - b * d) / den;
    return (p1 + d1 * s + p2 + d2 * t) * 0.5;
}

// Bucla exterioară a feței, ca listă de vârfuri; înlocuiește vârfurile din `replace` cu secvențe de puncte.
std::vector<Vec3d> replacedLoop(const Mesh& mesh, FaceId face, const std::map<VertexId, std::vector<Vec3d>>& replace)
{
    std::vector<Vec3d> out;
    for (VertexId v : mesh.loopVertices(mesh.face(face).outer)) {
        if (const auto it = replace.find(v); it != replace.end()) {
            out.insert(out.end(), it->second.begin(), it->second.end());
        } else {
            out.push_back(mesh.position(v));
        }
    }
    return out;
}

// Ordinea punctelor care înlocuiesc vârful `v` în bucla feței: de la vecinul de dinainte spre cel de după.
// `first` = punctul care trebuie să fie lângă vecinul de pe muchia comună cu `firstFace`.
std::vector<Vec3d> orderedReplacement(const Mesh& mesh, FaceId face, VertexId v, FaceId firstFace, std::vector<Vec3d> points)
{
    const std::vector<VertexId> loop = mesh.loopVertices(mesh.face(face).outer);
    const auto it = std::find(loop.begin(), loop.end(), v);
    const VertexId prev = it == loop.begin() ? loop.back() : *(it - 1);
    const EdgeId shared = mesh.findEdge(prev, v);
    const bool prevOnFirst = shared && std::find(mesh.edge(shared).faces.begin(), mesh.edge(shared).faces.end(), firstFace)
        != mesh.edge(shared).faces.end();
    if (!prevOnFirst) {
        std::reverse(points.begin(), points.end());
    }
    return points;
}

// Șterge fețele, apoi muchiile rămase fără fețe din jurul vârfurilor date; desenează fețele noi și le dă stilul.
Selection rebuild(Mesh& mesh, const std::set<FaceId>& faces, const std::set<VertexId>& moved, const std::vector<NewFace>& added,
    const std::vector<std::pair<Vec3d, Vec3d>>& smoothEdges)
{
    for (FaceId f : faces) {
        eraseFace(mesh, f);
    }
    std::set<EdgeId> stale;
    for (VertexId v : moved) {
        if (!mesh.vertices().contains(v)) {
            continue;
        }
        for (EdgeId e : mesh.vertex(v).edges) {
            if (mesh.edge(e).faces.empty()) {
                stale.insert(e);
            }
        }
    }
    for (EdgeId e : stale) {
        if (mesh.edges().contains(e)) {
            eraseEdge(mesh, e);
        }
    }
    const std::vector<FaceId> before = mesh.faces().keys();
    const std::set<FaceId> existing(before.begin(), before.end());
    Selection out;
    for (const NewFace& face : added) {
        const DrawLineResult drawn = drawPolygon(mesh, face.points);
        // Fața poligonului: cea nouă care conține centrul lui.
        Vec3d centroid { 0.0 };
        for (const Vec3d& p : face.points) {
            centroid += p;
        }
        centroid /= static_cast<double>(face.points.size());
        for (FaceId f : mesh.faces().keys()) {
            if (faceContainsPoint(mesh, f, centroid) && std::abs(glm::dot(mesh.face(f).plane.normal, face.normal)) > 0.999) {
                const FaceId oriented = orientFace(mesh, f, face.normal);
                mesh.setFaceMaterial(oriented, face.style.material);
                mesh.setFaceBackMaterial(oriented, face.style.backMaterial);
                mesh.setFaceTag(oriented, face.style.tag);
                mesh.setFaceTexture(oriented, face.style.texture);
                out.faces.insert(oriented);
                break;
            }
        }
        (void)drawn;
    }
    // drawPolygon închide singur orice buclă plană; fețele pe care nu le-am cerut (de ex. inelul de muchii
    // coplanare de sub o față teșită) se scot, muchiile rămân.
    for (FaceId f : mesh.faces().keys()) {
        if (!existing.contains(f) && !out.faces.contains(f)) {
            eraseFace(mesh, f);
        }
    }
    // Muchiile dintre benzile unei rotunjiri (și cele tangente) sunt netede.
    for (const auto& [a, b] : smoothEdges) {
        const VertexId va = mesh.findVertexNear(a, tol::kLength * 10);
        const VertexId vb = mesh.findVertexNear(b, tol::kLength * 10);
        if (va && vb) {
            if (const EdgeId e = mesh.findEdge(va, vb)) {
                mesh.setEdgeStyle(e, true, 0, true);
            }
        }
    }
    out.prune(mesh);
    return out;
}

// Fața cealaltă a muchiei (exact două fețe).
std::optional<FaceId> otherFace(const Mesh& mesh, EdgeId e, FaceId face)
{
    const Edge& edge = mesh.edge(e);
    if (edge.faces.size() != 2) {
        return std::nullopt;
    }
    return edge.faces[0] == face ? edge.faces[1] : edge.faces[0];
}

} // namespace

BevelResult bevelEdges(Mesh& mesh, const std::set<EdgeId>& edges, double distance, int segments)
{
    BevelResult result;
    if (edges.empty() || distance <= tol::kLength) {
        result.error = "Alege o muchie și o distanță pozitivă.";
        return result;
    }
    std::set<VertexId> used;
    for (EdgeId e : edges) {
        if (!mesh.edges().contains(e)) {
            result.error = "Muchia nu mai există.";
            return result;
        }
        for (VertexId v : { mesh.edge(e).v0, mesh.edge(e).v1 }) {
            if (!used.insert(v).second) {
                result.error = "Muchiile alese au un vârf comun: alege muchii separate sau fața întreagă.";
                return result;
            }
        }
    }
    // Pozițiile capetelor, ca muchiile să poată fi regăsite după fiecare refacere.
    std::vector<std::pair<Vec3d, Vec3d>> ends;
    for (EdgeId e : edges) {
        ends.emplace_back(mesh.position(mesh.edge(e).v0), mesh.position(mesh.edge(e).v1));
    }
    for (const auto& [pa, pb] : ends) {
        const VertexId v0 = mesh.findVertexNear(pa, tol::kLength);
        const VertexId v1 = mesh.findVertexNear(pb, tol::kLength);
        const EdgeId e = v0 && v1 ? mesh.findEdge(v0, v1) : EdgeId {};
        if (!e || mesh.edge(e).faces.size() != 2) {
            result.error = "Muchia trebuie să fie între exact două fețe.";
            return result;
        }
        const FaceId f1 = mesh.edge(e).faces[0];
        const FaceId f2 = mesh.edge(e).faces[1];
        const Vec3d a = mesh.position(v0);
        const Vec3d b = mesh.position(v1);
        const Vec3d dir = glm::normalize(b - a);
        const Vec3d u1 = inwardDirection(mesh, f1, a, b);
        const Vec3d u2 = inwardDirection(mesh, f2, a, b);
        // Convexă: fața 2 „coboară” sub exteriorul feței 1.
        if (glm::dot(u2, mesh.face(f1).plane.normal) >= -1e-6) {
            result.error = "Muchia nu e convexă (un colț interior): bevel-ul merge doar pe muchii exterioare.";
            return result;
        }
        // Capetele: fața a treia de la fiecare capăt (vârf cu exact trei muchii).
        std::map<VertexId, FaceId> caps;
        for (VertexId v : { v0, v1 }) {
            if (mesh.vertex(v).edges.size() != 3) {
                result.error = "Capetele muchiei trebuie să aibă exact trei muchii (ca la colțul unei cutii).";
                return result;
            }
            std::set<FaceId> around;
            for (EdgeId ve : mesh.vertex(v).edges) {
                around.insert(mesh.edge(ve).faces.begin(), mesh.edge(ve).faces.end());
            }
            around.erase(f1);
            around.erase(f2);
            if (around.size() != 1) {
                result.error = "La capătul muchiei trebuie să fie o singură față în plus.";
                return result;
            }
            caps[v] = *around.begin();
        }
        for (FaceId f : { f1, f2, caps[v0], caps[v1] }) {
            if (!mesh.face(f).inner.empty()) {
                result.error = "Fețele din jurul muchiei au găuri: încă nu se pot teși.";
                return result;
            }
        }
        // Profilul (în jurul lui a), mutat de-a lungul muchiei până în planurile capetelor.
        const std::vector<Vec3d> prof = profile(a, u1, u2, distance, segments);
        std::vector<Vec3d> at0, at1;
        for (const Vec3d& p : prof) {
            const auto q0 = onPlane(p, dir, mesh.face(caps[v0]).plane);
            const auto q1 = onPlane(p, dir, mesh.face(caps[v1]).plane);
            if (!q0 || !q1) {
                result.error = "Un capăt al muchiei e paralel cu ea: nu se poate teși.";
                return result;
            }
            at0.push_back(*q0);
            at1.push_back(*q1);
        }
        if (glm::dot(at1.front() - at0.front(), dir) <= tol::kLength) {
            result.error = "Distanța e prea mare pentru muchia aleasă.";
            return result;
        }
        std::vector<NewFace> added;
        added.push_back({ replacedLoop(mesh, f1, { { v0, { at0.front() } }, { v1, { at1.front() } } }), mesh.face(f1).plane.normal,
            styleOf(mesh.face(f1)) });
        added.push_back({ replacedLoop(mesh, f2, { { v0, { at0.back() } }, { v1, { at1.back() } } }), mesh.face(f2).plane.normal,
            styleOf(mesh.face(f2)) });
        for (VertexId v : { v0, v1 }) {
            const FaceId cap = caps[v];
            const auto& pts = v == v0 ? at0 : at1;
            added.push_back({ replacedLoop(mesh, cap, { { v, orderedReplacement(mesh, cap, v, f1, pts) } }),
                mesh.face(cap).plane.normal, styleOf(mesh.face(cap)) });
        }
        std::vector<std::pair<Vec3d, Vec3d>> smooth;
        for (std::size_t k = 0; k + 1 < prof.size(); ++k) {
            const Vec3d mid = (at0[k] + at0[k + 1] + at1[k] + at1[k + 1]) * 0.25;
            Vec3d n = glm::normalize(glm::cross(at1[k] - at0[k], at0[k + 1] - at0[k]));
            if (glm::dot(n, mid - a) < 0.0) {
                n = -n;
            }
            added.push_back({ { at0[k], at1[k], at1[k + 1], at0[k + 1] }, n, styleOf(mesh.face(f1)) });
        }
        if (segments > 1) {
            for (std::size_t k = 0; k < prof.size(); ++k) {
                smooth.emplace_back(at0[k], at1[k]);
            }
        }
        const Selection sel = rebuild(mesh, { f1, f2, caps[v0], caps[v1] }, { v0, v1 }, added, smooth);
        result.selection.faces.insert(sel.faces.begin(), sel.faces.end());
    }
    return result;
}

BevelResult bevelFace(Mesh& mesh, FaceId face, double distance, int segments)
{
    BevelResult result;
    if (!mesh.faces().contains(face) || distance <= tol::kLength) {
        result.error = "Alege o față și o distanță pozitivă.";
        return result;
    }
    const Face& f = mesh.face(face);
    if (!f.inner.empty()) {
        result.error = "Fața are găuri: încă nu se poate teși.";
        return result;
    }
    const std::vector<VertexId> v = mesh.loopVertices(f.outer);
    const std::size_t m = v.size();
    std::vector<FaceId> sides(m);
    std::vector<std::vector<Vec3d>> prof(m); // profilul fiecărei muchii, în jurul vârfului ei de început
    std::vector<Vec3d> dirs(m);
    for (std::size_t i = 0; i < m; ++i) {
        const VertexId a = v[i];
        const VertexId b = v[(i + 1) % m];
        if (mesh.vertex(a).edges.size() != 3) {
            result.error = "Colțurile feței trebuie să aibă exact trei muchii (ca la o cutie sau o formă extrudată).";
            return result;
        }
        const EdgeId e = mesh.findEdge(a, b);
        const auto side = e ? otherFace(mesh, e, face) : std::nullopt;
        if (!side || !mesh.face(*side).inner.empty()) {
            result.error = "Fiecare latură a feței trebuie să aibă o singură față vecină, fără găuri (un volum).";
            return result;
        }
        sides[i] = *side;
        const Vec3d pa = mesh.position(a);
        const Vec3d pb = mesh.position(b);
        dirs[i] = glm::normalize(pb - pa);
        const Vec3d u1 = inwardDirection(mesh, face, pa, pb);
        const Vec3d u2 = inwardDirection(mesh, *side, pa, pb);
        if (glm::dot(u2, f.plane.normal) >= -1e-6) {
            result.error = "O latură a feței nu e convexă (colț interior): bevel-ul merge doar pe muchii exterioare.";
            return result;
        }
        prof[i] = profile(pa, u1, u2, distance, segments);
    }
    for (std::size_t i = 0; i < m; ++i) {
        for (std::size_t j = i + 1; j < m; ++j) {
            if (sides[i] == sides[j]) {
                result.error = "Două laturi ale feței au aceeași față vecină: nu se poate teși.";
                return result;
            }
        }
    }
    // Punctele de colț: intersecția profilurilor celor două laturi care se întâlnesc în vârful i.
    const std::size_t n = prof[0].size();
    std::vector<std::vector<Vec3d>> corner(m, std::vector<Vec3d>(n));
    for (std::size_t i = 0; i < m; ++i) {
        const std::size_t prev = (i + m - 1) % m;
        for (std::size_t k = 0; k < n; ++k) {
            // Profilul laturii anterioare e definit în jurul vârfului ei de început (v[prev]); ambele drepte sunt
            // paralele cu laturile lor.
            corner[i][k] = closestBetween(prof[prev][k], dirs[prev], prof[i][k], dirs[i]);
        }
    }
    // Retragerea nu are voie să întoarcă fața pe dos.
    for (std::size_t i = 0; i < m; ++i) {
        const std::size_t next = (i + 1) % m;
        if (glm::dot(corner[next][0] - corner[i][0], dirs[i]) <= tol::kLength) {
            result.error = "Distanța e prea mare pentru fața aleasă.";
            return result;
        }
    }
    std::vector<NewFace> added;
    std::vector<Vec3d> inset;
    for (std::size_t i = 0; i < m; ++i) {
        inset.push_back(corner[i][0]);
    }
    added.push_back({ inset, f.plane.normal, styleOf(f) });
    std::vector<std::pair<Vec3d, Vec3d>> smooth;
    for (std::size_t i = 0; i < m; ++i) {
        const std::size_t next = (i + 1) % m;
        const FaceId side = sides[i];
        // Fața vecină: vârfurile laturii (v[i], v[next]) devin punctele de la capătul profilului.
        added.push_back({ replacedLoop(mesh, side, { { v[i], { corner[i][n - 1] } }, { v[next], { corner[next][n - 1] } } }),
            mesh.face(side).plane.normal, styleOf(mesh.face(side)) });
        for (std::size_t k = 0; k + 1 < n; ++k) {
            const Vec3d mid = (corner[i][k] + corner[i][k + 1] + corner[next][k] + corner[next][k + 1]) * 0.25;
            Vec3d nrm = glm::normalize(glm::cross(corner[next][k] - corner[i][k], corner[i][k + 1] - corner[i][k]));
            // Spre exterior: departe de interiorul volumului (sub fața inițială, spre interiorul ei).
            const Vec3d inside = mesh.position(v[i]) - f.plane.normal * distance + (corner[i][0] - mesh.position(v[i]));
            if (glm::dot(nrm, mid - inside) < 0.0) {
                nrm = -nrm;
            }
            added.push_back({ { corner[i][k], corner[next][k], corner[next][k + 1], corner[i][k + 1] }, nrm, styleOf(f) });
        }
        if (segments > 1) {
            for (std::size_t k = 0; k < n; ++k) {
                smooth.emplace_back(corner[i][k], corner[next][k]);
            }
        }
    }
    std::set<FaceId> erased(sides.begin(), sides.end());
    erased.insert(face);
    const std::set<VertexId> moved(v.begin(), v.end());
    result.selection = rebuild(mesh, erased, moved, added, smooth);
    return result;
}

} // namespace aion::geom
