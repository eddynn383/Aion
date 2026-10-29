#include "aion/core/model/Copying.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Operations.h"

#include <algorithm>
#include <functional>
#include <set>
#include <string>

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

// Animațiile pornite de blocurile PlayAnimation (recursiv în Alternează).
void collectPlayed(const std::vector<Action>& actions, std::set<AnimationId>& out)
{
    for (const Action& a : actions) {
        if (a.kind == ActionKind::PlayAnimation && a.animation) {
            out.insert(a.animation);
        }
        collectPlayed(a.first, out);
        collectPlayed(a.second, out);
    }
}

bool remapPlayed(std::vector<Action>& actions, const std::map<AnimationId, AnimationId>& animations)
{
    bool changed = false;
    for (Action& a : actions) {
        if (a.kind == ActionKind::PlayAnimation) {
            if (const auto it = animations.find(a.animation); it != animations.end()) {
                a.animation = it->second;
                changed = true;
            }
        }
        changed = remapPlayed(a.first, animations) || changed;
        changed = remapPlayed(a.second, animations) || changed;
    }
    return changed;
}

// După fiecare bloc care pornește `from`, unul identic care pornește `to`.
bool addPlayed(std::vector<Action>& actions, AnimationId from, AnimationId to)
{
    bool changed = false;
    for (std::size_t i = 0; i < actions.size(); ++i) {
        changed = addPlayed(actions[i].first, from, to) || changed;
        changed = addPlayed(actions[i].second, from, to) || changed;
        if (actions[i].kind == ActionKind::PlayAnimation && actions[i].animation == from) {
            Action extra = actions[i];
            extra.animation = to;
            actions.insert(actions.begin() + static_cast<std::ptrdiff_t>(i) + 1, extra);
            ++i;
            changed = true;
        }
    }
    return changed;
}

// „<nume> (copie)”, apoi „(copie 2)”…; copia unei copii pornește de la numele original.
std::string copyName(const Model& model, std::string name)
{
    if (const auto at = name.rfind(" (copie"); at != std::string::npos && !name.empty() && name.back() == ')') {
        name.erase(at);
    }
    const auto taken = [&](const std::string& candidate) {
        const auto& all = model.animations().values();
        return std::any_of(all.begin(), all.end(), [&](const Animation& a) { return a.name == candidate; });
    };
    std::string candidate = name + " (copie)";
    for (int n = 2; taken(candidate); ++n) {
        candidate = name + " (copie " + std::to_string(n) + ")";
    }
    return candidate;
}

// Cheile de poziție ale pistei (punctul pivotului, în coordonatele părintelui) trec prin `delta`.
void moveTrack(ObjectTrack& track, const math::Transform& delta, const math::Vec3d& base)
{
    const ObjectTrack original = track;
    const auto valueAt = [&](Property p, double time, double fallback) {
        const Channel* c = findChannel(original, p);
        const std::optional<double> v = c ? evaluateChannel(*c, time) : std::nullopt;
        return v.value_or(fallback);
    };
    for (Channel& c : track.channels) {
        const int axis = c.property == Property::PositionX ? 0 : c.property == Property::PositionY ? 1
            : c.property == Property::PositionZ                                                  ? 2
                                                                                                 : -1;
        if (axis < 0) {
            continue;
        }
        for (Key& k : c.keys) {
            const math::Vec3d p { valueAt(Property::PositionX, k.time, base.x), valueAt(Property::PositionY, k.time, base.y),
                valueAt(Property::PositionZ, k.time, base.z) };
            k.value = delta.applyToPoint(p)[axis];
        }
    }
}

bool containsAny(const std::set<InstanceId>& inside, const std::set<InstanceId>& instances)
{
    return std::any_of(instances.begin(), instances.end(), [&](InstanceId i) { return inside.contains(i); });
}

std::set<InstanceId> animated(const Animation& a)
{
    std::set<InstanceId> out;
    for (const ObjectTrack& t : a.tracks) {
        out.insert(t.instance);
    }
    return out;
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

void copyAnimations(
    Model& target, const Model& source, const CopyResult& copy, const math::Transform& delta, CopyKind kind)
{
    // Din sursă (poate fi ținta): animațiile pornite de regulile subarborelui și pistele de copiat.
    std::set<AnimationId> started;
    std::set<InstanceId> roots;
    for (const auto& [from, to] : copy.instances) {
        if (source.instances().contains(from)) {
            for (const Rule& r : source.instance(from).rules) {
                collectPlayed(r.actions, started);
            }
        }
        if (std::find(copy.roots.begin(), copy.roots.end(), to) != copy.roots.end()) {
            roots.insert(from);
        }
    }
    struct Planned {
        AnimationId id;
        Animation value;
        std::vector<ObjectTrack> tracks;
        bool movesRoot = false; // Unique: pista obiectului însuși trece pe copie
    };
    std::vector<Planned> planned;
    for (std::size_t i = 0; i < source.animations().size(); ++i) {
        const AnimationId id = source.animations().keys()[i];
        const Animation& a = source.animations().values()[i];
        const bool isStarted = started.contains(id);
        Planned p { id, a, {}, false };
        for (const ObjectTrack& t : a.tracks) {
            const auto it = copy.instances.find(t.instance);
            if (it == copy.instances.end()) {
                continue;
            }
            if (it->first == it->second) { // Unique: obiectul rămâne același
                if (isStarted) {
                    p.tracks.push_back(t);
                    p.movesRoot = true;
                }
                continue;
            }
            ObjectTrack moved = t;
            moved.instance = it->second;
            if (kind == CopyKind::Copy && roots.contains(t.instance)) {
                moveTrack(moved, delta, transformValues(source.instance(t.instance).transform,
                    pivotLocal(source, t.instance)).position);
            }
            p.tracks.push_back(std::move(moved));
        }
        if (!p.tracks.empty()) {
            planned.push_back(std::move(p));
        }
    }

    // Animațiile noi (pornite de reguli) și pistele adăugate la cele din timeline.
    std::map<AnimationId, AnimationId> copies;
    for (const Planned& p : planned) {
        if (started.contains(p.id)) {
            Animation a = p.value;
            a.name = copyName(target, a.name);
            a.tracks = p.tracks;
            a.camera.clear();
            copies[p.id] = target.addAnimation(a);
            if (p.movesRoot && target.animations().contains(p.id)) {
                Animation original = target.animations().at(p.id);
                std::erase_if(original.tracks, [&](const ObjectTrack& t) { return copy.instances.contains(t.instance)
                    && copy.instances.at(t.instance) == t.instance; });
                target.setAnimation(p.id, original);
            }
        } else if (target.animations().contains(p.id)) {
            Animation a = target.animations().at(p.id);
            a.tracks.insert(a.tracks.end(), p.tracks.begin(), p.tracks.end());
            target.setAnimation(p.id, a);
        }
    }
    if (copies.empty()) {
        return;
    }

    // Regulile: ale copiilor trec pe animațiile noi; la Fă unic, și cele din afară, după ce conțin.
    std::set<InstanceId> inside;
    for (const auto& [from, to] : copy.instances) {
        inside.insert(to);
    }
    if (kind == CopyKind::Unique) {
        for (InstanceId root : copy.roots) {
            const std::set<InstanceId> below = subtree(target, root);
            inside.insert(below.begin(), below.end());
        }
    }
    for (InstanceId id : inside) {
        Instance inst = target.instance(id);
        bool changed = false;
        for (Rule& r : inst.rules) {
            changed = remapPlayed(r.actions, copies) || changed;
        }
        if (changed) {
            target.setInstance(id, inst);
        }
    }
    if (kind != CopyKind::Unique) {
        return;
    }
    const std::vector<InstanceId> all(target.instances().keys().begin(), target.instances().keys().end());
    for (InstanceId id : all) {
        if (inside.contains(id) || target.instance(id).rules.empty()) {
            continue;
        }
        Instance inst = target.instance(id);
        const std::set<InstanceId> below = subtree(target, id);
        bool changed = false;
        for (const auto& [from, to] : copies) {
            const bool hasOriginal = containsAny(below, animated(target.animations().at(from)));
            const bool hasCopy = containsAny(below, animated(target.animations().at(to)));
            for (Rule& r : inst.rules) {
                if (hasCopy && !hasOriginal) {
                    changed = remapPlayed(r.actions, { { from, to } }) || changed;
                } else if (hasCopy == hasOriginal) {
                    changed = addPlayed(r.actions, from, to) || changed;
                }
            }
        }
        if (changed) {
            target.setInstance(id, inst);
        }
    }
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
