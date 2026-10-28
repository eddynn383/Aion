#pragma once

#include "aion/core/model/Model.h"

#include <optional>
#include <set>
#include <vector>

// Evaluarea și editarea animațiilor (timeline-ul). Cheile au valori absolute; între două chei se interpolează cu
// easing-ul cheii de la început. Înainte de prima cheie se ține prima valoare, după ultima — ultima.
namespace aion::model {

[[nodiscard]] double ease(Easing easing, double t);

[[nodiscard]] const ObjectTrack* findTrack(const Animation& animation, InstanceId instance);
[[nodiscard]] std::optional<math::Transform> evaluateTransform(const ObjectTrack& track, double time);
[[nodiscard]] std::optional<Color> evaluateColor(const ObjectTrack& track, double time);
[[nodiscard]] std::optional<CameraKey> evaluateCamera(const Animation& animation, double time);

// Adaugă sau înlocuiește cheia de la momentul dat (o cheie la mai puțin de jumătate de cadru e „aceeași”).
void setTransformKey(Animation& animation, InstanceId instance, double time, const math::Transform& value);
void setColorKey(Animation& animation, InstanceId instance, double time, const Color& value);
void setCameraKey(Animation& animation, CameraKey key);

// Ce se poate selecta și edita pe timeline: o cheie de pe o pistă.
enum class KeyKind { Transform, Color, Camera };
// Șterge sau mută (la `newTime`) cheia de la `time` de pe pista dată. Pistele goale dispar.
void removeKey(Animation& animation, KeyKind kind, InstanceId instance, double time);
void moveKey(Animation& animation, KeyKind kind, InstanceId instance, double time, double newTime);

// Tastatura timeline-ului (Animare). Momentele cu chei, sortate și fără dubluri: ale instanțelor date, sau ale
// tuturor pistelor și ale camerei dacă mulțimea e goală.
[[nodiscard]] std::vector<double> keyTimes(const Animation& animation, const std::set<InstanceId>& instances);
// Momentul cheii următoare (direction > 0) sau anterioare (direction < 0) față de `time`; o cheie la mai puțin de
// jumătate de cadru de `time` e chiar cea de sub playhead și se sare. Nullopt dacă nu mai e nicio cheie.
[[nodiscard]] std::optional<double> adjacentKeyTime(const Animation& animation, const std::set<InstanceId>& instances,
    double time, int direction);
// Momentul aflat la `frames` cadre de `time`, pe grila cadrelor, ținut în [0, durată]. De pe un timp dintre cadre,
// un pas înainte duce la cadrul următor, unul înapoi la cel anterior.
[[nodiscard]] double stepFrames(const Animation& animation, double time, int frames);

} // namespace aion::model
