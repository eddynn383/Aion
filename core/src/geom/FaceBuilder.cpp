#include "FaceBuilder.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Plane.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <set>

namespace aion::geom {

namespace {

// Fața nou desenată a poligonului: are toate punctele lui pe conturul exterior și planul paralel cu normala cerută.
// (Centrul nu ajunge: la un poligon concav poate cădea în afara lui.)
FaceId faceOfPolygon(const Mesh& mesh, const NewFace& face)
{
    std::vector<VertexId> corners;
    for (const Vec3d& p : face.points) {
        const VertexId v = mesh.findVertexNear(p, tol::kLength * 10);
        if (!v) {
            return {};
        }
        corners.push_back(v);
    }
    for (EdgeId e : mesh.vertex(corners.front()).edges) {
        for (FaceId f : mesh.edge(e).faces) {
            const Face& candidate = mesh.face(f);
            if (std::abs(glm::dot(candidate.plane.normal, face.normal)) < 0.999) {
                continue;
            }
            const std::vector<VertexId> loop = mesh.loopVertices(candidate.outer);
            const bool all = std::all_of(corners.begin(), corners.end(),
                [&](VertexId v) { return std::find(loop.begin(), loop.end(), v) != loop.end(); });
            if (all) {
                return f;
            }
        }
    }
    return {};
}

// Punctul e în interiorul poligonului (în planul lui)?
bool insidePolygon(const std::vector<Vec3d>& polygon, const math::Plane& plane, const Vec3d& p)
{
    if (std::abs(plane.signedDistance(p)) > tol::kLength) {
        return false;
    }
    const math::PlaneFrame frame(plane);
    std::vector<math::Vec2d> poly;
    poly.reserve(polygon.size());
    for (const Vec3d& q : polygon) {
        poly.push_back(frame.toLocal(q));
    }
    return math::locatePoint(frame.toLocal(p), poly, tol::kLength) == math::PointLocation::Inside;
}

} // namespace

FaceStyle styleOf(const Face& f)
{
    return { f.material, f.backMaterial, f.tag, f.texture };
}

Selection buildFaces(Mesh& mesh, const std::vector<NewFace>& faces, const std::vector<std::pair<Vec3d, Vec3d>>& smoothEdges)
{
    const std::vector<FaceId> before = mesh.faces().keys();
    const std::set<FaceId> existing(before.begin(), before.end());
    // Fețele de dinainte (conturul și planul): o față nouă din interiorul uneia e o bucată a ei (împărțită de
    // o latură nouă), nu o față închisă din greșeală, și rămâne.
    std::vector<std::pair<math::Plane, std::vector<Vec3d>>> previous;
    for (FaceId f : before) {
        previous.emplace_back(mesh.face(f).plane, mesh.loopPoints(mesh.face(f).outer));
    }
    const auto pieceOfPrevious = [&](FaceId f) {
        const Vec3d p = faceInteriorPoint(mesh, f);
        return std::any_of(previous.begin(), previous.end(),
            [&](const auto& face) { return insidePolygon(face.second, face.first, p); });
    };
    Selection out;
    std::set<FaceId> built;
    for (const NewFace& face : faces) {
        drawPolygon(mesh, face.points);
        if (const FaceId f = faceOfPolygon(mesh, face)) {
            const FaceId oriented = orientFace(mesh, f, face.normal);
            mesh.setFaceMaterial(oriented, face.style.material);
            mesh.setFaceBackMaterial(oriented, face.style.backMaterial);
            mesh.setFaceTag(oriented, face.style.tag);
            mesh.setFaceTexture(oriented, face.style.texture);
            built.insert(oriented);
            if (face.selected) {
                out.faces.insert(oriented);
            }
        }
    }
    // drawPolygon închide singur orice buclă plană; fețele pe care nu le-am cerut (de exemplu inelul de muchii
    // coplanare de sub o față teșită) se scot, muchiile rămân. O copie a listei: eraseFace o schimbă.
    const std::vector<FaceId> after = mesh.faces().keys();
    for (FaceId f : after) {
        if (!existing.contains(f) && !built.contains(f) && !pieceOfPrevious(f)) {
            eraseFace(mesh, f);
        }
    }
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

} // namespace aion::geom
