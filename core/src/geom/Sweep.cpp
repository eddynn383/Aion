#include "aion/core/geom/Sweep.h"

#include "FaceBuilder.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>

namespace aion::geom {

namespace {

// Traseul ca șir de vârfuri, în ordine; `closed` = un inel (ultimul vârf se leagă de primul).
struct Path {
    std::vector<VertexId> vertices;
    std::vector<EdgeId> edges; // edges[i] leagă vertices[i] de vertices[i + 1] (și ultimul de primul, la inel)
    bool closed = false;
};

std::optional<Path> orderPath(const Mesh& mesh, const std::set<EdgeId>& edges, std::string& error)
{
    std::map<VertexId, std::vector<EdgeId>> around;
    for (EdgeId e : edges) {
        if (!mesh.edges().contains(e)) {
            error = "Traseul are muchii care nu mai există.";
            return std::nullopt;
        }
        around[mesh.edge(e).v0].push_back(e);
        around[mesh.edge(e).v1].push_back(e);
    }
    VertexId start;
    for (const auto& [v, list] : around) {
        if (list.size() > 2) {
            error = "Traseul se ramifică: alege muchii care formează un singur drum.";
            return std::nullopt;
        }
        if (list.size() == 1 && !start) {
            start = v;
        }
    }
    Path path;
    path.closed = !start;
    if (!start) {
        start = around.begin()->first;
    }
    std::set<EdgeId> used;
    VertexId v = start;
    path.vertices.push_back(v);
    while (true) {
        EdgeId next;
        for (EdgeId e : around[v]) {
            if (!used.contains(e)) {
                next = e;
                break;
            }
        }
        if (!next) {
            break;
        }
        used.insert(next);
        path.edges.push_back(next);
        v = mesh.edge(next).other(v);
        if (path.closed && v == start) {
            break;
        }
        path.vertices.push_back(v);
    }
    if (used.size() != edges.size()) {
        error = "Muchiile traseului nu sunt legate între ele.";
        return std::nullopt;
    }
    return path;
}

// Punctul în care dreapta (p, d) taie planul (origin, normal).
Vec3d alongTo(const Vec3d& p, const Vec3d& d, const Vec3d& origin, const Vec3d& normal)
{
    const double denom = glm::dot(d, normal);
    if (std::abs(denom) < 1e-12) {
        return p;
    }
    return p + d * (glm::dot(origin - p, normal) / denom);
}

bool isSmooth(const Mesh& mesh, EdgeId e)
{
    const Edge& edge = mesh.edge(e);
    return edge.soft || edge.smoothCurve;
}

} // namespace

SweepResult followMe(Mesh& mesh, FaceId profile, const std::set<EdgeId>& pathEdges)
{
    SweepResult result;
    if (!mesh.faces().contains(profile) || pathEdges.empty()) {
        result.error = "Alege o față (profilul) și un traseu de muchii.";
        return result;
    }
    const Face& face = mesh.face(profile);
    if (!face.inner.empty()) {
        result.error = "Profilul are găuri: încă nu se poate folosi la Follow Me.";
        return result;
    }
    // Profilul nu face parte din traseu.
    std::set<EdgeId> pathSet = pathEdges;
    for (const EdgeUse& u : face.outer.uses) {
        pathSet.erase(u.edge);
    }
    if (pathSet.empty()) {
        result.error = "Traseul trebuie să fie alte muchii decât conturul profilului.";
        return result;
    }
    auto ordered = orderPath(mesh, pathSet, result.error);
    if (!ordered) {
        return result;
    }
    Path path = std::move(*ordered);

    const std::vector<Vec3d> profilePoints = mesh.loopPoints(face.outer);
    const std::vector<EdgeId> profileEdges = [&] {
        std::vector<EdgeId> out;
        for (const EdgeUse& u : face.outer.uses) {
            out.push_back(u.edge);
        }
        return out;
    }();
    Vec3d centroid { 0.0 };
    for (const Vec3d& p : profilePoints) {
        centroid += p;
    }
    centroid /= static_cast<double>(profilePoints.size());

    // Pornirea: un traseu deschis, de la capătul de lângă profil; un inel, de la vârful cel mai apropiat.
    if (!path.closed) {
        const double front = glm::length(mesh.position(path.vertices.front()) - centroid);
        const double back = glm::length(mesh.position(path.vertices.back()) - centroid);
        if (back < front) {
            std::reverse(path.vertices.begin(), path.vertices.end());
            std::reverse(path.edges.begin(), path.edges.end());
        }
    } else {
        std::size_t best = 0;
        for (std::size_t i = 1; i < path.vertices.size(); ++i) {
            if (glm::length(mesh.position(path.vertices[i]) - centroid) < glm::length(mesh.position(path.vertices[best]) - centroid)) {
                best = i;
            }
        }
        std::rotate(path.vertices.begin(), path.vertices.begin() + std::ptrdiff_t(best), path.vertices.end());
        std::rotate(path.edges.begin(), path.edges.begin() + std::ptrdiff_t(best), path.edges.end());
    }
    std::vector<Vec3d> points;
    for (VertexId v : path.vertices) {
        points.push_back(mesh.position(v));
    }
    const std::size_t n = points.size();
    const std::size_t segments = path.closed ? n : n - 1;
    std::vector<Vec3d> dirs(segments);
    for (std::size_t i = 0; i < segments; ++i) {
        const Vec3d d = points[(i + 1) % n] - points[i];
        if (glm::length(d) < tol::kLength) {
            result.error = "Traseul are o muchie de lungime zero.";
            return result;
        }
        dirs[i] = glm::normalize(d);
    }
    const Vec3d normal = face.plane.normal;
    const double facing = glm::dot(normal, dirs.front());
    if (std::abs(facing) < 0.05) {
        result.error = "Profilul e paralel cu traseul: așază-l de-a curmezișul începutului traseului.";
        return result;
    }
    // Sensul buclei profilului față de direcția de mers (pentru normalele spre exterior ale benzilor).
    const double winding = facing > 0.0 ? 1.0 : -1.0;

    // Secțiunile: la un inel, profilul se proiectează întâi pe planul bisector din vârful de pornire.
    const auto miter = [&](std::size_t i) {
        const Vec3d in = dirs[(i + segments - 1) % segments];
        const Vec3d out = dirs[i % segments];
        const Vec3d m = in + out;
        return glm::length(m) < 1e-9 ? out : glm::normalize(m);
    };
    const std::size_t ringCount = n;
    std::vector<std::vector<Vec3d>> rings(ringCount);
    if (path.closed) {
        for (const Vec3d& p : profilePoints) {
            rings[0].push_back(alongTo(p, dirs[0], points[0], miter(0)));
        }
    } else {
        rings[0] = profilePoints;
    }
    for (std::size_t i = 1; i < ringCount; ++i) {
        const Vec3d planeNormal = (!path.closed && i == n - 1) ? dirs[i - 1] : miter(i);
        for (const Vec3d& p : rings[i - 1]) {
            rings[i].push_back(alongTo(p, dirs[i - 1], points[i], planeNormal));
        }
    }
    // O secțiune strânsă într-un punct (cot prea strâns pentru profil) ar da fețe degenerate.
    for (const auto& ring : rings) {
        for (std::size_t k = 0; k < ring.size(); ++k) {
            if (glm::length(ring[k] - ring[(k + 1) % ring.size()]) < tol::kLength) {
                result.error = "Un cot al traseului e prea strâns pentru mărimea profilului.";
                return result;
            }
        }
    }

    const FaceStyle style = styleOf(face);
    const std::size_t m = profilePoints.size();
    std::vector<NewFace> faces;
    std::vector<std::pair<Vec3d, Vec3d>> smooth;
    for (std::size_t i = 0; i < segments; ++i) {
        const std::vector<Vec3d>& a = rings[i];
        const std::vector<Vec3d>& b = rings[(i + 1) % ringCount];
        for (std::size_t k = 0; k < m; ++k) {
            const std::size_t k1 = (k + 1) % m;
            const Vec3d outward = glm::cross(a[k1] - a[k], dirs[i]) * winding;
            faces.push_back({ { a[k], a[k1], b[k1], b[k] }, glm::normalize(outward), style, true });
        }
    }
    // Muchiile netede: de-a lungul (la un vârf de profil între două muchii netede, de exemplu un cerc) și de-a
    // curmezișul (la un vârf de traseu între două muchii netede, de exemplu un arc).
    for (std::size_t k = 0; k < m; ++k) {
        if (isSmooth(mesh, profileEdges[(k + m - 1) % m]) && isSmooth(mesh, profileEdges[k])) {
            for (std::size_t i = 0; i < segments; ++i) {
                smooth.emplace_back(rings[i][k], rings[(i + 1) % ringCount][k]);
            }
        }
    }
    for (std::size_t i = 0; i < ringCount; ++i) {
        const bool interior = path.closed || (i > 0 && i + 1 < n);
        if (!interior) {
            continue;
        }
        const EdgeId in = path.edges[(i + segments - 1) % segments];
        const EdgeId out = path.edges[i % segments];
        if (isSmooth(mesh, in) && isSmooth(mesh, out)) {
            for (std::size_t k = 0; k < m; ++k) {
                smooth.emplace_back(rings[i][k], rings[i][(k + 1) % m]);
            }
        }
    }
    // Capacele: la un traseu deschis, profilul rămâne la început (întors spre exterior), iar la final apare o față.
    if (!path.closed) {
        faces.push_back({ rings[n - 1], dirs.back(), style, true });
        orientFace(mesh, profile, -dirs.front());
    } else {
        // Inelul înlocuiește profilul: fața lui și muchiile rămase libere dispar.
        eraseFace(mesh, profile);
        for (EdgeId e : profileEdges) {
            if (mesh.edges().contains(e) && mesh.edge(e).faces.empty() && !pathSet.contains(e)) {
                eraseEdge(mesh, e);
            }
        }
    }
    result.selection = buildFaces(mesh, faces, smooth);
    if (result.selection.faces.empty()) {
        result.error = "Follow Me nu a putut construi fețele.";
    }
    return result;
}

} // namespace aion::geom
