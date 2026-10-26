#include "aion/core/model/Operations.h"

#include "aion/core/geom/Operations.h"

#include <glm/common.hpp>

#include <limits>

namespace aion::model {

namespace {

geom::Selection everything(const geom::Mesh& mesh)
{
    geom::Selection s;
    s.edges.insert(mesh.edges().keys().begin(), mesh.edges().keys().end());
    s.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
    return s;
}

std::string defaultName(const Model& model, bool component)
{
    if (!component) {
        return "Grup";
    }
    int count = 0;
    for (const Definition& d : model.definitions().values()) {
        count += d.group ? 0 : 1;
    }
    return "Obiect " + std::to_string(count); // rădăcina nu e grup, deci primul obiect e „Obiect 1”
}

// O copie independentă a definiției (geometrie și instanțele din ea).
DefinitionId cloneDefinition(Model& model, DefinitionId source)
{
    Definition copy = model.definition(source);
    if (!copy.group) {
        copy.name += " (copie)";
    }
    const DefinitionId id = model.addDefinition(copy);
    geom::copyEntities(model.mesh(source), everything(model.mesh(source)), model.mesh(id), {});
    for (InstanceId child : model.childrenOf(source)) {
        Instance c = model.instance(child);
        c.parent = id;
        model.addInstance(c);
    }
    return id;
}

} // namespace

math::Transform worldTransform(const Model& model, const std::vector<InstanceId>& path)
{
    math::Transform t;
    for (InstanceId id : path) {
        t = t * model.instance(id).transform;
    }
    return t;
}

InstanceId makeGroup(Model& model, DefinitionId context, const geom::Selection& selection, bool component, std::string name)
{
    if (selection.empty()) {
        return {};
    }
    const DefinitionId definition
        = model.addDefinition({ name.empty() ? defaultName(model, component) : std::move(name), !component });
    geom::Mesh& source = model.mesh(context);
    geom::Mesh& target = model.mesh(definition);

    const geom::Selection geometry { selection.edges, selection.faces, {} };
    geom::copyEntities(source, geometry, target, {});

    // Din context dispar fețele selectate și muchiile rămase fără fețe (cele folosite și de alte fețe rămân).
    std::set<geom::EdgeId> candidates(selection.edges.begin(), selection.edges.end());
    for (geom::FaceId f : selection.faces) {
        if (const geom::Face* face = source.faces().find(f)) {
            for (const geom::EdgeUse& u : face->outer.uses) {
                candidates.insert(u.edge);
            }
            for (const geom::Loop& h : face->inner) {
                for (const geom::EdgeUse& u : h.uses) {
                    candidates.insert(u.edge);
                }
            }
        }
    }
    for (geom::FaceId f : selection.faces) {
        if (source.faces().contains(f)) {
            geom::eraseFace(source, f);
        }
    }
    for (geom::EdgeId e : candidates) {
        if (source.edges().contains(e) && source.edge(e).faces.empty()) {
            geom::eraseEdge(source, e);
        }
    }

    for (InstanceId i : selection.instances) {
        if (model.instances().contains(i) && model.instance(i).parent == context) {
            Instance inst = model.instance(i);
            inst.parent = definition;
            model.setInstance(i, inst);
        }
    }
    return model.addInstance({ context, definition, {}, {}, {}, false, false, {}, {} });
}

geom::Selection explode(Model& model, InstanceId id)
{
    const Instance inst = model.instance(id);
    const DefinitionId definition = inst.definition;
    const bool shared = model.instancesOf(definition).size() > 1;

    geom::Selection result = geom::copyEntities(
        model.mesh(definition), everything(model.mesh(definition)), model.mesh(inst.parent), inst.transform);
    for (InstanceId child : model.childrenOf(definition)) {
        Instance c = model.instance(child);
        c.parent = inst.parent;
        c.transform = inst.transform * c.transform;
        if (shared) {
            result.instances.insert(model.addInstance(c)); // celelalte copii ale componentei își păstrează copiii
        } else {
            model.setInstance(child, c);
            result.instances.insert(child);
        }
    }
    model.removeInstance(id);
    if (model.instancesOf(definition).empty()) {
        model.removeDefinition(definition);
    }
    return result;
}

DefinitionId makeUnique(Model& model, InstanceId id)
{
    Instance inst = model.instance(id);
    if (model.instancesOf(inst.definition).size() <= 1) {
        return inst.definition;
    }
    inst.definition = cloneDefinition(model, inst.definition);
    model.setInstance(id, inst);
    return inst.definition;
}

InstanceId copyInstance(Model& model, InstanceId id, const math::Transform& delta)
{
    Instance copy = model.instance(id);
    copy.transform = delta * copy.transform;
    if (model.definition(copy.definition).group) {
        copy.definition = cloneDefinition(model, copy.definition);
    }
    return model.addInstance(copy);
}

void eraseInstance(Model& model, InstanceId id)
{
    const DefinitionId definition = model.instance(id).definition;
    model.removeInstance(id);
    if (!model.definition(definition).group || !model.instancesOf(definition).empty()) {
        return;
    }
    for (InstanceId child : model.childrenOf(definition)) {
        eraseInstance(model, child);
    }
    model.removeDefinition(definition);
}

void transformInstances(Model& model, const std::set<InstanceId>& instances, const math::Transform& delta)
{
    for (InstanceId id : instances) {
        Instance inst = model.instance(id);
        inst.transform = delta * inst.transform;
        model.setInstance(id, inst);
    }
}

std::pair<math::Vec3d, math::Vec3d> bounds(const Model& model, DefinitionId definition)
{
    constexpr double kInf = std::numeric_limits<double>::infinity();
    math::Vec3d lo { kInf };
    math::Vec3d hi { -kInf };
    for (const geom::Vertex& v : model.mesh(definition).vertices().values()) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    for (InstanceId child : model.childrenOf(definition)) {
        const Instance& c = model.instance(child);
        const auto [clo, chi] = bounds(model, c.definition);
        if (clo.x > chi.x) {
            continue;
        }
        for (int k = 0; k < 8; ++k) {
            const math::Vec3d corner { (k & 1) ? chi.x : clo.x, (k & 2) ? chi.y : clo.y, (k & 4) ? chi.z : clo.z };
            const math::Vec3d p = c.transform.applyToPoint(corner);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
    }
    return { lo, hi };
}

} // namespace aion::model
