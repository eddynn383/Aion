#include "aion/core/geom/Subdivide.h"

#include "aion/core/geom/Operations.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aion::geom {

namespace {

using math::Vec3d;

// O plasă de poligoane după indici, ca Catmull-Clark să lucreze fără topologia Mesh-ului.
struct Polygon {
    std::vector<int> vertices; // în sensul normalei (regula mâinii drepte)
    int source = 0; // fața originală din care vine (pentru material și tag)
};

struct PolyMesh {
    std::vector<Vec3d> points;
    std::vector<Polygon> polygons;
};

using EdgeKey = std::pair<int, int>;

EdgeKey keyOf(int a, int b)
{
    return a < b ? EdgeKey { a, b } : EdgeKey { b, a };
}

// Un pas Catmull-Clark: punct de față, puncte de muchie, vârfuri mutate, apoi câte un patrulater pe colț.
PolyMesh catmullClark(const PolyMesh& in)
{
    const int vertexCount = static_cast<int>(in.points.size());
    std::vector<Vec3d> facePoints;
    facePoints.reserve(in.polygons.size());
    for (const Polygon& p : in.polygons) {
        Vec3d sum { 0.0 };
        for (int v : p.vertices) {
            sum += in.points[static_cast<std::size_t>(v)];
        }
        facePoints.push_back(sum / static_cast<double>(p.vertices.size()));
    }

    // Muchiile, cu fețele lor.
    std::map<EdgeKey, std::vector<int>> edgeFaces;
    for (std::size_t f = 0; f < in.polygons.size(); ++f) {
        const auto& vs = in.polygons[f].vertices;
        for (std::size_t i = 0; i < vs.size(); ++i) {
            edgeFaces[keyOf(vs[i], vs[(i + 1) % vs.size()])].push_back(static_cast<int>(f));
        }
    }

    PolyMesh out;
    out.points.resize(static_cast<std::size_t>(vertexCount)); // vârfurile mutate, completate mai jos
    // Punctele de față.
    const int faceBase = static_cast<int>(out.points.size());
    out.points.insert(out.points.end(), facePoints.begin(), facePoints.end());
    // Punctele de muchie: media capetelor și a punctelor de față (pe o margine, mijlocul).
    std::map<EdgeKey, int> edgePoint;
    for (const auto& [key, faces] : edgeFaces) {
        const Vec3d a = in.points[static_cast<std::size_t>(key.first)];
        const Vec3d b = in.points[static_cast<std::size_t>(key.second)];
        Vec3d p = (a + b) * 0.5;
        if (faces.size() == 2) {
            p = (a + b + facePoints[static_cast<std::size_t>(faces[0])] + facePoints[static_cast<std::size_t>(faces[1])]) * 0.25;
        }
        edgePoint[key] = static_cast<int>(out.points.size());
        out.points.push_back(p);
    }

    // Vârfurile mutate.
    std::vector<std::vector<int>> vertexFaces(static_cast<std::size_t>(vertexCount));
    std::vector<std::vector<EdgeKey>> vertexEdges(static_cast<std::size_t>(vertexCount));
    for (std::size_t f = 0; f < in.polygons.size(); ++f) {
        for (int v : in.polygons[f].vertices) {
            vertexFaces[static_cast<std::size_t>(v)].push_back(static_cast<int>(f));
        }
    }
    for (const auto& [key, faces] : edgeFaces) {
        vertexEdges[static_cast<std::size_t>(key.first)].push_back(key);
        vertexEdges[static_cast<std::size_t>(key.second)].push_back(key);
    }
    for (int v = 0; v < vertexCount; ++v) {
        const auto vi = static_cast<std::size_t>(v);
        const Vec3d p = in.points[vi];
        std::vector<Vec3d> borderNeighbours;
        Vec3d midSum { 0.0 };
        for (const EdgeKey& e : vertexEdges[vi]) {
            const int other = e.first == v ? e.second : e.first;
            const Vec3d q = in.points[static_cast<std::size_t>(other)];
            midSum += (p + q) * 0.5;
            if (edgeFaces.at(e).size() != 2) {
                borderNeighbours.push_back(q);
            }
        }
        if (vertexEdges[vi].empty()) {
            out.points[vi] = p;
        } else if (!borderNeighbours.empty()) {
            // Pe o margine: urmează curba marginii; un colț (sau o răscruce de margini) rămâne pe loc.
            out.points[vi] = borderNeighbours.size() == 2 ? (p * 6.0 + borderNeighbours[0] + borderNeighbours[1]) / 8.0 : p;
        } else {
            const double n = static_cast<double>(vertexFaces[vi].size());
            Vec3d faceSum { 0.0 };
            for (int f : vertexFaces[vi]) {
                faceSum += facePoints[static_cast<std::size_t>(f)];
            }
            const Vec3d faceAverage = faceSum / n;
            const Vec3d midAverage = midSum / static_cast<double>(vertexEdges[vi].size());
            out.points[vi] = (faceAverage + midAverage * 2.0 + p * (n - 3.0)) / n;
        }
    }

    // Un patrulater pe fiecare colț: vârful, mijlocul muchiei următoare, centrul, mijlocul muchiei dinainte.
    for (std::size_t f = 0; f < in.polygons.size(); ++f) {
        const auto& vs = in.polygons[f].vertices;
        const std::size_t n = vs.size();
        for (std::size_t i = 0; i < n; ++i) {
            const int prev = vs[(i + n - 1) % n];
            const int cur = vs[i];
            const int next = vs[(i + 1) % n];
            out.polygons.push_back({ { cur, edgePoint.at(keyOf(cur, next)), faceBase + static_cast<int>(f),
                                         edgePoint.at(keyOf(prev, cur)) },
                in.polygons[f].source });
        }
    }
    return out;
}

Vec3d polygonNormal(const std::vector<Vec3d>& points)
{
    Vec3d n { 0.0 };
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Vec3d& a = points[i];
        const Vec3d& b = points[(i + 1) % points.size()];
        n += Vec3d { (a.y - b.y) * (a.z + b.z), (a.z - b.z) * (a.x + b.x), (a.x - b.x) * (a.y + b.y) };
    }
    return n;
}

} // namespace

SubdivideResult subdivide(Mesh& mesh, const std::set<FaceId>& faces, int levels, double softAngleDegrees)
{
    SubdivideResult result;
    levels = std::clamp(levels, 1, 4);
    std::vector<FaceId> sources;
    std::size_t corners = 0;
    for (FaceId f : faces) {
        if (!mesh.faces().contains(f)) {
            continue;
        }
        if (!mesh.face(f).inner.empty()) {
            result.error = "Fețele cu găuri nu se pot subdiviza (împarte-le întâi cu o linie până la marginea găurii).";
            return result;
        }
        sources.push_back(f);
        corners += mesh.face(f).outer.uses.size();
    }
    if (sources.empty()) {
        result.error = "Nu e nicio față de subdivizat.";
        return result;
    }
    // Primul pas face câte un patrulater pe colț, apoi fiecare pas înmulțește cu 4 (și fiecare se poate împărți în 2).
    std::size_t estimate = corners * 2;
    for (int k = 1; k < levels; ++k) {
        estimate *= 4;
    }
    if (estimate > kMaxSubdivideFaces) {
        result.error = "Ar ieși prea multe fețe (" + std::to_string(estimate) + "). Alege mai puține niveluri.";
        return result;
    }

    // Plasa de poligoane, cu vârfurile comune unite după ID.
    PolyMesh poly;
    std::unordered_map<std::uint64_t, int> indexOf;
    std::vector<VertexId> usedVertices;
    std::set<EdgeId> usedEdges;
    for (std::size_t s = 0; s < sources.size(); ++s) {
        const Face& face = mesh.face(sources[s]);
        std::vector<VertexId> loop = mesh.loopVertices(face.outer);
        std::vector<Vec3d> points;
        for (VertexId v : loop) {
            points.push_back(mesh.position(v));
        }
        if (glm::dot(polygonNormal(points), face.plane.normal) < 0.0) {
            std::reverse(loop.begin(), loop.end());
        }
        Polygon p { {}, static_cast<int>(s) };
        for (VertexId v : loop) {
            const auto [it, inserted] = indexOf.emplace(v.value, static_cast<int>(poly.points.size()));
            if (inserted) {
                poly.points.push_back(mesh.position(v));
                usedVertices.push_back(v);
            }
            p.vertices.push_back(it->second);
        }
        poly.polygons.push_back(std::move(p));
        for (const EdgeUse& u : face.outer.uses) {
            usedEdges.insert(u.edge);
        }
    }
    for (int k = 0; k < levels; ++k) {
        poly = catmullClark(poly);
    }

    // Stilul fețelor originale, înainte să dispară.
    struct Style {
        model::MaterialId material;
        model::MaterialId backMaterial;
        model::TagId tag;
    };
    std::vector<Style> styles;
    for (FaceId f : sources) {
        const Face& face = mesh.face(f);
        styles.push_back({ face.material, face.backMaterial, face.tag });
    }

    // Scoatem fețele vechi și ce rămâne fără ele (muchiile și vârfurile lor, dacă nu mai țin de altceva).
    for (FaceId f : sources) {
        mesh.removeFace(f);
    }
    for (EdgeId e : usedEdges) {
        if (mesh.edges().contains(e) && mesh.edge(e).faces.empty()) {
            mesh.removeEdge(e);
        }
    }
    for (VertexId v : usedVertices) {
        if (mesh.vertices().contains(v) && mesh.vertex(v).edges.empty()) {
            mesh.removeVertex(v);
        }
    }

    // Fețele noi.
    MeshBuilder builder(mesh);
    for (const Polygon& p : poly.polygons) {
        std::vector<Vec3d> points;
        for (int v : p.vertices) {
            points.push_back(poly.points[static_cast<std::size_t>(v)]);
        }
        const Style& style = styles[static_cast<std::size_t>(p.source)];
        for (FaceId f : builder.face(points)) {
            if (style.material) {
                mesh.setFaceMaterial(f, style.material);
            }
            if (style.backMaterial) {
                mesh.setFaceBackMaterial(f, style.backMaterial);
            }
            if (style.tag) {
                mesh.setFaceTag(f, style.tag);
            }
            result.selection.faces.insert(f);
        }
    }
    // Unește doar fețele coplanare vecine (netezirea o facem mai jos, doar pe muchiile noi).
    builder.finish(0.0);

    // finish poate uni fețe noi: selecția păstrează doar fețele care mai există.
    std::erase_if(result.selection.faces, [&](FaceId f) { return !mesh.faces().contains(f); });
    const double cosSoft = std::cos(glm::radians(std::clamp(softAngleDegrees, 0.0, 180.0)));
    std::set<EdgeId> fresh;
    for (FaceId f : result.selection.faces) {
        for (const EdgeUse& u : mesh.face(f).outer.uses) {
            fresh.insert(u.edge);
        }
    }
    for (EdgeId e : fresh) {
        if (!mesh.edges().contains(e)) {
            continue;
        }
        const Edge& edge = mesh.edge(e);
        if (edge.faces.size() != 2) {
            continue;
        }
        const double cosAngle = glm::dot(mesh.face(edge.faces[0]).plane.normal, mesh.face(edge.faces[1]).plane.normal);
        if (cosAngle >= cosSoft && (!edge.soft || !edge.smoothCurve)) {
            mesh.setEdgeStyle(e, true, edge.curve, true);
        }
    }
    return result;
}

} // namespace aion::geom
