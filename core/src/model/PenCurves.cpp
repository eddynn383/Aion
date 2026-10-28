#include "aion/core/model/PenCurves.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>

#include <set>

namespace aion::model {

namespace {

std::vector<math::Vec3d> samplesOf(const geom::BezierPath& path)
{
    return geom::sampleBezier(path, geom::defaultTolerance(path));
}

// Muchiile curbei, în ordine, dacă geometria se potrivește încă exact cu eșantionarea ei; altfel gol.
std::vector<geom::EdgeId> intactEdges(const geom::Mesh& mesh, const PenCurve& c)
{
    const std::vector<math::Vec3d> samples = samplesOf(c.path);
    std::vector<geom::EdgeId> out;
    if (samples.size() < 2) {
        return out;
    }
    const std::size_t count = c.path.closed ? samples.size() : samples.size() - 1;
    for (std::size_t i = 0; i < count; ++i) {
        const geom::VertexId a = mesh.findVertexNear(samples[i], tol::kLength);
        const geom::VertexId b = mesh.findVertexNear(samples[(i + 1) % samples.size()], tol::kLength);
        const geom::EdgeId e = a && b ? mesh.findEdge(a, b) : geom::EdgeId {};
        if (!e || mesh.edge(e).curve != c.curve) {
            return {};
        }
        out.push_back(e);
    }
    return out;
}

} // namespace

PenCurveId addPenCurveGeometry(Model& model, DefinitionId definition, const geom::BezierPath& path, const math::Vec3d& normal)
{
    geom::Mesh& mesh = model.mesh(definition);
    const std::uint32_t curve = mesh.newCurveId();
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

bool replacePenCurve(Model& model, PenCurveId id, const geom::BezierPath& path)
{
    if (!penCurveEditable(model, id) || path.points.size() < 2) {
        return false;
    }
    PenCurve c = model.penCurves().at(id);
    geom::Mesh& mesh = model.mesh(c.definition);
    const std::vector<geom::EdgeId> old = intactEdges(mesh, c);
    // Fața-gazdă (fețele lipite de curbă, fără cea a curbei): muchiile ei și materialul, ca s-o refacem.
    const std::set<geom::EdgeId> oldSet(old.begin(), old.end());
    std::set<geom::EdgeId> host;
    model::MaterialId material;
    model::MaterialId backMaterial;
    for (geom::EdgeId e : old) {
        for (geom::FaceId f : mesh.edge(e).faces) {
            const geom::Face& face = mesh.face(f);
            bool hostFace = false;
            const auto collect = [&](const geom::Loop& loop) {
                for (const geom::EdgeUse& use : loop.uses) {
                    if (!oldSet.contains(use.edge)) {
                        host.insert(use.edge);
                        hostFace = true;
                    }
                }
            };
            collect(face.outer);
            for (const geom::Loop& loop : face.inner) {
                collect(loop);
            }
            if (hostFace && !material && !backMaterial) {
                material = face.material;
                backMaterial = face.backMaterial;
            }
        }
    }
    for (geom::EdgeId e : old) {
        if (mesh.edges().contains(e)) {
            geom::eraseEdge(mesh, e);
        }
    }
    const geom::DrawLineResult drawn = geom::drawCurve(mesh, samplesOf(path), path.closed, true, c.curve);
    if (!host.empty()) {
        std::vector<geom::EdgeId> all(host.begin(), host.end());
        all.insert(all.end(), drawn.edges.begin(), drawn.edges.end());
        const geom::DrawLineResult faces = geom::createFaces(mesh, all);
        for (geom::FaceId f : faces.createdFaces) {
            // Fața refăcută a gazdei își primește materialul; cea a curbei rămâne ca la desen.
            bool touchesHost = false;
            for (const geom::EdgeUse& use : mesh.face(f).outer.uses) {
                touchesHost = touchesHost || host.contains(use.edge);
            }
            if (touchesHost) {
                mesh.setFaceMaterial(f, material);
                if (backMaterial) {
                    mesh.setFaceBackMaterial(f, backMaterial);
                }
            }
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
    for (std::size_t i = 0; i < model.penCurves().size(); ++i) {
        const PenCurve& c = model.penCurves().values()[i];
        if (c.definition == definition && c.curve == e->curve) {
            return model.penCurves().keys()[i];
        }
    }
    return {};
}

} // namespace aion::model
