#include "aion/core/model/Interaction.h"

#include "aion/core/model/Operations.h"

#include <glm/trigonometric.hpp>

namespace aion::model {

using math::Transform;
using math::Vec3d;

Transform behaviorMotion(const Model& model, InstanceId instance, double fraction)
{
    const Instance& inst = model.instance(instance);
    const Behavior& b = inst.behavior;
    const auto [lo, hi] = bounds(model, inst.definition);
    if (b.kind == Behavior::Kind::None || lo.x > hi.x) {
        return {};
    }
    if (b.kind == Behavior::Kind::Slide) {
        Vec3d direction { 0.0 };
        switch (b.anchor) {
        case Behavior::Anchor::Front: direction = { 0, -1, 0 }; break;
        case Behavior::Anchor::Back: direction = { 0, 1, 0 }; break;
        case Behavior::Anchor::Left: direction = { -1, 0, 0 }; break;
        case Behavior::Anchor::Right: direction = { 1, 0, 0 }; break;
        case Behavior::Anchor::Top: direction = { 0, 0, 1 }; break;
        case Behavior::Anchor::Bottom: direction = { 0, 0, -1 }; break;
        }
        return Transform::translation(direction * (b.amount * fraction));
    }
    // Rotate: balamaua e pe latura din față (Y minim) a cutiei; sensul pozitiv deschide spre față (−Y).
    const double angle = glm::radians(b.amount) * fraction;
    switch (b.anchor) {
    case Behavior::Anchor::Right:
        return Transform::rotation({ hi.x, lo.y, 0.0 }, { 0, 0, 1 }, angle);
    case Behavior::Anchor::Top:
        return Transform::rotation({ 0.0, lo.y, hi.z }, { 1, 0, 0 }, -angle);
    case Behavior::Anchor::Bottom:
        return Transform::rotation({ 0.0, lo.y, lo.z }, { 1, 0, 0 }, angle);
    case Behavior::Anchor::Left:
    case Behavior::Anchor::Front:
    case Behavior::Anchor::Back:
        break;
    }
    return Transform::rotation({ lo.x, lo.y, 0.0 }, { 0, 0, 1 }, -angle);
}

Transform transformAtOpenness(const Model& model, InstanceId instance, double fraction)
{
    const Instance& inst = model.instance(instance);
    const Transform closed = inst.behavior.open ? inst.transform * behaviorMotion(model, instance, 1.0).inverse()
                                                : inst.transform;
    return closed * behaviorMotion(model, instance, fraction);
}

void toggleBehavior(Model& model, InstanceId instance)
{
    Instance inst = model.instance(instance);
    if (inst.behavior.kind == Behavior::Kind::None) {
        return;
    }
    inst.transform = transformAtOpenness(model, instance, inst.behavior.open ? 0.0 : 1.0);
    inst.behavior.open = !inst.behavior.open;
    model.setInstance(instance, inst);
}

} // namespace aion::model
