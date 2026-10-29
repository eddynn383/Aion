#include "aion/core/model/SnapSources.h"

#include "aion/core/model/Operations.h"

namespace aion::model {

namespace {

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
    return std::move(w.scene);
}

} // namespace aion::model
