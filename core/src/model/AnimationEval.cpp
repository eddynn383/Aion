#include "aion/core/model/AnimationEval.h"

#include "aion/core/model/Operations.h"
#include "aion/core/view/CameraMotion.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace aion::model {

namespace {

// Două chei la mai puțin de jumătate de cadru sunt „aceeași”.
double tolerance(const Animation& a)
{
    return 0.5 / std::max(a.fps, 1);
}

// Cât de mult trebuie să se schimbe o valoare ca auto-key-ul să scrie o cheie.
double valueTolerance(Property p)
{
    return static_cast<int>(p) >= static_cast<int>(Property::ScaleX) ? 1e-9 : 1e-6;
}

// Cele două chei între care cade `time` și fracțiunea (după easing). Nullopt dacă lista e goală.
template <class K>
std::optional<std::tuple<const K*, const K*, double>> bracket(const std::vector<K>& keys, double time)
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
    const auto next = std::upper_bound(keys.begin(), keys.end(), time, [](double t, const K& k) { return t < k.time; });
    const K& b = *next;
    const K& a = *(next - 1);
    const double span = b.time - a.time;
    const double k = span > 0.0 ? (time - a.time) / span : 1.0;
    return std::tuple { &a, &b, ease(a.easing, k) };
}

// Pune cheia în listă, în ordinea timpului; una la mai puțin de `tol` e înlocuită (și își păstrează easing-ul, dacă
// `keepEasing`).
template <class K>
void upsert(std::vector<K>& keys, K key, double tol, bool keepEasing)
{
    for (K& k : keys) {
        if (std::abs(k.time - key.time) < tol) {
            if (keepEasing) {
                key.easing = k.easing;
            }
            k = key;
            return;
        }
    }
    keys.insert(std::upper_bound(keys.begin(), keys.end(), key.time, [](double t, const K& k) { return t < k.time; }),
        key);
}

template <class K>
K* keyAt(std::vector<K>& keys, double time, double tol)
{
    for (K& k : keys) {
        if (std::abs(k.time - time) < tol) {
            return &k;
        }
    }
    return nullptr;
}

ObjectTrack& trackFor(Animation& animation, InstanceId instance)
{
    for (ObjectTrack& t : animation.tracks) {
        if (t.instance == instance) {
            return t;
        }
    }
    animation.tracks.push_back({ instance, {} });
    return animation.tracks.back();
}

ObjectTrack* findTrack(Animation& animation, InstanceId instance)
{
    for (ObjectTrack& t : animation.tracks) {
        if (t.instance == instance) {
            return &t;
        }
    }
    return nullptr;
}

Channel& channelFor(ObjectTrack& track, Property property)
{
    const auto it = std::lower_bound(track.channels.begin(), track.channels.end(), property,
        [](const Channel& c, Property p) { return static_cast<int>(c.property) < static_cast<int>(p); });
    if (it != track.channels.end() && it->property == property) {
        return *it;
    }
    return *track.channels.insert(it, Channel { property, {} });
}

Channel* findChannel(ObjectTrack& track, Property property)
{
    for (Channel& c : track.channels) {
        if (c.property == property) {
            return &c;
        }
    }
    return nullptr;
}

void dropEmpty(Animation& animation)
{
    for (ObjectTrack& t : animation.tracks) {
        std::erase_if(t.channels, [](const Channel& c) { return c.keys.empty(); });
    }
    std::erase_if(animation.tracks, [](const ObjectTrack& t) { return t.channels.empty(); });
}

// Coordonata unei curbe Bézier cubice 1D cu capetele 0 și 1 și punctele de control a, b, la parametrul s.
double bezier(double a, double b, double s)
{
    const double u = 1.0 - s;
    return 3.0 * u * u * s * a + 3.0 * u * s * s * b + s * s * s;
}

double bezierSlope(double a, double b, double s)
{
    const double u = 1.0 - s;
    return 3.0 * u * u * a + 6.0 * u * s * (b - a) + 3.0 * s * s * (1.0 - b);
}

// Unghiurile Euler cele mai apropiate de `reference`: cele două soluții ale aceleiași rotații ((x, y, z) și
// (x + 180, 180 − y, z + 180)), fiecare despachetată față de referință.
math::Vec3d nearestEuler(const math::Vec3d& euler, const math::Vec3d& reference)
{
    const auto unwrap = [&](math::Vec3d e) {
        for (int i = 0; i < 3; ++i) {
            e[i] = unwrapDegrees(e[i], reference[i]);
        }
        return e;
    };
    const math::Vec3d a = unwrap(euler);
    const math::Vec3d b = unwrap({ euler.x + 180.0, 180.0 - euler.y, euler.z + 180.0 });
    return glm::length(b - reference) + 1e-9 < glm::length(a - reference) ? b : a;
}

} // namespace

// --- Easing ---

double ease(const Easing& easing, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    if (easing.step) {
        return t < 1.0 ? 0.0 : 1.0;
    }
    // Timpul (x) trebuie să crească: mânerele stau în [0, 1] pe x.
    const double x1 = std::clamp(easing.x1, 0.0, 1.0);
    const double x2 = std::clamp(easing.x2, 0.0, 1.0);
    if (x1 == easing.y1 && x2 == easing.y2) {
        return t; // o dreaptă
    }
    // x(s) = t: Newton, apoi bisecție dacă nu converge.
    double s = t;
    for (int i = 0; i < 8; ++i) {
        const double error = bezier(x1, x2, s) - t;
        if (std::abs(error) < 1e-9) {
            return bezier(easing.y1, easing.y2, s);
        }
        const double slope = bezierSlope(x1, x2, s);
        if (std::abs(slope) < 1e-9) {
            break;
        }
        s -= error / slope;
        if (s < 0.0 || s > 1.0) {
            break;
        }
    }
    double lo = 0.0;
    double hi = 1.0;
    s = t;
    for (int i = 0; i < 64; ++i) {
        const double x = bezier(x1, x2, s);
        if (std::abs(x - t) < 1e-10) {
            break;
        }
        (x < t ? lo : hi) = s;
        s = (lo + hi) * 0.5;
    }
    return bezier(easing.y1, easing.y2, s);
}

const std::vector<Easing>& easingPresets()
{
    static const std::vector<Easing> presets {
        { "linear", 0.0, 0.0, 1.0, 1.0, false },
        { "ease", 0.25, 0.1, 0.25, 1.0, false },
        { "easeIn", 0.42, 0.0, 1.0, 1.0, false },
        { "easeOut", 0.0, 0.0, 0.58, 1.0, false },
        { "easeInOut", 0.42, 0.0, 0.58, 1.0, false },
        { "easeInBack", 0.36, 0.0, 0.66, -0.56, false },
        { "easeOutBack", 0.34, 1.56, 0.64, 1.0, false },
        { "step", 0.0, 0.0, 1.0, 1.0, true },
    };
    return presets;
}

std::optional<Easing> easingPreset(std::string_view name)
{
    for (const Easing& e : easingPresets()) {
        if (e.name == name) {
            return e;
        }
    }
    return std::nullopt;
}

// --- Proprietățile ---

namespace {

constexpr std::array<std::string_view, kPropertyCount> kPropertyNames { "position.x", "position.y", "position.z",
    "rotation.x", "rotation.y", "rotation.z", "scale.x", "scale.y", "scale.z", "color" };

} // namespace

std::string_view propertyName(Property property)
{
    return kPropertyNames[static_cast<std::size_t>(property)];
}

std::optional<Property> propertyFromName(std::string_view name)
{
    for (std::size_t i = 0; i < kPropertyNames.size(); ++i) {
        if (kPropertyNames[i] == name) {
            return static_cast<Property>(i);
        }
    }
    return std::nullopt;
}

double TransformValues::get(Property p) const
{
    const int i = static_cast<int>(p);
    return i < 3 ? position[i] : i < 6 ? rotation[i - 3] : i < 9 ? scale[i - 6] : 0.0;
}

void TransformValues::set(Property p, double value)
{
    const int i = static_cast<int>(p);
    if (i < 3) {
        position[i] = value;
    } else if (i < 6) {
        rotation[i - 3] = value;
    } else if (i < 9) {
        scale[i - 6] = value;
    }
}

TransformValues transformValues(const math::Transform& transform, const math::Vec3d& pivot)
{
    const Decomposed d = decompose(transform);
    return { transform.applyToPoint(pivot), eulerDegrees(d.rotation), d.scale };
}

math::Transform composeTransform(const TransformValues& values, const math::Vec3d& pivot)
{
    glm::dmat3 rs = rotationFromEuler(values.rotation);
    for (int i = 0; i < 3; ++i) {
        const double s = values.scale[i];
        rs[i] *= std::abs(s) < 1e-9 ? std::copysign(1e-9, s) : s; // o scară nulă ar turti obiectul ireversibil
    }
    glm::dmat4 m(rs);
    m[3] = glm::dvec4(values.position - rs * pivot, 1.0);
    return math::Transform(m);
}

double unwrapDegrees(double degrees, double reference)
{
    return degrees + 360.0 * std::round((reference - degrees) / 360.0);
}

// --- Evaluarea ---

const ObjectTrack* findTrack(const Animation& animation, InstanceId instance)
{
    for (const ObjectTrack& t : animation.tracks) {
        if (t.instance == instance) {
            return &t;
        }
    }
    return nullptr;
}

const Channel* findChannel(const ObjectTrack& track, Property property)
{
    for (const Channel& c : track.channels) {
        if (c.property == property) {
            return &c;
        }
    }
    return nullptr;
}

std::optional<double> evaluateChannel(const Channel& channel, double time)
{
    const auto b = bracket(channel.keys, time);
    if (!b) {
        return std::nullopt;
    }
    const auto [a, c, k] = *b;
    return a == c ? a->value : a->value + (c->value - a->value) * k;
}

std::optional<Color> evaluateColor(const ObjectTrack& track, double time)
{
    const Channel* channel = findChannel(track, Property::Color);
    const auto b = channel != nullptr ? bracket(channel->keys, time) : std::nullopt;
    if (!b) {
        return std::nullopt;
    }
    const auto [a, c, k] = *b;
    const auto mix = [&](std::uint8_t x, std::uint8_t y) {
        return static_cast<std::uint8_t>(std::clamp(std::lround(x + (y - x) * k), 0L, 255L));
    };
    return Color { mix(a->color.r, c->color.r), mix(a->color.g, c->color.g), mix(a->color.b, c->color.b),
        mix(a->color.a, c->color.a) };
}

TransformValues evaluateValues(const ObjectTrack& track, double time, const TransformValues& base)
{
    TransformValues v = base;
    for (const Channel& c : track.channels) {
        if (isTransform(c.property)) {
            if (const auto value = evaluateChannel(c, time)) {
                v.set(c.property, *value);
            }
        }
    }
    return v;
}

std::optional<math::Transform> evaluateInstance(
    const ObjectTrack& track, double time, const math::Transform& base, const math::Vec3d& pivot)
{
    if (std::none_of(track.channels.begin(), track.channels.end(), [](const Channel& c) { return isTransform(c.property); })) {
        return std::nullopt;
    }
    return composeTransform(evaluateValues(track, time, transformValues(base, pivot)), pivot);
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

// --- Editarea ---

void setChannelKey(Animation& animation, InstanceId instance, Property property, double time, double value,
    const std::optional<Easing>& easing)
{
    Key key;
    key.time = time;
    key.value = value;
    key.easing = easing.value_or(animation.defaultEasing);
    upsert(channelFor(trackFor(animation, instance), property).keys, key, tolerance(animation), !easing.has_value());
    animation.duration = std::max(animation.duration, time);
}

void setColorKey(Animation& animation, InstanceId instance, double time, const Color& value)
{
    Key key;
    key.time = time;
    key.color = value;
    key.easing = animation.defaultEasing;
    upsert(channelFor(trackFor(animation, instance), Property::Color).keys, key, tolerance(animation), true);
    animation.duration = std::max(animation.duration, time);
}

void setCameraKey(Animation& animation, CameraKey key)
{
    animation.duration = std::max(animation.duration, key.time);
    upsert(animation.camera, key, tolerance(animation), true);
}

std::vector<Property> keyTransform(Animation& animation, InstanceId instance, double time, const TransformValues& before,
    const TransformValues& after, bool onlyChanged)
{
    TransformValues target = after;
    target.rotation = nearestEuler(after.rotation, before.rotation);
    // Auto-key: un canal nou pornește din valoarea de dinainte, la începutul pistei (prima cheie a obiectului, sau
    // 0 s), ca mișcarea să se vadă de acolo până la cheia nouă (altfel o singură cheie ar ține valoarea peste tot).
    double start = 0.0;
    const ObjectTrack* existing = findTrack(std::as_const(animation), instance);
    if (existing != nullptr) {
        start = time;
        for (const Channel& c : existing->channels) {
            if (!c.keys.empty()) {
                start = std::min(start, c.keys.front().time);
            }
        }
    }
    std::vector<Property> written;
    for (int i = 0; i < 9; ++i) {
        const auto p = static_cast<Property>(i);
        if (onlyChanged && std::abs(target.get(p) - before.get(p)) <= valueTolerance(p)) {
            continue;
        }
        const bool created = existing == nullptr || findChannel(*existing, p) == nullptr;
        if (onlyChanged && created && time - start > tolerance(animation)) {
            setChannelKey(animation, instance, p, start, before.get(p));
        }
        setChannelKey(animation, instance, p, time, target.get(p));
        written.push_back(p);
        existing = findTrack(std::as_const(animation), instance);
    }
    return written;
}

math::Transform adjustWithoutKeys(Animation& animation, InstanceId instance, const TransformValues& before,
    const TransformValues& after, const math::Transform& base, const math::Vec3d& pivot)
{
    TransformValues target = after;
    target.rotation = nearestEuler(after.rotation, before.rotation);
    TransformValues baseValues = transformValues(base, pivot);
    ObjectTrack* track = findTrack(animation, instance);
    for (int i = 0; i < 9; ++i) {
        const auto p = static_cast<Property>(i);
        const double delta = target.get(p) - before.get(p);
        if (std::abs(delta) <= valueTolerance(p)) {
            continue;
        }
        if (Channel* c = track != nullptr ? findChannel(*track, p) : nullptr) {
            for (Key& k : c->keys) {
                k.value += delta;
            }
        } else {
            baseValues.set(p, baseValues.get(p) + delta);
        }
    }
    const math::Transform result = composeTransform(baseValues, pivot);
    // Baza se citește înapoi descompusă (|Y| ≤ 90°). Dacă rotația ei nouă trece pe cealaltă soluție Euler, iar o parte
    // din rotație e animată, axele neanimate ar căpăta alte unghiuri: rotația trece atunci toată pe canale (valorile
    // de bază, ținute constante), ca poza să rămână exact cea cerută.
    if (track != nullptr) {
        const math::Vec3d read = transformValues(result, pivot).rotation;
        const math::Vec3d intended = baseValues.rotation;
        bool rotationChannel = false;
        double start = 0.0;
        for (const Channel& c : track->channels) {
            const int i = static_cast<int>(c.property);
            if (i >= 3 && i < 6 && !c.keys.empty()) {
                start = rotationChannel ? std::min(start, c.keys.front().time) : c.keys.front().time;
                rotationChannel = true;
            }
        }
        bool flipped = false;
        for (int i = 0; i < 3; ++i) {
            flipped = flipped || std::abs(unwrapDegrees(read[i], intended[i]) - intended[i]) > 1e-6;
        }
        if (rotationChannel && flipped) {
            for (int i = 0; i < 3; ++i) {
                const auto p = static_cast<Property>(3 + i);
                if (findChannel(*track, p) == nullptr) {
                    setChannelKey(animation, instance, p, start, intended[i]);
                    track = findTrack(animation, instance); // setChannelKey poate muta pista
                }
            }
        }
    }
    return result;
}

void rebasePivot(Animation& animation, InstanceId instance, const math::Transform& base, const math::Vec3d& oldPivot,
    const math::Vec3d& newPivot)
{
    ObjectTrack* track = findTrack(animation, instance);
    if (track == nullptr) {
        return;
    }
    const ObjectTrack before = *track; // evaluarea se face pe cheile vechi
    const TransformValues baseOld = transformValues(base, oldPivot);
    const TransformValues baseNew = transformValues(base, newPivot);
    const double tol = tolerance(animation);
    // Momentele tuturor cheilor de transformare, cu easing-ul primei chei de la fiecare (pentru cheile noi).
    std::vector<std::pair<double, Easing>> times;
    bool rotatesOrScales = false;
    for (const Channel& c : before.channels) {
        if (!isTransform(c.property)) {
            continue;
        }
        rotatesOrScales = rotatesOrScales || static_cast<int>(c.property) >= static_cast<int>(Property::RotationX);
        for (const Key& k : c.keys) {
            if (std::none_of(times.begin(), times.end(), [&](const auto& t) { return std::abs(t.first - k.time) < tol; })) {
                times.emplace_back(k.time, k.easing);
            }
        }
    }
    std::sort(times.begin(), times.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
    // Poziția nouă a pivotului la un moment: unde ajunge pivotul nou în poza veche.
    const auto positionAt = [&](double t) {
        return composeTransform(evaluateValues(before, t, baseOld), oldPivot).applyToPoint(newPivot);
    };
    for (int axis = 0; axis < 3; ++axis) {
        const auto property = static_cast<Property>(axis);
        Channel* existing = findChannel(*track, property);
        if (existing != nullptr && !rotatesOrScales) {
            // Fără rotație / scară animate, diferența e aceeași peste tot: cheile rămân la momentele lor.
            for (Key& k : existing->keys) {
                k.value = positionAt(k.time)[axis];
            }
            continue;
        }
        if (!rotatesOrScales) {
            continue; // fără canal și fără rotație: poza de bază (cu pivotul nou) e deja corectă
        }
        // Cu rotație / scară animate, poziția pivotului nou variază: o cheie la fiecare moment cu chei, ca poza să
        // rămână exact aceeași la toate (canalul se creează doar dacă e nevoie).
        bool needed = existing != nullptr;
        std::vector<double> values;
        for (const auto& [t, easing] : times) {
            values.push_back(positionAt(t)[axis]);
            needed = needed || std::abs(values.back() - baseNew.position[axis]) > valueTolerance(property);
        }
        if (!needed) {
            continue;
        }
        Channel& channel = channelFor(*track, property);
        for (std::size_t i = 0; i < times.size(); ++i) {
            Key key;
            key.time = times[i].first;
            key.value = values[i];
            key.easing = times[i].second;
            upsert(channel.keys, key, tol, true); // o cheie existentă își păstrează easing-ul
        }
        for (Key& k : channel.keys) { // cheile existente de la alte momente: tot la pivotul nou
            k.value = positionAt(k.time)[axis];
        }
    }
}

std::vector<KeyRef> expandKeys(const Animation& animation, std::span<const KeyRef> refs)
{
    const double tol = tolerance(animation);
    std::vector<KeyRef> out;
    const auto add = [&](const KeyRef& r) {
        if (std::find(out.begin(), out.end(), r) == out.end()) {
            out.push_back(r);
        }
    };
    for (const KeyRef& ref : refs) {
        if (!ref.instance) {
            for (const CameraKey& k : animation.camera) {
                if (std::abs(k.time - ref.time) < tol) {
                    add({ {}, std::nullopt, k.time });
                }
            }
            continue;
        }
        const ObjectTrack* track = findTrack(animation, ref.instance);
        if (track == nullptr) {
            continue;
        }
        for (const Channel& c : track->channels) {
            if (ref.property && *ref.property != c.property) {
                continue;
            }
            for (const Key& k : c.keys) {
                if (std::abs(k.time - ref.time) < tol) {
                    add({ ref.instance, c.property, k.time });
                }
            }
        }
    }
    return out;
}

void removeKeys(Animation& animation, std::span<const KeyRef> refs)
{
    for (const KeyRef& r : expandKeys(animation, refs)) {
        if (!r.instance) {
            std::erase_if(animation.camera, [&](const CameraKey& k) { return k.time == r.time; });
        } else if (ObjectTrack* t = findTrack(animation, r.instance)) {
            if (Channel* c = findChannel(*t, *r.property)) {
                std::erase_if(c->keys, [&](const Key& k) { return k.time == r.time; });
            }
        }
    }
    dropEmpty(animation);
}

std::vector<KeyRef> moveKeys(Animation& animation, std::span<const KeyRef> refs, double delta)
{
    const std::vector<KeyRef> keys = expandKeys(animation, refs);
    // Întâi se scot toate (ca o cheie mutată să nu înlocuiască una care pleacă și ea), apoi se pun la loc.
    std::vector<std::pair<KeyRef, Key>> moved;
    std::vector<CameraKey> camera;
    for (const KeyRef& r : keys) {
        if (!r.instance) {
            for (auto it = animation.camera.begin(); it != animation.camera.end(); ++it) {
                if (it->time == r.time) {
                    camera.push_back(*it);
                    animation.camera.erase(it);
                    break;
                }
            }
        } else if (ObjectTrack* t = findTrack(animation, r.instance)) {
            if (Channel* c = findChannel(*t, *r.property)) {
                for (auto it = c->keys.begin(); it != c->keys.end(); ++it) {
                    if (it->time == r.time) {
                        moved.emplace_back(r, *it);
                        c->keys.erase(it);
                        break;
                    }
                }
            }
        }
    }
    const double tol = tolerance(animation);
    const auto shifted = [&](double t) { return std::clamp(t + delta, 0.0, animation.duration); };
    std::vector<KeyRef> out;
    for (CameraKey k : camera) {
        k.time = shifted(k.time);
        upsert(animation.camera, k, tol, false);
        out.push_back({ {}, std::nullopt, k.time });
    }
    for (auto [ref, key] : moved) {
        key.time = shifted(key.time);
        upsert(channelFor(trackFor(animation, ref.instance), *ref.property).keys, key, tol, false);
        out.push_back({ ref.instance, ref.property, key.time });
    }
    dropEmpty(animation);
    return out;
}

void setKeysEasing(Animation& animation, std::span<const KeyRef> refs, const Easing& easing)
{
    const double tol = tolerance(animation);
    for (const KeyRef& r : expandKeys(animation, refs)) {
        if (!r.instance) {
            if (CameraKey* k = keyAt(animation.camera, r.time, tol)) {
                k->easing = easing;
            }
        } else if (ObjectTrack* t = findTrack(animation, r.instance)) {
            if (Channel* c = findChannel(*t, *r.property)) {
                if (Key* k = keyAt(c->keys, r.time, tol)) {
                    k->easing = easing;
                }
            }
        }
    }
}

bool setKeyValue(Animation& animation, const KeyRef& ref, double value)
{
    if (!ref.instance || !ref.property || !isTransform(*ref.property)) {
        return false;
    }
    ObjectTrack* t = findTrack(animation, ref.instance);
    Channel* c = t != nullptr ? findChannel(*t, *ref.property) : nullptr;
    Key* k = c != nullptr ? keyAt(c->keys, ref.time, tolerance(animation)) : nullptr;
    if (k == nullptr) {
        return false;
    }
    k->value = value;
    return true;
}

std::vector<ClipboardKey> copyKeys(const Animation& animation, std::span<const KeyRef> refs)
{
    const std::vector<KeyRef> keys = expandKeys(animation, refs);
    if (keys.empty()) {
        return {};
    }
    double first = keys.front().time;
    for (const KeyRef& r : keys) {
        first = std::min(first, r.time);
    }
    std::vector<ClipboardKey> out;
    for (const KeyRef& r : keys) {
        ClipboardKey c;
        c.ref = r;
        c.ref.time = r.time - first;
        if (!r.instance) {
            for (const CameraKey& k : animation.camera) {
                if (k.time == r.time) {
                    c.camera = k;
                }
            }
        } else if (const ObjectTrack* t = findTrack(animation, r.instance)) {
            if (const Channel* ch = findChannel(*t, *r.property)) {
                for (const Key& k : ch->keys) {
                    if (k.time == r.time) {
                        c.key = k;
                    }
                }
            }
        }
        out.push_back(c);
    }
    return out;
}

std::vector<KeyRef> pasteKeys(Animation& animation, std::span<const ClipboardKey> keys, double time, InstanceId target)
{
    const double tol = tolerance(animation);
    std::vector<KeyRef> out;
    for (const ClipboardKey& c : keys) {
        const double t = time + c.ref.time;
        if (!c.ref.instance) {
            CameraKey k = c.camera;
            k.time = t;
            upsert(animation.camera, k, tol, false);
            out.push_back({ {}, std::nullopt, t });
        } else {
            Key k = c.key;
            k.time = t;
            const InstanceId instance = target ? target : c.ref.instance;
            upsert(channelFor(trackFor(animation, instance), *c.ref.property).keys, k, tol, false);
            out.push_back({ instance, c.ref.property, t });
        }
        animation.duration = std::max(animation.duration, t);
    }
    return out;
}

// --- Tastatura timeline-ului ---

std::vector<double> keyTimes(const Animation& animation, const std::set<InstanceId>& instances)
{
    std::vector<double> out;
    for (const ObjectTrack& t : animation.tracks) {
        if (!instances.empty() && !instances.contains(t.instance)) {
            continue;
        }
        for (const Channel& c : t.channels) {
            for (const Key& k : c.keys) {
                out.push_back(k.time);
            }
        }
    }
    if (instances.empty()) {
        for (const CameraKey& k : animation.camera) {
            out.push_back(k.time);
        }
    }
    std::sort(out.begin(), out.end());
    const double tol = tolerance(animation);
    out.erase(std::unique(out.begin(), out.end(), [tol](double a, double b) { return std::abs(a - b) < tol; }), out.end());
    return out;
}

std::optional<double> adjacentKeyTime(const Animation& animation, const std::set<InstanceId>& instances, double time,
    int direction)
{
    const double tol = tolerance(animation);
    const std::vector<double> times = keyTimes(animation, instances);
    if (direction > 0) {
        const auto it = std::find_if(times.begin(), times.end(), [&](double t) { return t > time + tol; });
        return it != times.end() ? std::optional(*it) : std::nullopt;
    }
    const auto it = std::find_if(times.rbegin(), times.rend(), [&](double t) { return t < time - tol; });
    return it != times.rend() ? std::optional(*it) : std::nullopt;
}

double stepFrames(const Animation& animation, double time, int frames)
{
    const double fps = std::max(animation.fps, 1);
    const double frame = time * fps;
    // De pe un cadru (cu o mică toleranță) se pleacă de la el; dintre cadre, de la cel din direcția pasului.
    double start = std::round(frame);
    if (std::abs(frame - start) > 1e-6) {
        start = frames > 0 ? std::floor(frame) : std::ceil(frame);
    }
    return std::clamp((start + frames) / fps, 0.0, std::max(animation.duration, 0.0));
}

// --- Migrarea ---

void setMatrixKeys(Animation& animation, InstanceId instance, std::span<const MatrixKey> keys,
    const math::Transform& base, const math::Vec3d& pivot)
{
    if (keys.empty()) {
        return;
    }
    const TransformValues baseValues = transformValues(base, pivot);
    std::vector<TransformValues> values;
    for (const MatrixKey& k : keys) {
        TransformValues v = transformValues(k.value, pivot);
        v.rotation = nearestEuler(v.rotation, values.empty() ? baseValues.rotation : values.back().rotation);
        values.push_back(v);
    }
    for (int i = 0; i < 9; ++i) {
        const auto p = static_cast<Property>(i);
        double lo = values.front().get(p);
        double hi = lo;
        for (const TransformValues& v : values) {
            lo = std::min(lo, v.get(p));
            hi = std::max(hi, v.get(p));
        }
        const double tol = valueTolerance(p) * 1000.0; // erorile de rotunjire ale descompunerii
        if (hi - lo > tol) {
            for (std::size_t k = 0; k < keys.size(); ++k) {
                setChannelKey(animation, instance, p, keys[k].time, values[k].get(p), keys[k].easing);
            }
        } else if (std::abs(values.front().get(p) - baseValues.get(p)) > tol) {
            setChannelKey(animation, instance, p, keys.front().time, values.front().get(p), keys.front().easing);
        }
    }
}

} // namespace aion::model
