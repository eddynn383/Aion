#include "aion/core/model/Copying.h"

#include "aion/core/geom/Operations.h"

#include <algorithm>
#include <functional>
#include <set>

namespace aion::model {

namespace {

geom::Selection everything(const geom::Mesh& mesh)
{
    geom::Selection s;
    s.edges.insert(mesh.edges().keys().begin(), mesh.edges().keys().end());
    s.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
    return s;
}

// Țintele acțiunilor (recursiv în Alternează) trec pe copii, după `instances`.
void remapTargets(std::vector<Action>& actions, const std::map<InstanceId, InstanceId>& instances)
{
    for (Action& a : actions) {
        if (const auto it = instances.find(a.target); it != instances.end()) {
            a.target = it->second;
        }
        remapTargets(a.first, instances);
        remapTargets(a.second, instances);
    }
}

void remapRules(Model& model, InstanceId id, const std::map<InstanceId, InstanceId>& instances)
{
    Instance inst = model.instance(id);
    if (inst.rules.empty()) {
        return;
    }
    const std::vector<Rule> before = inst.rules;
    for (Rule& r : inst.rules) {
        remapTargets(r.actions, instances);
    }
    if (inst.rules != before) {
        model.setInstance(id, inst);
    }
}

// Instanțele subarborelui (fiecare id o dată: copiii unei definiții folosite de două ori în subarbore sunt aceleași).
std::set<InstanceId> subtree(const Model& model, InstanceId root)
{
    std::set<InstanceId> out;
    std::vector<InstanceId> stack { root };
    while (!stack.empty()) {
        const InstanceId id = stack.back();
        stack.pop_back();
        if (!out.insert(id).second) {
            continue;
        }
        for (InstanceId c : model.childrenOf(model.instance(id).definition)) {
            stack.push_back(c);
        }
    }
    return out;
}

bool usedOutside(const Model& model, DefinitionId definition, const std::set<InstanceId>& inside)
{
    const std::vector<InstanceId> users = model.instancesOf(definition);
    return std::any_of(users.begin(), users.end(), [&](InstanceId i) { return !inside.contains(i); });
}

// O copie a definiției: geometria și instanțele din ea (cu id-uri noi, aceleași definiții); perechile vechi → noi.
DefinitionId cloneShallow(Model& model, DefinitionId source, std::map<InstanceId, InstanceId>& instances)
{
    const Definition copy = model.definition(source);
    const std::vector<InstanceId> children = model.childrenOf(source);
    const DefinitionId id = model.addDefinition(copy);
    geom::copyEntities(model.mesh(source), everything(model.mesh(source)), model.mesh(id), {});
    for (InstanceId child : children) {
        Instance c = model.instance(child);
        c.parent = id;
        instances[child] = model.addInstance(c);
    }
    return id;
}

} // namespace

CopyResult copyDeep(Model& target, DefinitionId context, const Model& source, const std::vector<InstanceId>& sources,
    const math::Transform& delta)
{
    // Tot ce trebuie din sursă, citit înainte de orice scriere (sursa poate fi chiar ținta).
    struct DefinitionData {
        DefinitionId id;
        Definition value;
        std::vector<std::pair<InstanceId, Instance>> children;
    };
    std::vector<DefinitionData> definitions;
    std::set<DefinitionId> seen;
    const std::function<void(DefinitionId)> gather = [&](DefinitionId d) {
        if (!seen.insert(d).second) {
            return;
        }
        DefinitionData data { d, source.definition(d), {} };
        for (InstanceId c : source.childrenOf(d)) {
            data.children.emplace_back(c, source.instance(c));
        }
        const std::vector<std::pair<InstanceId, Instance>> children = data.children;
        definitions.push_back(std::move(data));
        for (const auto& [id, inst] : children) {
            gather(inst.definition);
        }
    };
    std::vector<std::pair<InstanceId, Instance>> roots;
    for (InstanceId id : sources) {
        if (source.instances().contains(id)) {
            roots.emplace_back(id, source.instance(id));
            gather(source.instance(id).definition);
        }
    }

    // Definițiile noi (cu geometria), apoi instanțele din ele, apoi copiile de sus.
    std::map<DefinitionId, DefinitionId> newDefinition;
    for (const DefinitionData& d : definitions) {
        const DefinitionId id = target.addDefinition(d.value);
        geom::copyEntities(source.mesh(d.id), everything(source.mesh(d.id)), target.mesh(id), {});
        newDefinition[d.id] = id;
    }
    CopyResult result;
    for (const DefinitionData& d : definitions) {
        for (auto [id, inst] : d.children) {
            inst.parent = newDefinition.at(d.id);
            inst.definition = newDefinition.at(inst.definition);
            result.instances[id] = target.addInstance(inst);
        }
    }
    for (auto [id, inst] : roots) {
        inst.parent = context;
        inst.definition = newDefinition.at(inst.definition);
        inst.transform = delta * inst.transform;
        const InstanceId copy = target.addInstance(inst);
        result.instances[id] = copy;
        result.roots.push_back(copy);
    }
    for (const auto& [from, to] : result.instances) {
        remapRules(target, to, result.instances);
    }
    return result;
}

InstanceId copyLinked(Model& model, InstanceId id, const math::Transform& delta)
{
    Instance copy = model.instance(id);
    copy.transform = delta * copy.transform;
    Definition d = model.definition(copy.definition);
    if (d.group && !d.collection) {
        d.group = false;
        model.setDefinition(copy.definition, d);
    }
    return model.addInstance(copy);
}

CopyResult makeUniqueDeep(Model& model, InstanceId instance)
{
    CopyResult result;
    result.roots.push_back(instance);
    result.instances[instance] = instance;
    std::map<DefinitionId, DefinitionId> cloned;
    const std::function<void(InstanceId)> process = [&](InstanceId id) {
        Instance inst = model.instance(id);
        DefinitionId d = inst.definition;
        if (const auto it = cloned.find(d); it != cloned.end()) {
            inst.definition = it->second; // aceeași definiție de două ori în subarbore: aceeași copie
            model.setInstance(id, inst);
            return;
        }
        if (usedOutside(model, d, subtree(model, instance))) {
            const DefinitionId copy = cloneShallow(model, d, result.instances);
            cloned[d] = copy;
            inst.definition = copy;
            model.setInstance(id, inst);
            d = copy;
        }
        for (InstanceId c : model.childrenOf(d)) {
            process(c);
        }
    };
    process(instance);
    for (InstanceId id : subtree(model, instance)) {
        remapRules(model, id, result.instances);
    }
    return result;
}

bool canMakeUnique(const Model& model, InstanceId instance)
{
    if (!model.instances().contains(instance)) {
        return false;
    }
    const std::set<InstanceId> inside = subtree(model, instance);
    return std::any_of(inside.begin(), inside.end(),
        [&](InstanceId id) { return usedOutside(model, model.instance(id).definition, inside); });
}

std::vector<std::pair<math::Vec3d, math::Vec3d>> instanceOutline(const Model& model, const std::vector<InstanceId>& instances)
{
    std::vector<std::pair<math::Vec3d, math::Vec3d>> out;
    const std::function<void(DefinitionId, const math::Transform&)> walk = [&](DefinitionId d, const math::Transform& t) {
        const geom::Mesh& mesh = model.mesh(d);
        for (const geom::Edge& e : mesh.edges().values()) {
            out.emplace_back(t.applyToPoint(mesh.position(e.v0)), t.applyToPoint(mesh.position(e.v1)));
        }
        for (InstanceId c : model.childrenOf(d)) {
            const Instance& child = model.instance(c);
            if (!child.hidden) {
                walk(child.definition, t * child.transform);
            }
        }
    };
    for (InstanceId id : instances) {
        if (model.instances().contains(id)) {
            walk(model.instance(id).definition, model.instance(id).transform);
        }
    }
    return out;
}

} // namespace aion::model
