#include "aion/core/model/PenCurves.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/math/Plane.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <set>

namespace aion::model {

namespace {

std::vector<math::Vec3d> samplesOf(const geom::BezierPath& path)
{
    return geom::sampleBezier(path, geom::defaultTolerance(path));
}

// Muchiile curbei, în ordine, dacă geometria se potrivește încă exact cu eșantionarea ei; altfel gol. Se caută doar
// printre muchiile cu id-ul curbei (nu în toate vârfurile: se apelează la fiecare schimbare a selecției).
std::vector<geom::EdgeId> intactEdges(const geom::Mesh& mesh, const PenCurve& c)
{
    const std::vector<math::Vec3d> samples = samplesOf(c.path);
    std::vector<geom::EdgeId> out;
    if (samples.size() < 2) {
        return out;
    }
    std::vector<geom::EdgeId> candidates;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        if (mesh.edges().values()[i].curve == c.curve) {
            candidates.push_back(mesh.edges().keys()[i]);
        }
    }
    const std::size_t count = c.path.closed ? samples.size() : samples.size() - 1;
    for (std::size_t i = 0; i < count; ++i) {
        const math::Vec3d& a = samples[i];
        const math::Vec3d& b = samples[(i + 1) % samples.size()];
        const auto found = std::find_if(candidates.begin(), candidates.end(), [&](geom::EdgeId id) {
            const geom::Edge& e = mesh.edge(id);
            const math::Vec3d p0 = mesh.position(e.v0);
            const math::Vec3d p1 = mesh.position(e.v1);
            return (tol::samePoint(p0, a) && tol::samePoint(p1, b)) || (tol::samePoint(p0, b) && tol::samePoint(p1, a));
        });
        if (found == candidates.end()) {
            return {};
        }
        out.push_back(*found);
    }
    return out;
}

} // namespace

PenCurveId addPenCurveGeometry(Model& model, DefinitionId definition, const geom::BezierPath& path, const math::Vec3d& normal)
{
    geom::Mesh& mesh = model.mesh(definition);
    // Un id nou și față de curbele Pen rămase fără muchii (grupate, șterse), ca să nu se confunde cu ele.
    std::uint32_t curve = mesh.newCurveId();
    for (const PenCurve& other : model.penCurves().values()) {
        if (other.definition == definition) {
            curve = std::max(curve, other.curve + 1);
        }
    }
    geom::drawCurve(mesh, samplesOf(path), path.closed, true, curve);
    return model.addPenCurve({ definition, curve, path, glm::normalize(normal) });
}

bool penCurveEditable(const Model& model, PenCurveId id)
{
    const PenCurve* c = model.penCurves().find(id);
    if (c == nullptr || !model.definitions().contains(c->definition)) {
        return false;
    }
    const geom::Mesh& mesh = model.mesh(c->definition);
    const std::vector<geom::EdgeId> edges = intactEdges(mesh, *c);
    if (edges.empty()) {
        return false;
    }
    // Doar fețe în planul curbei (a ei și ale feței-gazdă): o față laterală (Push/Pull) o scoate din editare.
    const math::Vec3d anchor = c->path.points.front().point;
    for (geom::EdgeId e : edges) {
        const geom::Edge& edge = mesh.edge(e);
        if (edge.faces.size() > 2) {
            return false;
        }
        for (geom::FaceId f : edge.faces) {
            const math::Plane& plane = mesh.face(f).plane;
            if (!tol::parallel(plane.normal, c->normal) || std::abs(plane.signedDistance(anchor)) > tol::kLength) {
                return false;
            }
        }
    }
    return true;
}

namespace {

// O față de dinainte de editare: conturul ei (pe planul ei) și materialele, ca fețele refăcute să le primească înapoi.
struct OldFace {
    math::Plane plane;
    std::vector<std::vector<math::Vec3d>> loops; // conturul, apoi găurile
    model::MaterialId material;
    model::MaterialId backMaterial;
    bool curveFace = false; // fața închisă de curbă
};

bool insideOld(const OldFace& f, const math::Vec3d& p)
{
    const math::PlaneFrame frame(f.plane);
    const auto locate = [&](const std::vector<math::Vec3d>& loop) {
        std::vector<math::Vec2d> poly;
        for (const math::Vec3d& v : loop) {
            poly.push_back(frame.toLocal(v));
        }
        return math::locatePoint(frame.toLocal(p), poly, tol::kLength);
    };
    if (f.loops.empty() || locate(f.loops.front()) != math::PointLocation::Inside) {
        return false;
    }
    return std::none_of(f.loops.begin() + 1, f.loops.end(),
        [&](const std::vector<math::Vec3d>& hole) { return locate(hole) != math::PointLocation::Outside; });
}

// Un punct sigur în interiorul feței: centrul celui mai mare triunghi al ei.
std::optional<math::Vec3d> interiorPoint(const geom::Mesh& mesh, geom::FaceId face)
{
    const geom::FaceTriangles tris = geom::triangulateFace(mesh, face);
    double best = 0.0;
    std::optional<math::Vec3d> out;
    for (std::size_t i = 0; i + 2 < tris.indices.size(); i += 3) {
        const math::Vec3d a = tris.points[tris.indices[i]];
        const math::Vec3d b = tris.points[tris.indices[i + 1]];
        const math::Vec3d c = tris.points[tris.indices[i + 2]];
        const double area = glm::length(glm::cross(b - a, c - a));
        if (area > best) {
            best = area;
            out = (a + b + c) / 3.0;
        }
    }
    return out;
}

} // namespace

bool replacePenCurve(Model& model, PenCurveId id, const geom::BezierPath& path)
{
    if (!penCurveEditable(model, id) || path.points.size() < 2) {
        return false;
    }
    PenCurve c = model.penCurves().at(id);
    geom::Mesh& mesh = model.mesh(c.definition);
    const std::vector<geom::EdgeId> old = intactEdges(mesh, c);
    // Fețele lipite de curbă: a ei (închisă doar de muchiile curbei) și ale gazdei, cu conturul și materialele lor;
    // muchiile gazdei, ca s-o refacem.
    const std::set<geom::EdgeId> oldSet(old.begin(), old.end());
    std::set<geom::EdgeId> host;
    std::vector<OldFace> faces;
    std::set<geom::FaceId> seen;
    for (geom::EdgeId e : old) {
        for (geom::FaceId f : mesh.edge(e).faces) {
            if (!seen.insert(f).second) {
                continue;
            }
            const geom::Face& face = mesh.face(f);
            OldFace record { face.plane, {}, face.material, face.backMaterial, true };
            record.loops.push_back(mesh.loopPoints(face.outer));
            for (const geom::Loop& loop : face.inner) {
                record.loops.push_back(mesh.loopPoints(loop));
            }
            const auto collect = [&](const geom::Loop& loop) {
                for (const geom::EdgeUse& use : loop.uses) {
                    if (!oldSet.contains(use.edge)) {
                        host.insert(use.edge);
                        record.curveFace = false;
                    }
                }
            };
            collect(face.outer);
            for (const geom::Loop& loop : face.inner) {
                collect(loop);
            }
            faces.push_back(std::move(record));
        }
    }
    for (geom::EdgeId e : old) {
        if (mesh.edges().contains(e)) {
            geom::eraseEdge(mesh, e);
        }
    }
    const geom::DrawLineResult drawn = geom::drawCurve(mesh, samplesOf(path), path.closed, true, c.curve);
    std::vector<geom::FaceId> created = drawn.createdFaces;
    if (!host.empty()) {
        std::vector<geom::EdgeId> all(host.begin(), host.end());
        all.insert(all.end(), drawn.edges.begin(), drawn.edges.end());
        const geom::DrawLineResult refaced = geom::createFaces(mesh, all);
        created.insert(created.end(), refaced.createdFaces.begin(), refaced.createdFaces.end());
    }
    // Fiecare față nouă primește materialele fețelor vechi în care cade; fața curbei (mutată poate în afara ei), pe ale
    // vechii fețe a curbei.
    const std::set<geom::EdgeId> drawnSet(drawn.edges.begin(), drawn.edges.end());
    for (geom::FaceId f : created) {
        if (!mesh.faces().contains(f)) {
            continue;
        }
        const geom::Loop& outer = mesh.face(f).outer;
        const bool curveFace = std::all_of(outer.uses.begin(), outer.uses.end(),
            [&](const geom::EdgeUse& use) { return drawnSet.contains(use.edge); });
        const OldFace* source = nullptr;
        for (const OldFace& before : faces) {
            if (curveFace && before.curveFace) {
                source = &before;
                break;
            }
        }
        if (source == nullptr) {
            if (const auto p = interiorPoint(mesh, f)) {
                for (const OldFace& before : faces) {
                    if (!before.curveFace && insideOld(before, *p)) {
                        source = &before;
                        break;
                    }
                }
            }
        }
        if (source == nullptr) {
            for (const OldFace& before : faces) {
                if (!before.curveFace) {
                    source = &before; // o față-gazdă fără punct comun: materialul primei gazde
                    break;
                }
            }
        }
        if (source != nullptr) {
            mesh.setFaceMaterial(f, source->material);
            mesh.setFaceBackMaterial(f, source->backMaterial);
        }
    }
    c.path = path;
    model.setPenCurve(id, c);
    return true;
}

PenCurveId penCurveForEdge(const Model& model, DefinitionId definition, geom::EdgeId edge)
{
    if (!model.definitions().contains(definition)) {
        return {};
    }
    const geom::Mesh& mesh = model.mesh(definition);
    const geom::Edge* e = mesh.edges().find(edge);
    if (e == nullptr || e->curve == 0) {
        return {};
    }
    // Doar o curbă ale cărei muchii sunt încă ale ei (o entitate rămasă fără muchii nu ia altă curbă cu același id).
    for (std::size_t i = 0; i < model.penCurves().size(); ++i) {
        const PenCurve& c = model.penCurves().values()[i];
        if (c.definition == definition && c.curve == e->curve) {
            const std::vector<geom::EdgeId> edges = intactEdges(mesh, c);
            if (std::find(edges.begin(), edges.end(), edge) != edges.end()) {
                return model.penCurves().keys()[i];
            }
        }
    }
    return {};
}

} // namespace aion::model
