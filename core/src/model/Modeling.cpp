#include "aion/core/model/Modeling.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Operations.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>

namespace aion::model {

std::pair<math::Vec3d, math::Vec3d> selectionBounds(
    const Model& model, DefinitionId context, const geom::Selection& selection)
{
    constexpr double kInf = std::numeric_limits<double>::infinity();
    math::Vec3d lo { kInf };
    math::Vec3d hi { -kInf };
    const geom::Mesh& mesh = model.mesh(context);
    for (geom::VertexId v : geom::verticesOf(mesh, selection)) {
        lo = glm::min(lo, mesh.position(v));
        hi = glm::max(hi, mesh.position(v));
    }
    for (InstanceId id : selection.instances) {
        const Instance& inst = model.instance(id);
        const auto [a, b] = bounds(model, inst.definition);
        if (a.x > b.x) {
            continue;
        }
        for (int k = 0; k < 8; ++k) {
            const math::Vec3d corner { (k & 1) ? b.x : a.x, (k & 2) ? b.y : a.y, (k & 4) ? b.z : a.z };
            const math::Vec3d p = inst.transform.applyToPoint(corner);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    return { lo, hi };
}

geom::Selection mirrorSelection(Model& model, DefinitionId context, const geom::Selection& selection, int axis,
    const math::Vec3d& center)
{
    math::Vec3d factors { 1.0 };
    factors[std::clamp(axis, 0, 2)] = -1.0;
    const math::Transform reflection = math::Transform::scaling(center, factors);
    geom::Selection result = selection;

    for (InstanceId id : selection.instances) {
        Instance inst = model.instance(id);
        inst.transform = reflection * inst.transform;
        model.setInstance(id, inst);
    }

    geom::Mesh& mesh = model.mesh(context);
    const std::set<geom::VertexId> vertices = geom::verticesOf(mesh, selection);
    if (!vertices.empty()) {
        // Normala dorită a fiecărei fețe atinse: cea veche, oglindită (bucla oglindită ar întoarce-o pe dos).
        std::map<geom::FaceId, math::Vec3d> wanted;
        for (geom::VertexId v : vertices) {
            for (geom::EdgeId e : mesh.vertex(v).edges) {
                for (geom::FaceId f : mesh.edge(e).faces) {
                    wanted.emplace(f, reflection.applyToVector(mesh.face(f).plane.normal));
                }
            }
        }
        if (geom::transformVertices(mesh, vertices, reflection)) {
            for (const auto& [face, normal] : wanted) {
                if (!mesh.faces().contains(face)) {
                    continue;
                }
                const geom::FaceId oriented = geom::orientFace(mesh, face, normal);
                if (oriented != face && result.faces.erase(face) > 0) {
                    result.faces.insert(oriented);
                }
            }
        }
    }
    return result;
}

geom::Selection arraySelection(Model& model, DefinitionId context, const geom::Selection& selection,
    const math::Transform& step, int copies)
{
    geom::Selection created;
    math::Transform t = step;
    for (int k = 1; k <= copies; ++k) {
        for (InstanceId id : selection.instances) {
            created.instances.insert(copyLinked(model, id, t));
        }
        if (!selection.edges.empty() || !selection.faces.empty()) {
            const geom::Selection loose { selection.edges, selection.faces, {} };
            const geom::Selection copy = geom::copySelection(model.mesh(context), loose, t);
            created.edges.insert(copy.edges.begin(), copy.edges.end());
            created.faces.insert(copy.faces.begin(), copy.faces.end());
        }
        t = step * t;
    }
    return created;
}

geom::Selection linearArray(Model& model, DefinitionId context, const geom::Selection& selection,
    const math::Vec3d& offset, int copies)
{
    return arraySelection(model, context, selection, math::Transform::translation(offset), copies);
}

geom::Selection polarArray(Model& model, DefinitionId context, const geom::Selection& selection,
    const math::Vec3d& center, const math::Vec3d& axis, double stepRadians, int copies, bool rotateCopies)
{
    if (glm::length(axis) < 1e-12) {
        return {};
    }
    if (rotateCopies) {
        return arraySelection(model, context, selection, math::Transform::rotation(center, axis, stepRadians), copies);
    }
    // Fără rotire: fiecare copie se mută cât s-ar muta centrul selecției pe cerc.
    const auto [lo, hi] = selectionBounds(model, context, selection);
    const math::Vec3d middle = lo.x <= hi.x ? (lo + hi) * 0.5 : center;
    geom::Selection created;
    for (int k = 1; k <= copies; ++k) {
        const math::Vec3d moved = math::Transform::rotation(center, axis, stepRadians * k).applyToPoint(middle);
        const geom::Selection one = arraySelection(model, context, selection, math::Transform::translation(moved - middle), 1);
        created.edges.insert(one.edges.begin(), one.edges.end());
        created.faces.insert(one.faces.begin(), one.faces.end());
        created.instances.insert(one.instances.begin(), one.instances.end());
    }
    return created;
}

int softenEdges(geom::Mesh& mesh, const std::set<geom::EdgeId>& edges, double maxAngleDegrees, bool includeCoplanar)
{
    int softened = 0;
    const double limit = std::clamp(maxAngleDegrees, 0.0, 180.0);
    for (geom::EdgeId id : edges) {
        if (!mesh.edges().contains(id)) {
            continue;
        }
        const geom::Edge& e = mesh.edge(id);
        if (e.faces.size() != 2) {
            continue;
        }
        const math::Vec3d n0 = mesh.face(e.faces[0]).plane.normal;
        const math::Vec3d n1 = mesh.face(e.faces[1]).plane.normal;
        const double angle = glm::degrees(std::acos(std::clamp(glm::dot(n0, n1), -1.0, 1.0)));
        const bool coplanar = angle < 0.01;
        const bool soft = coplanar ? includeCoplanar : angle <= limit + 1e-9;
        if (soft != e.soft) {
            mesh.setEdgeStyle(id, soft, e.curve, e.smoothCurve);
        }
        softened += soft ? 1 : 0;
    }
    return softened;
}

} // namespace aion::model
