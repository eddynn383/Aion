#include "aion/core/view/ModelPicking.h"
#include "aion/core/view/Picking.h"

#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/model/Operations.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <limits>

namespace aion::view {

namespace {

// Cel mai apropiat punct atins în definiție (geometria ei și instanțele din ea), în coordonatele ei.
std::optional<math::Vec3d> nearestHit(
    const model::Model& model, model::DefinitionId definition, const Camera& camera, Vec2d screen)
{
    const math::Ray ray = camera.rayAt(screen);
    std::optional<math::Vec3d> best;
    double bestT = std::numeric_limits<double>::infinity();
    const geom::Mesh& mesh = model.mesh(definition);
    // Toată definiția ratată: nimic de testat înăuntru.
    const auto [blo, bhi] = model::bounds(model, definition);
    if (blo.x > bhi.x || !rayHitsBox(ray, blo, bhi, 1.0)) {
        return std::nullopt;
    }
    for (geom::FaceId f : mesh.faces().keys()) {
        const geom::FaceTriangles tri = geom::triangulateFace(mesh, f);
        for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
            const auto t = math::intersectRayTriangle(
                ray, tri.points[tri.indices[i]], tri.points[tri.indices[i + 1]], tri.points[tri.indices[i + 2]]);
            if (t && *t < bestT) {
                bestT = *t;
                best = ray.at(*t);
            }
        }
    }
    for (model::InstanceId child : model.childrenOf(definition)) {
        if (!model.isVisible(child)) {
            continue;
        }
        const model::Instance& inst = model.instance(child);
        if (const auto p = nearestHit(model, inst.definition, camera.inContext(inst.transform), screen)) {
            const math::Vec3d q = inst.transform.applyToPoint(*p);
            const double t = glm::dot(q - ray.origin, ray.direction);
            if (t < bestT) {
                bestT = t;
                best = q;
            }
        }
    }
    return best;
}

} // namespace

std::optional<InstanceHit> pickInstance(
    const model::Model& model, model::DefinitionId context, const Camera& camera, Vec2d screen)
{
    const math::Ray ray = camera.rayAt(screen);
    std::optional<InstanceHit> best;
    for (model::InstanceId id : model.childrenOf(context)) {
        if (!model.isVisible(id)) {
            continue;
        }
        const model::Instance& inst = model.instance(id);
        if (const auto p = nearestHit(model, inst.definition, camera.inContext(inst.transform), screen)) {
            const math::Vec3d q = inst.transform.applyToPoint(*p);
            const double t = glm::dot(q - ray.origin, ray.direction);
            if (!best || t < best->t) {
                best = InstanceHit { id, t, q };
            }
        }
    }
    return best;
}

std::set<model::InstanceId> pickInstancesInRectangle(const model::Model& model, model::DefinitionId context,
    const Camera& camera, Vec2d corner0, Vec2d corner1, bool crossing)
{
    const Vec2d lo = glm::min(corner0, corner1);
    const Vec2d hi = glm::max(corner0, corner1);
    std::set<model::InstanceId> out;
    for (model::InstanceId id : model.childrenOf(context)) {
        if (!model.isVisible(id)) {
            continue;
        }
        const model::Instance& inst = model.instance(id);
        const auto [blo, bhi] = model::bounds(model, inst.definition);
        if (blo.x > bhi.x) {
            continue;
        }
        Vec2d slo { std::numeric_limits<double>::infinity() };
        Vec2d shi { -std::numeric_limits<double>::infinity() };
        bool allVisible = true;
        for (int k = 0; k < 8; ++k) {
            const math::Vec3d corner { (k & 1) ? bhi.x : blo.x, (k & 2) ? bhi.y : blo.y, (k & 4) ? bhi.z : blo.z };
            const auto s = camera.project(inst.transform.applyToPoint(corner));
            if (!s) {
                allVisible = false;
                continue;
            }
            slo = glm::min(slo, *s);
            shi = glm::max(shi, *s);
        }
        const bool inside = allVisible && slo.x >= lo.x && slo.y >= lo.y && shi.x <= hi.x && shi.y <= hi.y;
        const bool touches = slo.x <= hi.x && shi.x >= lo.x && slo.y <= hi.y && shi.y >= lo.y;
        if (crossing ? touches : inside) {
            out.insert(id);
        }
    }
    return out;
}

} // namespace aion::view
