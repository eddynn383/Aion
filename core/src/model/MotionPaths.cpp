#include "aion/core/model/MotionPaths.h"

#include "aion/core/geom/Bezier.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Operations.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace aion::model {

namespace {

using math::Vec3d;

// Cadrul căii în lume la o distanță: punctul, tangenta (în planul căii), stânga și normala.
struct Frame {
    Vec3d point { 0.0 };
    Vec3d t { 1.0, 0.0, 0.0 };
    Vec3d l { 0.0, 1.0, 0.0 };
    Vec3d n { 0.0, 0.0, 1.0 };
};

Frame frameAt(const MotionPath& path, const geom::ArcLength& arc, double distance)
{
    // La un colț al curbei, obiectul se întoarce treptat (5% din lungime, între 0,2 și 1,5 m).
    const geom::PathFrame f = arc.at(distance, std::clamp(arc.length() * 0.05, 200.0, 1500.0));
    Frame out;
    out.n = path.transform.applyToVector(path.normal);
    out.n = glm::length(out.n) > tol::kLength ? glm::normalize(out.n) : Vec3d { 0, 0, 1 };
    out.point = path.transform.applyToPoint(f.point);
    Vec3d t = path.transform.applyToVector(f.tangent);
    t -= out.n * glm::dot(t, out.n);
    if (glm::length(t) <= 1e-12) {
        t = std::abs(out.n.z) < 0.9 ? glm::cross(Vec3d { 0, 0, 1 }, out.n) : glm::cross(out.n, Vec3d { 0, 1, 0 });
    }
    out.t = glm::normalize(t);
    out.l = glm::cross(out.n, out.t);
    return out;
}

double travelled(const MotionPath& path, const geom::ArcLength& arc, double time)
{
    const double u = path.duration > 1e-9 ? std::clamp((time - path.start) / path.duration, 0.0, 1.0)
                                          : (time >= path.start ? 1.0 : 0.0);
    return ease(path.easing, u) * arc.length();
}

// Centrul feței de jos a cutiei obiectului, în coordonatele lui.
Vec3d baseLocal(const Model& model, InstanceId id)
{
    const auto [lo, hi] = bounds(model, model.instance(id).definition);
    return lo.x <= hi.x ? Vec3d { (lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, lo.z } : Vec3d { 0.0 };
}

glm::dmat3 rotationZ(double degrees)
{
    const double a = glm::radians(degrees);
    const double c = std::cos(a);
    const double s = std::sin(a);
    return { c, s, 0.0, -s, c, 0.0, 0.0, 0.0, 1.0 };
}

Vec3d frontLocal(int front)
{
    return rotationZ(90.0 * front) * Vec3d { 1.0, 0.0, 0.0 };
}

// Lungimea obiectului pe direcția feței (cu scara).
double lengthAlongFront(const Model& model, const PathAnchor& a)
{
    if (!model.instances().contains(a.instance)) {
        return 0.0;
    }
    const auto [lo, hi] = bounds(model, model.instance(a.instance).definition);
    if (lo.x > hi.x) {
        return 0.0;
    }
    const Vec3d scale = decompose(a.before).scale;
    return a.front % 2 == 0 ? (hi.x - lo.x) * std::abs(scale.x) : (hi.y - lo.y) * std::abs(scale.y);
}

Vec3d scaleOf(const PathAnchor& a)
{
    return decompose(a.before).scale;
}

// Poza obiectului pe cale la `time`, doar din decalajele ancorei (fără mutările din Modelare); tabela de lungime și
// baza obiectului vin calculate (se refolosesc pentru start și pentru timp, și pentru toate ancorele unei căi).
math::Transform poseOn(const MotionPath& path, const geom::ArcLength& arc, const PathAnchor& anchor, double time,
    const Vec3d& scale, const Vec3d& base)
{
    const Frame f = frameAt(path, arc, travelled(path, arc, time) + anchor.along);
    // Orientarea vine numai din tangentă (decalajul lateral nu o schimbă, chiar pe curbe strânse).
    const glm::dmat3 basis(f.t, f.l, f.n);
    const glm::dmat3 rotation = basis * rotationZ(anchor.yaw) * rotationZ(-90.0 * anchor.front);
    const Vec3d at = f.point + f.l * anchor.lateral + f.n * anchor.up;
    math::Mat4d m(1.0);
    const glm::dmat3 linear = rotation * glm::dmat3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z);
    for (int c = 0; c < 3; ++c) {
        m[c] = glm::dvec4(linear[c], 0.0);
    }
    m[3] = glm::dvec4(at - linear * base, 1.0);
    return math::Transform(m);
}

Vec3d baseOf(const Model& model, const PathAnchor& anchor)
{
    return model.instances().contains(anchor.instance) ? baseLocal(model, anchor.instance) : Vec3d { 0.0 };
}

math::Transform pathPose(const Model& model, const MotionPath& path, const PathAnchor& anchor, double time, const Vec3d& scale)
{
    return poseOn(path, geom::ArcLength(path.curve), anchor, time, scale, baseOf(model, anchor));
}

// Poza la `time` plus mutările din Modelare (transformarea de bază față de poza de start a căii).
math::Transform evaluateAnchor(const Model& model, const MotionPath& path, const geom::ArcLength& arc, const PathAnchor& anchor,
    double time, const Vec3d& scale)
{
    const Vec3d base = baseOf(model, anchor);
    math::Transform relative;
    if (model.instances().contains(anchor.instance)) {
        relative = poseOn(path, arc, anchor, path.start, scaleOf(anchor), base).inverse() * model.instance(anchor.instance).transform;
    }
    return poseOn(path, arc, anchor, time, scale, base) * relative;
}

// Scara obiectului la `time`: din pista lui, dacă o animă, altfel cea de dinainte de ancorare.
Vec3d animatedScale(const Model& model, AnimationId animation, const PathAnchor& anchor, double time)
{
    const Animation* anim = model.animations().find(animation);
    if (const ObjectTrack* track = anim != nullptr ? findTrack(*anim, anchor.instance) : nullptr) {
        const bool scaled = std::any_of(track->channels.begin(), track->channels.end(), [](const Channel& c) {
            return c.property == Property::ScaleX || c.property == Property::ScaleY || c.property == Property::ScaleZ;
        });
        if (scaled) {
            const math::Vec3d pivot = pivotLocal(model, anchor.instance);
            return evaluateValues(*track, time, transformValues(model.instance(anchor.instance).transform, pivot)).scale;
        }
    }
    return scaleOf(anchor);
}

// Ce s-a schimbat în Modelare (Move, Rotate, Scale, Inspector) pe obiectul ancorat: transformarea lui de bază față de
// poza de start a căii. Se păstrează la fiecare moment al parcurgerii.
math::Transform relativeOf(const Model& model, const MotionPath& path, const PathAnchor& a)
{
    if (!model.instances().contains(a.instance)) {
        return {};
    }
    return pathPose(model, path, a, path.start, scaleOf(a)).inverse() * model.instance(a.instance).transform;
}

void place(Model& model, const MotionPath& path, const PathAnchor& a, const math::Transform& relative)
{
    if (!model.instances().contains(a.instance)) {
        return;
    }
    Instance inst = model.instance(a.instance);
    inst.transform = pathPose(model, path, a, path.start, scaleOf(a)) * relative;
    model.setInstance(a.instance, inst);
}

PathAnchor* findAnchor(MotionPath& path, InstanceId instance)
{
    const auto it = std::find_if(path.anchors.begin(), path.anchors.end(), [&](const PathAnchor& a) { return a.instance == instance; });
    return it == path.anchors.end() ? nullptr : &*it;
}

} // namespace

math::Transform anchorTransform(const Model& model, const MotionPath& path, const PathAnchor& anchor, double time,
    const math::Vec3d& scale)
{
    return evaluateAnchor(model, path, geom::ArcLength(path.curve), anchor, time, scale);
}

std::optional<math::Transform> pathTransformAt(const Model& model, AnimationId animation, InstanceId instance, double time)
{
    const std::optional<MotionPathId> id = pathOf(model, animation, instance);
    if (!id || !model.instances().contains(instance)) {
        return std::nullopt;
    }
    const MotionPath& path = model.motionPaths().at(*id);
    const auto a = std::find_if(path.anchors.begin(), path.anchors.end(), [&](const PathAnchor& x) { return x.instance == instance; });
    return evaluateAnchor(model, path, geom::ArcLength(path.curve), *a, time, animatedScale(model, animation, *a, time));
}

std::map<InstanceId, math::Transform> pathTransformsAt(const Model& model, AnimationId animation, double time)
{
    std::map<InstanceId, math::Transform> out;
    for (const MotionPath& path : model.motionPaths().values()) {
        if (path.animation != animation || path.anchors.empty()) {
            continue;
        }
        const geom::ArcLength arc(path.curve);
        for (const PathAnchor& a : path.anchors) {
            if (model.instances().contains(a.instance) && model.instance(a.instance).parent == model.root()) {
                out[a.instance] = evaluateAnchor(model, path, arc, a, time, animatedScale(model, animation, a, time));
            }
        }
    }
    return out;
}

void applyPaths(Model& posed, const Model& source, AnimationId animation, double time)
{
    for (const auto& [id, transform] : pathTransformsAt(source, animation, time)) {
        if (posed.instances().contains(id)) {
            Instance inst = posed.instance(id);
            inst.transform = transform;
            posed.setInstance(id, inst);
        }
    }
}

bool attachToPath(Model& model, MotionPathId id, InstanceId instance)
{
    if (!model.motionPaths().contains(id) || !model.instances().contains(instance)
        || model.instance(instance).parent != model.root()) {
        return false;
    }
    MotionPath path = model.motionPaths().at(id);
    if (pathOf(model, path.animation, instance)) {
        return false;
    }
    PathAnchor a;
    a.instance = instance;
    a.before = model.instance(instance).transform;
    // Fața: axa orizontală a obiectului cea mai apropiată de direcția de start (cât mai puțină rotire).
    const geom::ArcLength arc(path.curve);
    const Frame start = frameAt(path, arc, 0.0);
    const glm::dmat3 r0 = decompose(a.before).rotation;
    double best = -2.0;
    for (int k = 0; k < 4; ++k) {
        const double d = glm::dot(glm::normalize(r0 * frontLocal(k)), start.t);
        if (d > best + 1e-9) {
            best = d;
            a.front = k;
        }
    }
    // În coloană, în urma ultimului ancorat care încă există, ca să nu se suprapună.
    const auto live = std::find_if(path.anchors.rbegin(), path.anchors.rend(),
        [&](const PathAnchor& x) { return model.instances().contains(x.instance); });
    if (live != path.anchors.rend()) {
        const PathAnchor& last = *live;
        const double lenLast = lengthAlongFront(model, last);
        const double lenNew = lengthAlongFront(model, a);
        a.along = last.along - (lenLast * 0.5 + lenNew * 0.5 + std::max(200.0, 0.2 * std::max(lenLast, lenNew)));
    }
    path.anchors.push_back(a);
    model.setMotionPath(id, path);
    place(model, path, a, {});
    return true;
}

void detachFromPath(Model& model, MotionPathId id, InstanceId instance)
{
    if (!model.motionPaths().contains(id)) {
        return;
    }
    MotionPath path = model.motionPaths().at(id);
    PathAnchor* a = findAnchor(path, instance);
    if (a == nullptr) {
        return;
    }
    if (model.instances().contains(instance)) {
        Instance inst = model.instance(instance);
        inst.transform = a->before;
        model.setInstance(instance, inst);
    }
    path.anchors.erase(path.anchors.begin() + (a - path.anchors.data()));
    model.setMotionPath(id, path);
}

void removePath(Model& model, MotionPathId id)
{
    if (!model.motionPaths().contains(id)) {
        return;
    }
    for (const PathAnchor& a : model.motionPaths().at(id).anchors) {
        if (model.instances().contains(a.instance)) {
            Instance inst = model.instance(a.instance);
            inst.transform = a.before;
            model.setInstance(a.instance, inst);
        }
    }
    model.removeMotionPath(id);
}

void removeAnimationWithPaths(Model& model, AnimationId animation)
{
    std::vector<MotionPathId> paths;
    for (std::size_t i = 0; i < model.motionPaths().size(); ++i) {
        if (model.motionPaths().values()[i].animation == animation) {
            paths.push_back(model.motionPaths().keys()[i]);
        }
    }
    for (MotionPathId p : paths) {
        removePath(model, p);
    }
    if (model.animations().contains(animation)) {
        model.removeAnimation(animation);
    }
}

void updatePath(Model& model, MotionPathId id, MotionPath path)
{
    if (!model.motionPaths().contains(id)) {
        return;
    }
    // Fiecare obiect își păstrează mutările din Modelare față de calea veche.
    const MotionPath old = model.motionPaths().at(id);
    std::vector<math::Transform> relative;
    for (const PathAnchor& a : path.anchors) {
        const auto it = std::find_if(old.anchors.begin(), old.anchors.end(), [&](const PathAnchor& x) { return x.instance == a.instance; });
        relative.push_back(it != old.anchors.end() ? relativeOf(model, old, *it) : math::Transform {});
    }
    model.setMotionPath(id, path);
    for (std::size_t i = 0; i < path.anchors.size(); ++i) {
        place(model, path, path.anchors[i], relative[i]);
    }
}

void absorbAnchorEdit(Model& model, MotionPathId id, InstanceId instance, const math::Transform& edited, double time)
{
    if (!model.motionPaths().contains(id) || !model.instances().contains(instance)) {
        return;
    }
    MotionPath path = model.motionPaths().at(id);
    PathAnchor* a = findAnchor(path, instance);
    if (a == nullptr) {
        return;
    }
    const math::Transform relative = relativeOf(model, path, *a);
    const math::Transform expected = anchorTransform(model, path, *a, time, scaleOf(*a));
    const geom::ArcLength arc(path.curve);
    const Frame f = frameAt(path, arc, travelled(path, arc, time) + a->along);
    const Vec3d base = baseLocal(model, instance);
    const Vec3d delta = edited.applyToPoint(base) - expected.applyToPoint(base);
    // Rotirea în jurul normalei: unghiul dintre fața așteptată și cea editată, în planul căii.
    const auto flat = [&](Vec3d v) { return v - f.n * glm::dot(v, f.n); };
    const Vec3d fx = flat(expected.applyToVector(frontLocal(a->front)));
    const Vec3d fe = flat(edited.applyToVector(frontLocal(a->front)));
    if (glm::length(fx) > tol::kLength && glm::length(fe) > tol::kLength) {
        a->yaw += glm::degrees(std::atan2(glm::dot(glm::cross(fx, fe), f.n), glm::dot(fx, fe)));
    }
    a->along += glm::dot(delta, f.t);
    a->lateral += glm::dot(delta, f.l);
    a->up += glm::dot(delta, f.n);
    model.setMotionPath(id, path);
    place(model, path, *a, relative);
}

void rotateAnchorFront(Model& model, MotionPathId id, InstanceId instance)
{
    if (!model.motionPaths().contains(id)) {
        return;
    }
    MotionPath path = model.motionPaths().at(id);
    PathAnchor* a = findAnchor(path, instance);
    if (a == nullptr) {
        return;
    }
    const math::Transform relative = relativeOf(model, path, *a);
    a->front = (a->front + 1) % 4;
    model.setMotionPath(id, path);
    place(model, path, *a, relative);
}

std::optional<MotionPathId> pathOf(const Model& model, AnimationId animation, InstanceId instance)
{
    // Un obiect mutat într-un grup nu mai e pe cale (calea e în coordonatele modelului); scos din grup, revine pe ea.
    if (!model.instances().contains(instance) || model.instance(instance).parent != model.root()) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < model.motionPaths().size(); ++i) {
        const MotionPath& p = model.motionPaths().values()[i];
        if (p.animation == animation
            && std::any_of(p.anchors.begin(), p.anchors.end(), [&](const PathAnchor& a) { return a.instance == instance; })) {
            return model.motionPaths().keys()[i];
        }
    }
    return std::nullopt;
}

} // namespace aion::model
