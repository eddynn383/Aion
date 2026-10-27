#include "aion/core/model/Rules.h"

#include "aion/core/model/Operations.h"
#include "aion/core/view/CameraMotion.h"

#include <glm/trigonometric.hpp>

#include <algorithm>

namespace aion::model {

namespace {

// Centrul cutiei instanței, în coordonatele părintelui.
math::Vec3d centerOf(const Model& model, const Instance& inst)
{
    const auto [lo, hi] = bounds(model, inst.definition);
    const math::Vec3d c = lo.x <= hi.x ? (lo + hi) * 0.5 : math::Vec3d { 0.0 };
    return inst.transform.applyToPoint(c);
}

} // namespace

bool RuleRuntime::fire(Model& model, InstanceId self, Trigger trigger, std::string_view key)
{
    if (!model.instances().contains(self)) {
        return false;
    }
    bool any = false;
    const std::vector<Rule> rules = model.instance(self).rules;
    for (std::size_t r = 0; r < rules.size(); ++r) {
        const Rule& rule = rules[r];
        if (rule.trigger != trigger || (trigger == Trigger::Key && rule.key != key)) {
            continue;
        }
        any = true;
        Sequence s;
        s.self = self;
        for (std::size_t a = 0; a < rule.actions.size(); ++a) {
            s.queue.push_back({ rule.actions[a], "r" + std::to_string(r) + "/a" + std::to_string(a) });
        }
        if (startNext(model, s)) {
            m_running.push_back(std::move(s));
        }
    }
    return any;
}

bool RuleRuntime::startNext(Model& model, Sequence& s)
{
    while (!s.queue.empty()) {
        Queued q = std::move(s.queue.front());
        s.queue.pop_front();
        const Action& a = q.action;
        if (a.kind == ActionKind::Alternate) {
            int& count = m_alternate[{ s.self, q.path }];
            const std::vector<Action>& branch = count % 2 == 0 ? a.first : a.second;
            const std::string tag = count % 2 == 0 ? "/f" : "/s";
            ++count;
            for (std::size_t i = branch.size(); i-- > 0;) {
                s.queue.push_front({ branch[i], q.path + tag + std::to_string(i) });
            }
            continue;
        }
        const InstanceId target = a.target ? a.target : s.self;
        if (a.kind == ActionKind::PlayAnimation) {
            m_requests.push_back({ a.animation, {} });
            continue;
        }
        if (a.kind == ActionKind::GoToScene) {
            m_requests.push_back({ {}, a.scene });
            continue;
        }
        if (a.kind == ActionKind::Wait) {
            s.current = Step { a, target, {}, {}, 0.0, std::max(a.duration, 0.0), false };
            return true;
        }
        if (!model.instances().contains(target)) {
            continue;
        }
        Instance inst = model.instance(target);
        switch (a.kind) {
        case ActionKind::Show:
        case ActionKind::Hide:
        case ActionKind::ToggleVisible:
            inst.hidden = a.kind == ActionKind::Hide || (a.kind == ActionKind::ToggleVisible && !inst.hidden);
            model.setInstance(target, inst);
            continue;
        case ActionKind::SetMaterial:
            inst.material = a.material;
            model.setInstance(target, inst);
            continue;
        default:
            break;
        }
        // Mișcările: de la transformarea curentă la cea țintă, în `duration`.
        math::Transform to = inst.transform;
        const math::Vec3d center = centerOf(model, inst);
        switch (a.kind) {
        case ActionKind::MoveBy:
            to = math::Transform::translation(a.vector) * inst.transform;
            break;
        case ActionKind::MoveTo: {
            Placement p = placementOf(model, target);
            to = math::Transform::translation(a.vector - p.position) * inst.transform;
            break;
        }
        case ActionKind::RotateBy:
            to = math::Transform::rotation(center, { 0, 0, 1 }, glm::radians(a.vector.z)) * inst.transform;
            break;
        case ActionKind::ScaleTo: {
            const Placement p = placementOf(model, target);
            const double current = (std::abs(p.scale.x) + std::abs(p.scale.y) + std::abs(p.scale.z)) / 3.0;
            const double factor = current > 1e-9 ? std::max(a.vector.x, 1e-3) / current : 1.0;
            to = math::Transform::scaling(center, { factor, factor, factor }) * inst.transform;
            break;
        }
        default:
            break;
        }
        s.current = Step { a, target, inst.transform, to, 0.0, std::max(a.duration, 0.0), true };
        if (s.current->duration <= 0.0) {
            inst.transform = to;
            model.setInstance(target, inst);
            s.current.reset();
            continue;
        }
        return true;
    }
    s.current.reset();
    return false;
}

bool RuleRuntime::update(Model& model, double dt)
{
    for (Sequence& s : m_running) {
        double remaining = dt;
        while (s.current && remaining > 0.0) {
            Step& step = *s.current;
            const double take = std::min(remaining, step.duration - step.elapsed);
            step.elapsed += take;
            remaining -= take;
            if (step.animated && model.instances().contains(step.target)) {
                Instance inst = model.instance(step.target);
                const double k = step.duration > 0.0 ? step.elapsed / step.duration : 1.0;
                inst.transform = math::interpolate(step.from, step.to, view::easeInOutCubic(std::clamp(k, 0.0, 1.0)));
                model.setInstance(step.target, inst);
            }
            if (step.elapsed >= step.duration - 1e-12) {
                if (!startNext(model, s)) {
                    break;
                }
            }
        }
    }
    std::erase_if(m_running, [](const Sequence& s) { return !s.current && s.queue.empty(); });
    return !m_running.empty();
}

void RuleRuntime::reset()
{
    m_running.clear();
    m_alternate.clear();
    m_requests.clear();
}

} // namespace aion::model
