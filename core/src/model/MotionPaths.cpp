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
    const geom::PathFrame f = arc.at(distance);
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

void placeAtStart(Model& model, const MotionPath& path, const PathAnchor& a)
{
    if (!model.instances().contains(a.instance)) {
        return;
    }
    Instance inst = model.instance(a.instance);
    inst.transform = anchorTransform(model, path, a, path.start, scaleOf(a));
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
    const geom::ArcLength arc(path.curve);
    const Frame f = frameAt(path, arc, travelled(path, arc, time) + anchor.along);
    // Orientarea vine numai din tangentă (decalajul lateral nu o schimbă, chiar pe curbe strânse).
    const glm::dmat3 basis(f.t, f.l, f.n);
    const glm::dmat3 rotation = basis * rotationZ(anchor.yaw) * rotationZ(-90.0 * anchor.front);
    const Vec3d base = model.instances().contains(anchor.instance) ? baseLocal(model, anchor.instance) : Vec3d { 0.0 };
    const Vec3d at = f.point + f.l * anchor.lateral + f.n * anchor.up;
    math::Mat4d m(1.0);
    const glm::dmat3 linear = rotation * glm::dmat3(scale.x, 0, 0, 0, scale.y, 0, 0, 0, scale.z);
    for (int c = 0; c < 3; ++c) {
        m[c] = glm::dvec4(linear[c], 0.0);
    }
    m[3] = glm::dvec4(at - linear * base, 1.0);
    return math::Transform(m);
}

void applyPaths(Model& posed, const Model& source, AnimationId animation, double time)
{
    const Animation* anim = source.animations().find(animation);
    for (const MotionPath& path : source.motionPaths().values()) {
        if (path.animation != animation) {
            continue;
        }
        for (const PathAnchor& a : path.anchors) {
            if (!posed.instances().contains(a.instance) || !source.instances().contains(a.instance)) {
                continue;
            }
            Vec3d scale = scaleOf(a);
            if (const ObjectTrack* track = anim != nullptr ? findTrack(*anim, a.instance) : nullptr) {
                const bool scaled = std::any_of(track->channels.begin(), track->channels.end(), [](const Channel& c) {
                    return c.property == Property::ScaleX || c.property == Property::ScaleY || c.property == Property::ScaleZ;
                });
                if (scaled) {
                    const math::Vec3d pivot = pivotLocal(source, a.instance);
                    scale = evaluateValues(*track, time, transformValues(source.instance(a.instance).transform, pivot)).scale;
                }
            }
            Instance inst = posed.instance(a.instance);
            inst.transform = anchorTransform(source, path, a, time, scale);
            posed.setInstance(a.instance, inst);
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
    // În coloană, în urma ultimului ancorat, ca să nu se suprapună.
    if (!path.anchors.empty()) {
        const PathAnchor& last = path.anchors.back();
        const double lenLast = lengthAlongFront(model, last);
        const double lenNew = lengthAlongFront(model, a);
        a.along = last.along - (lenLast * 0.5 + lenNew * 0.5 + std::max(200.0, 0.2 * std::max(lenLast, lenNew)));
    }
    path.anchors.push_back(a);
    model.setMotionPath(id, path);
    placeAtStart(model, path, a);
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
    model.setMotionPath(id, path);
    for (const PathAnchor& a : path.anchors) {
        placeAtStart(model, path, a);
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
    placeAtStart(model, path, *a);
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
    a->front = (a->front + 1) % 4;
    model.setMotionPath(id, path);
    placeAtStart(model, path, *a);
}

std::optional<MotionPathId> pathOf(const Model& model, AnimationId animation, InstanceId instance)
{
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
