#include "aion/core/model/SnapSources.h"

#include "aion/core/model/Operations.h"

#include <glm/common.hpp>

#include <limits>
#include <unordered_map>
#include <utility>

namespace aion::model {

namespace {

// Cutia vârfurilor unui mesh, în coordonatele lui.
std::pair<math::Vec3d, math::Vec3d> meshBox(const geom::Mesh& mesh)
{
    math::Vec3d lo { std::numeric_limits<double>::infinity() };
    math::Vec3d hi { -std::numeric_limits<double>::infinity() };
    for (const geom::Vertex& v : mesh.vertices().values()) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    return { lo, hi };
}

// Cutia aliniată cu axele care cuprinde cutia `box` după transformarea `t` (cele 8 colțuri).
void transformBox(const std::pair<math::Vec3d, math::Vec3d>& box, const math::Transform& t, inference::SnapSource& s)
{
    const auto& [lo, hi] = box;
    if (lo.x > hi.x) {
        return;
    }
    s.lo = math::Vec3d { std::numeric_limits<double>::infinity() };
    s.hi = math::Vec3d { -std::numeric_limits<double>::infinity() };
    for (int i = 0; i < 8; ++i) {
        const math::Vec3d corner { (i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z };
        const math::Vec3d p = t.applyToPoint(corner);
        s.lo = glm::min(s.lo, p);
        s.hi = glm::max(s.hi, p);
    }
}

struct Walker {
    const Model& model;
    const std::vector<InstanceId>& path;
    const std::set<InstanceId>& exclude;
    inference::SnapScene scene;

    void add(DefinitionId def, const math::Transform& world, InstanceId top)
    {
        const geom::Mesh& mesh = model.mesh(def);
        if (mesh.vertices().empty() && mesh.edges().empty() && mesh.faces().empty()) {
            return;
        }
        scene.sources.push_back({ &mesh, world, top });
    }

    // Conținutul unei instanțe din afara căii: `top` e instanța cea mai de sus din afara căii.
    void visit(InstanceId id, const math::Transform& parentWorld, InstanceId top)
    {
        if (exclude.contains(id) || !model.isVisible(id)) {
            return;
        }
        const Instance& inst = model.instance(id);
        const math::Transform world = parentWorld * inst.transform;
        if (!top) {
            top = id;
        }
        add(inst.definition, world, top);
        for (InstanceId child : model.childrenOf(inst.definition)) {
            visit(child, world, top);
        }
    }

    // Un context de pe cale, la adâncimea `depth` (0 = rădăcina); `world` = transformarea lui locală → model.
    void walkContext(DefinitionId def, std::size_t depth, const math::Transform& world)
    {
        if (depth < path.size()) {
            add(def, world, {}); // geometria liberă a unui context părinte
        }
        for (InstanceId child : model.childrenOf(def)) {
            if (depth < path.size() && child == path[depth]) {
                walkContext(model.instance(child).definition, depth + 1, world * model.instance(child).transform);
            } else {
                visit(child, world, {});
            }
        }
    }
};

} // namespace

inference::SnapScene snapScene(const Model& model, const std::vector<InstanceId>& path, const std::set<InstanceId>& exclude)
{
    Walker w { model, path, exclude, {} };
    w.walkContext(model.root(), 0, math::Transform());
    // Din coordonatele modelului în cele ale contextului editat.
    const math::Transform toLocal = worldTransform(model, path).inverse();
    for (inference::SnapSource& s : w.scene.sources) {
        s.toContext = toLocal * s.toContext;
    }
    // Cutiile, o dată pe definiție (multe instanțe pot folosi aceeași).
    std::unordered_map<const geom::Mesh*, std::pair<math::Vec3d, math::Vec3d>> boxes;
    for (inference::SnapSource& s : w.scene.sources) {
        auto it = boxes.find(s.mesh);
        if (it == boxes.end()) {
            it = boxes.emplace(s.mesh, meshBox(*s.mesh)).first;
        }
        transformBox(it->second, s.toContext, s);
    }
    return std::move(w.scene);
}

} // namespace aion::model
