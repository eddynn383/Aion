#include "aion/core/model/AnimationEval.h"

#include "aion/core/view/CameraMotion.h"

#include <algorithm>
#include <cmath>

namespace aion::model {

namespace {

template <class Key>
double tolerance(const Animation& a)
{
    return 0.5 / std::max(a.fps, 1);
}

// Cele două chei între care cade `time` și fracțiunea (după easing). Nullopt dacă lista e goală.
template <class Key>
std::optional<std::tuple<const Key*, const Key*, double>> bracket(const std::vector<Key>& keys, double time)
{
    if (keys.empty()) {
        return std::nullopt;
    }
    if (time <= keys.front().time) {
        return std::tuple { &keys.front(), &keys.front(), 0.0 };
    }
    if (time >= keys.back().time) {
        return std::tuple { &keys.back(), &keys.back(), 0.0 };
    }
    const auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](double t, const Key& k) { return t < k.time; });
    const Key& b = *next;
    const Key& a = *(next - 1);
    const double span = b.time - a.time;
    const double k = span > 0.0 ? (time - a.time) / span : 1.0;
    return std::tuple { &a, &b, ease(a.easing, k) };
}

template <class Key>
void upsert(std::vector<Key>& keys, Key key, double tol)
{
    for (Key& k : keys) {
        if (std::abs(k.time - key.time) < tol) {
            key.easing = k.easing;
            k = key;
            return;
        }
    }
    keys.insert(std::upper_bound(keys.begin(), keys.end(), key.time, [](double t, const Key& k) { return t < k.time; }),
        key);
}

ObjectTrack& trackFor(Animation& animation, InstanceId instance)
{
    for (ObjectTrack& t : animation.tracks) {
        if (t.instance == instance) {
            return t;
        }
    }
    animation.tracks.push_back({ instance, {}, {} });
    return animation.tracks.back();
}

template <class Key>
bool takeKey(std::vector<Key>& keys, double time, double tol, Key* out)
{
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        if (std::abs(it->time - time) < tol) {
            if (out != nullptr) {
                *out = *it;
            }
            keys.erase(it);
            return true;
        }
    }
    return false;
}

void dropEmptyTracks(Animation& animation)
{
    std::erase_if(animation.tracks, [](const ObjectTrack& t) { return t.transform.empty() && t.color.empty(); });
}

} // namespace

double ease(Easing easing, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    switch (easing) {
    case Easing::Linear:
        return t;
    case Easing::EaseIn:
        return t * t * t;
    case Easing::EaseOut:
        return 1.0 - std::pow(1.0 - t, 3.0);
    case Easing::Step:
        return t < 1.0 ? 0.0 : 1.0;
    case Easing::EaseInOut:
        break;
    }
    return view::easeInOutCubic(t);
}

const ObjectTrack* findTrack(const Animation& animation, InstanceId instance)
{
    for (const ObjectTrack& t : animation.tracks) {
        if (t.instance == instance) {
            return &t;
        }
    }
    return nullptr;
}

std::optional<math::Transform> evaluateTransform(const ObjectTrack& track, double time)
{
    const auto b = bracket(track.transform, time);
    if (!b) {
        return std::nullopt;
    }
    const auto [a, c, k] = *b;
    return a == c ? a->value : math::interpolate(a->value, c->value, k);
}

std::optional<Color> evaluateColor(const ObjectTrack& track, double time)
{
    const auto b = bracket(track.color, time);
    if (!b) {
        return std::nullopt;
    }
    const auto [a, c, k] = *b;
    const auto mix = [&](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::lround(x + (y - x) * k));
    };
    return Color { mix(a->value.r, c->value.r), mix(a->value.g, c->value.g), mix(a->value.b, c->value.b),
        mix(a->value.a, c->value.a) };
}

std::optional<CameraKey> evaluateCamera(const Animation& animation, double time)
{
    const auto b = bracket(animation.camera, time);
    if (!b) {
        return std::nullopt;
    }
    const auto [a, c, k] = *b;
    if (a == c) {
        return *a;
    }
    const view::CameraPose pose = view::interpolatePose({ a->target, a->yaw, a->pitch, a->distance },
        { c->target, c->yaw, c->pitch, c->distance }, k);
    return CameraKey { time, pose.target, pose.yaw, pose.pitch, pose.distance, a->easing };
}

void setTransformKey(Animation& animation, InstanceId instance, double time, const math::Transform& value)
{
    upsert(trackFor(animation, instance).transform, TransformKey { time, value, Easing::EaseInOut },
        tolerance<TransformKey>(animation));
    animation.duration = std::max(animation.duration, time);
}

void setColorKey(Animation& animation, InstanceId instance, double time, const Color& value)
{
    upsert(trackFor(animation, instance).color, ColorKey { time, value, Easing::EaseInOut },
        tolerance<ColorKey>(animation));
    animation.duration = std::max(animation.duration, time);
}

void setCameraKey(Animation& animation, CameraKey key)
{
    animation.duration = std::max(animation.duration, key.time);
    upsert(animation.camera, key, tolerance<CameraKey>(animation));
}

void removeKey(Animation& animation, KeyKind kind, InstanceId instance, double time)
{
    const double tol = tolerance<TransformKey>(animation);
    if (kind == KeyKind::Camera) {
        takeKey<CameraKey>(animation.camera, time, tol, nullptr);
        return;
    }
    for (ObjectTrack& t : animation.tracks) {
        if (t.instance != instance) {
            continue;
        }
        if (kind == KeyKind::Transform) {
            takeKey<TransformKey>(t.transform, time, tol, nullptr);
        } else {
            takeKey<ColorKey>(t.color, time, tol, nullptr);
        }
    }
    dropEmptyTracks(animation);
}

void moveKey(Animation& animation, KeyKind kind, InstanceId instance, double time, double newTime)
{
    newTime = std::clamp(newTime, 0.0, animation.duration);
    const double tol = tolerance<TransformKey>(animation);
    if (kind == KeyKind::Camera) {
        CameraKey key;
        if (takeKey(animation.camera, time, tol, &key)) {
            key.time = newTime;
            upsert(animation.camera, key, tol);
        }
        return;
    }
    for (ObjectTrack& t : animation.tracks) {
        if (t.instance != instance) {
            continue;
        }
        if (kind == KeyKind::Transform) {
            TransformKey key;
            if (takeKey(t.transform, time, tol, &key)) {
                key.time = newTime;
                upsert(t.transform, key, tol);
            }
        } else {
            ColorKey key;
            if (takeKey(t.color, time, tol, &key)) {
                key.time = newTime;
                upsert(t.color, key, tol);
            }
        }
    }
}

} // namespace aion::model
