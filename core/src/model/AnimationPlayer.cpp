#include "aion/core/model/AnimationPlayer.h"

#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/MotionPaths.h"
#include "aion/core/model/Operations.h"

#include <algorithm>

namespace aion::model {

namespace {

constexpr double kEnd = 1e-6;

const Animation* playable(const Model& base, AnimationId id)
{
    const Animation* a = base.animations().find(id);
    return a != nullptr && a->duration > 0.0 ? a : nullptr;
}

} // namespace

void AnimationPlayer::start(const Model& base, const std::vector<PlayRequest>& requests)
{
    // Comută: o singură decizie pentru toate blocurile declanșării.
    bool allOpen = true;
    bool anyToggle = false;
    for (const PlayRequest& r : requests) {
        const Animation* a = playable(base, r.animation);
        if (r.direction != PlayDirection::Toggle || a == nullptr) {
            continue;
        }
        anyToggle = true;
        const auto it = m_states.find(r.animation);
        const bool open = it != m_states.end()
            && (it->second.direction > 0 || (it->second.direction == 0 && it->second.position >= a->duration - kEnd));
        allOpen = allOpen && open;
    }
    const int toggle = anyToggle && allOpen ? -1 : 1;

    for (const PlayRequest& r : requests) {
        const Animation* a = playable(base, r.animation);
        if (a == nullptr) {
            continue;
        }
        State& s = m_states[r.animation];
        // O buclă infinită se oprește, pe loc, la următoarea declanșare a aceluiași bloc.
        if (s.direction != 0 && s.remaining < 0 && s.action == r.action) {
            s.direction = 0;
            continue;
        }
        const int direction = r.direction == PlayDirection::Forward ? 1
            : r.direction == PlayDirection::Reverse                 ? -1
                                                                    : toggle;
        if (r.repeat == 1) {
            const bool atTarget = direction > 0 ? s.position >= a->duration - kEnd : s.position <= kEnd;
            if (s.direction == 0 && atTarget) {
                continue; // deja acolo
            }
            s.remaining = 0;
        } else {
            s.position = direction > 0 ? 0.0 : a->duration;
            s.remaining = r.repeat <= 0 ? -1 : r.repeat - 1;
        }
        s.direction = direction;
        s.order = ++m_counter;
        s.action = r.action;
    }
}

bool AnimationPlayer::update(Model& runtime, const Model& base, double dt)
{
    std::vector<std::pair<std::uint64_t, AnimationId>> moved;
    for (auto& [id, s] : m_states) {
        if (s.direction == 0) {
            continue;
        }
        const Animation* a = playable(base, id);
        if (a == nullptr) {
            s.direction = 0;
            continue;
        }
        s.position += s.direction * dt;
        while (s.direction > 0 && s.position >= a->duration) {
            if (s.remaining == 0) {
                s.position = a->duration;
                s.direction = 0;
            } else {
                s.remaining = s.remaining > 0 ? s.remaining - 1 : -1;
                s.position -= a->duration;
            }
        }
        while (s.direction < 0 && s.position <= 0.0) {
            if (s.remaining == 0) {
                s.position = 0.0;
                s.direction = 0;
            } else {
                s.remaining = s.remaining > 0 ? s.remaining - 1 : -1;
                s.position += a->duration;
            }
        }
        moved.emplace_back(s.order, id);
    }
    // Ultima pornită scrie ultima.
    std::sort(moved.begin(), moved.end());
    bool wrote = false;
    for (const auto& [order, id] : moved) {
        const Animation& a = base.animations().at(id);
        const double t = m_states.at(id).position;
        for (const ObjectTrack& track : a.tracks) {
            if (!base.instances().contains(track.instance) || !runtime.instances().contains(track.instance)) {
                continue;
            }
            if (const auto transform = evaluateInstance(
                    track, t, base.instance(track.instance).transform, pivotLocal(base, track.instance))) {
                Instance inst = runtime.instance(track.instance);
                inst.transform = *transform;
                runtime.setInstance(track.instance, inst);
                wrote = true;
            }
        }
        for (const auto& [instance, transform] : pathTransformsAt(base, id, t)) {
            if (runtime.instances().contains(instance)) {
                Instance inst = runtime.instance(instance);
                inst.transform = transform;
                runtime.setInstance(instance, inst);
                wrote = true;
            }
        }
    }
    return wrote;
}

bool AnimationPlayer::active() const
{
    return std::any_of(m_states.begin(), m_states.end(), [](const auto& entry) { return entry.second.direction != 0; });
}

double AnimationPlayer::position(AnimationId animation) const
{
    const auto it = m_states.find(animation);
    return it == m_states.end() ? 0.0 : it->second.position;
}

bool AnimationPlayer::playing(AnimationId animation) const
{
    const auto it = m_states.find(animation);
    return it != m_states.end() && it->second.direction != 0;
}

std::optional<Color> AnimationPlayer::color(const Model& base, InstanceId instance) const
{
    std::optional<Color> out;
    std::uint64_t best = 0;
    for (const auto& [id, s] : m_states) {
        const Animation* a = base.animations().find(id);
        const ObjectTrack* track = a != nullptr ? findTrack(*a, instance) : nullptr;
        if (track == nullptr || s.order < best) {
            continue;
        }
        if (const auto c = evaluateColor(*track, s.position)) {
            out = c;
            best = s.order;
        }
    }
    return out;
}

std::optional<CameraKey> AnimationPlayer::camera(const Model& base) const
{
    std::optional<CameraKey> out;
    std::uint64_t best = 0;
    for (const auto& [id, s] : m_states) {
        const Animation* a = base.animations().find(id);
        if (s.direction == 0 || a == nullptr || a->camera.empty() || s.order < best) {
            continue;
        }
        out = evaluateCamera(*a, s.position);
        best = s.order;
    }
    return out;
}

void AnimationPlayer::reset()
{
    m_states.clear();
    m_counter = 0;
}

} // namespace aion::model
