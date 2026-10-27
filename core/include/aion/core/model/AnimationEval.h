#pragma once

#include "aion/core/model/Model.h"

#include <optional>

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

} // namespace aion::model
