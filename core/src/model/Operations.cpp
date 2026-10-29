#include "aion/core/model/Operations.h"

#include "aion/core/model/AnimationEval.h"
#include "aion/core/view/Camera.h"

#include "aion/core/geom/Operations.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/matrix.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

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
    return model.addInstance({ context, definition, {}, {}, {}, false, false, {}, {}, {} });
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
        const std::optional<Placement> before = inst.placementLocks != 0 ? std::optional(placementOf(model, id)) : std::nullopt;
        inst.transform = delta * inst.transform;
        model.setInstance(id, inst);
        if (before) {
            restoreLockedPlacement(model, id, *before);
        }
    }
}

std::pair<math::Vec3d, math::Vec3d> bounds(const Model& model, DefinitionId definition)
{
    BoundsCache cache;
    return bounds(model, definition, cache);
}

std::pair<math::Vec3d, math::Vec3d> bounds(const Model& model, DefinitionId definition, BoundsCache& cache)
{
    if (const auto it = cache.find(definition.value); it != cache.end()) {
        return it->second;
    }
    constexpr double kInf = std::numeric_limits<double>::infinity();
    math::Vec3d lo { kInf };
    math::Vec3d hi { -kInf };
    for (const geom::Vertex& v : model.mesh(definition).vertices().values()) {
        lo = glm::min(lo, v.position);
        hi = glm::max(hi, v.position);
    }
    for (InstanceId child : model.childrenOf(definition)) {
        const Instance& c = model.instance(child);
        const auto [clo, chi] = bounds(model, c.definition, cache);
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
    cache.emplace(definition.value, std::pair { lo, hi });
    return { lo, hi };
}

Decomposed decompose(const math::Transform& t)
{
    const glm::dmat3 m(t.matrix());
    Decomposed d;
    for (int i = 0; i < 3; ++i) {
        const double length = glm::length(m[i]);
        d.scale[i] = length;
        d.rotation[i] = length > 1e-12 ? m[i] / length : math::Vec3d { i == 0, i == 1, i == 2 };
    }
    // O oglindire apare ca scară negativă pe X (rotația rămâne o rotație adevărată).
    if (glm::determinant(d.rotation) < 0.0) {
        d.rotation[0] = -d.rotation[0];
        d.scale.x = -d.scale.x;
    }
    return d;
}

math::Vec3d eulerDegrees(const glm::dmat3& r)
{
    const double sy = std::clamp(-r[0].z, -1.0, 1.0);
    const double y = std::asin(sy);
    double x = 0.0;
    double z = 0.0;
    if (std::abs(sy) < 1.0 - 1e-9) {
        x = std::atan2(r[1].z, r[2].z);
        z = std::atan2(r[0].y, r[0].x);
    } else {
        z = std::atan2(-r[1].x, r[1].y);
    }
    const auto clean = [](double degrees) { return std::abs(degrees) < 1e-9 ? 0.0 : degrees; };
    return { clean(glm::degrees(x)), clean(glm::degrees(y)), clean(glm::degrees(z)) };
}

glm::dmat3 rotationFromEuler(const math::Vec3d& degrees)
{
    const glm::dmat4 m = glm::rotate(glm::dmat4(1.0), glm::radians(degrees.z), math::Vec3d { 0, 0, 1 })
        * glm::rotate(glm::dmat4(1.0), glm::radians(degrees.y), math::Vec3d { 0, 1, 0 })
        * glm::rotate(glm::dmat4(1.0), glm::radians(degrees.x), math::Vec3d { 1, 0, 0 });
    return glm::dmat3(m);
}

Placement placementOf(const Model& model, InstanceId instance)
{
    const Instance& inst = model.instance(instance);
    const auto [lo, hi] = bounds(model, inst.definition);
    const math::Vec3d corner = lo.x <= hi.x ? lo : math::Vec3d { 0.0 };
    const math::Vec3d extent = lo.x <= hi.x ? hi - lo : math::Vec3d { 0.0 };
    const Decomposed d = decompose(inst.transform);
    Placement p;
    p.position = inst.transform.applyToPoint(pivotLocal(model, instance));
    p.rotation = eulerDegrees(d.rotation);
    p.scale = d.scale;
    p.size = glm::abs(extent * d.scale);
    p.pivot = (pivotLocal(model, instance) - corner) * glm::abs(d.scale);
    return p;
}

math::Vec3d pivotLocal(const Model& model, InstanceId instance)
{
    const Instance& inst = model.instance(instance);
    if (inst.pivot) {
        return *inst.pivot;
    }
    const auto [lo, hi] = bounds(model, inst.definition);
    return lo.x <= hi.x ? (lo + hi) * 0.5 : math::Vec3d { 0.0 };
}

math::Vec3d pivotPoint(const Model& model, InstanceId instance)
{
    return model.instance(instance).transform.applyToPoint(pivotLocal(model, instance));
}

namespace {

// Animațiile instanței după un pivot nou: obiectul rămâne pe loc la fiecare cheie.
void rebaseAnimations(Model& model, InstanceId instance, const math::Transform& base, const math::Vec3d& oldPivot,
    const math::Vec3d& newPivot)
{
    if (glm::length(newPivot - oldPivot) < 1e-12) {
        return;
    }
    std::vector<std::pair<AnimationId, Animation>> changed;
    for (std::size_t i = 0; i < model.animations().size(); ++i) {
        const Animation& a = model.animations().values()[i];
        if (findTrack(a, instance) != nullptr) {
            Animation copy = a;
            rebasePivot(copy, instance, base, oldPivot, newPivot);
            changed.emplace_back(model.animations().keys()[i], std::move(copy));
        }
    }
    for (auto& [id, a] : changed) {
        model.setAnimation(id, std::move(a));
    }
}

} // namespace

void setPivot(Model& model, InstanceId instance, const math::Vec3d& local)
{
    const math::Vec3d old = pivotLocal(model, instance);
    Instance inst = model.instance(instance);
    inst.pivot = local;
    model.setInstance(instance, inst);
    rebaseAnimations(model, instance, inst.transform, old, local);
}

void setPlacement(Model& model, InstanceId instance, const Placement& wanted)
{
    const Placement current = placementOf(model, instance);
    Instance inst = model.instance(instance);
    const auto [lo, hi] = bounds(model, inst.definition);
    const math::Vec3d corner = lo.x <= hi.x ? lo : math::Vec3d { 0.0 };
    const math::Vec3d extent = lo.x <= hi.x ? hi - lo : math::Vec3d { 0.0 };
    Decomposed d = decompose(inst.transform);

    // Pivotul nou, din distanța față de colț (mm pe axele obiectului) în coordonatele definiției.
    if (wanted.pivot && current.pivot && glm::length(*wanted.pivot - *current.pivot) > 1e-7) {
        math::Vec3d local = corner;
        for (int i = 0; i < 3; ++i) {
            local[i] += (*wanted.pivot)[i] / std::max(std::abs(d.scale[i]), 1e-9);
        }
        inst.pivot = local;
    }
    const math::Vec3d pivot = inst.pivot ? *inst.pivot : (lo.x <= hi.x ? (lo + hi) * 0.5 : math::Vec3d { 0.0 });
    const math::Vec3d pivotWorld = inst.transform.applyToPoint(pivot);

    // Rotația: refăcută din unghiuri doar dacă s-a schimbat (altfel rămâne exact cea existentă, fără erori de rotunjire).
    if (glm::length(wanted.rotation - current.rotation) > 1e-7) {
        d.rotation = rotationFromEuler(wanted.rotation);
    }

    math::Vec3d scale = wanted.scale;
    for (int i = 0; i < 3; ++i) {
        if (wanted.size[i] > 0.0 && extent[i] > 1e-9) {
            scale[i] = std::copysign(wanted.size[i] / extent[i], scale[i] == 0.0 ? 1.0 : scale[i]);
        }
        if (std::abs(scale[i]) < 1e-9) {
            scale[i] = 1e-9; // o scară nulă ar turti obiectul ireversibil
        }
    }
    glm::dmat3 rs = d.rotation;
    for (int i = 0; i < 3; ++i) {
        rs[i] *= scale[i];
    }
    glm::dmat4 m(rs);
    if (glm::length(wanted.position - current.position) > 1e-6) {
        // O mutare: pivotul ajunge în poziția cerută.
        m[3] = glm::dvec4(wanted.position - rs * pivot, 1.0);
    } else {
        // Rotație / scară / dimensiuni: pivotul rămâne unde era.
        m[3] = glm::dvec4(pivotWorld - rs * pivot, 1.0);
    }
    const math::Transform before = inst.transform;
    const math::Vec3d oldPivot = pivotLocal(model, instance);
    inst.transform = math::Transform(m);
    model.setInstance(instance, inst);
    rebaseAnimations(model, instance, before, oldPivot, pivot);
}

void restoreLockedPlacement(Model& model, InstanceId instance, const Placement& before)
{
    const std::uint16_t locks = model.instance(instance).placementLocks;
    if (locks == 0) {
        return;
    }
    const Placement after = placementOf(model, instance);
    Placement wanted = after;
    wanted.size = math::Vec3d { 0.0 };
    for (int i = 0; i < 3; ++i) {
        const auto locked = [&](int group) { return (locks & (1u << (group * 3 + i))) != 0; };
        if (locked(0)) {
            wanted.position[i] = before.position[i];
        }
        if (locked(1)) {
            wanted.size[i] = before.size[i];
        }
        if (locked(2)) {
            wanted.rotation[i] = before.rotation[i];
        }
        if (locked(3)) {
            wanted.scale[i] = before.scale[i];
        }
    }
    setPlacement(model, instance, wanted);
}

InstanceId groupInstances(Model& model, DefinitionId context, const std::set<InstanceId>& instances, std::string name)
{
    if (instances.empty()) {
        return {};
    }
    geom::Selection selection;
    selection.instances = instances;
    const InstanceId group = makeGroup(model, context, selection, false, std::move(name));
    if (group) {
        const DefinitionId def = model.instance(group).definition;
        Definition d = model.definition(def);
        d.collection = true;
        if (d.name.empty()) {
            d.name = "Grup";
        }
        model.setDefinition(def, d);
    }
    return group;
}

math::Transform definitionWorldTransform(const Model& model, DefinitionId definition)
{
    math::Transform world;
    while (definition != model.root()) {
        const auto owners = model.instancesOf(definition);
        if (owners.empty()) {
            break;
        }
        const Instance& inst = model.instance(owners.front());
        world = inst.transform * world;
        definition = inst.parent;
    }
    return world;
}

bool moveToParent(Model& model, InstanceId instance, DefinitionId target)
{
    Instance inst = model.instance(instance);
    if (inst.parent == target || !model.definitions().contains(target)) {
        return false;
    }
    if (target != model.root() && model.instancesOf(target).size() != 1) {
        return false; // un obiect cu copii: nu e clar în care copie
    }
    // Ținta nu poate fi instanța însăși sau ceva din interiorul ei (ar face un ciclu).
    for (DefinitionId d = target; d != model.root();) {
        if (d == inst.definition) {
            return false;
        }
        const auto owners = model.instancesOf(d);
        if (owners.empty()) {
            break;
        }
        d = model.instance(owners.front()).parent;
    }
    const math::Transform world = definitionWorldTransform(model, inst.parent) * inst.transform;
    inst.transform = definitionWorldTransform(model, target).inverse() * world;
    inst.parent = target;
    model.setInstance(instance, inst);
    return true;
}

void captureSceneState(const Model& model, Scene& scene)
{
    scene.hasState = true;
    scene.transforms.clear();
    scene.hiddenInstances.clear();
    scene.hiddenTags.clear();
    for (std::size_t i = 0; i < model.instances().size(); ++i) {
        const InstanceId id = model.instances().keys()[i];
        const Instance& inst = model.instances().values()[i];
        scene.transforms[id] = inst.transform;
        if (inst.hidden) {
            scene.hiddenInstances.insert(id);
        }
    }
    for (std::size_t i = 0; i < model.tags().size(); ++i) {
        if (!model.tags().values()[i].visible) {
            scene.hiddenTags.insert(model.tags().keys()[i]);
        }
    }
}

void applySceneState(Model& model, const Scene& scene)
{
    if (!scene.hasState) {
        return;
    }
    for (const auto& [id, transform] : scene.transforms) {
        if (!model.instances().contains(id)) {
            continue;
        }
        Instance inst = model.instance(id);
        const bool hidden = scene.hiddenInstances.contains(id);
        if (inst.transform != transform || inst.hidden != hidden) {
            inst.transform = transform;
            inst.hidden = hidden;
            model.setInstance(id, inst);
        }
    }
    for (std::size_t i = 0; i < model.tags().size(); ++i) {
        const TagId id = model.tags().keys()[i];
        Tag tag = model.tags().values()[i];
        const bool visible = !scene.hiddenTags.contains(id);
        if (tag.visible != visible) {
            tag.visible = visible;
            model.setTag(id, tag);
        }
    }
}

void transformSceneObject(Model& model, const SceneObjectRef& object, const math::Transform& transform)
{
    if (const CameraObject* found = model.cameras().find(object.camera)) {
        CameraObject c = *found;
        // „Sus”-ul camerei (cu roll), dus de transformare; roll-ul nou e unghiul lui față de „sus”-ul fără înclinare.
        view::Camera before;
        before.setPose(view::poseLookingAt(c.position, c.target));
        before.setRoll(glm::radians(c.roll));
        const math::Vec3d up = transform.applyToVector(before.up());
        c.position = transform.applyToPoint(c.position);
        c.target = transform.applyToPoint(c.target);
        view::Camera level;
        level.setPose(view::poseLookingAt(c.position, c.target));
        const double roll = glm::degrees(std::atan2(-glm::dot(up, level.right()), glm::dot(up, level.up())));
        c.roll = std::abs(roll) < 1e-9 ? 0.0 : roll;
        model.setCamera(object.camera, c);
    } else if (const Light* light = model.lights().find(object.light)) {
        Light l = *light;
        l.position = transform.applyToPoint(l.position);
        const math::Vec3d d = transform.applyToVector(l.direction);
        if (glm::length(d) > 1e-12) {
            l.direction = glm::normalize(d);
        }
        model.setLight(object.light, l);
    }
}

void scaleSceneObject(Model& model, const SceneObjectRef& object, const math::Vec3d& factors)
{
    const double uniform = std::cbrt(std::abs(factors.x * factors.y * factors.z));
    if (const CameraObject* found = model.cameras().find(object.camera)) {
        CameraObject c = *found;
        if (c.orthographic) {
            c.orthoScale = std::max(c.orthoScale * uniform, 1.0);
        } else {
            c.displaySize = std::clamp(c.displaySize * uniform, 0.05, 50.0);
        }
        model.setCamera(object.camera, c);
    } else if (const Light* light = model.lights().find(object.light)) {
        Light l = *light;
        switch (l.kind) {
        case Light::Kind::Area:
            l.width = std::max(l.width * std::abs(factors.x), 1.0);
            l.height = std::max(l.height * std::abs(factors.y), 1.0);
            break;
        case Light::Kind::Spot:
            l.cone = std::clamp(l.cone * uniform, 1.0, 179.0);
            break;
        default:
            l.range = std::max(l.range * uniform, 1.0);
            break;
        }
        model.setLight(object.light, l);
    }
}

std::vector<math::Vec3d> sceneObjectPoints(const Model& model, const SceneObjectRef& object)
{
    if (const CameraObject* c = model.cameras().find(object.camera)) {
        return { c->position, c->target };
    }
    if (const Light* l = model.lights().find(object.light)) {
        return { l->position };
    }
    return {};
}

} // namespace aion::model
