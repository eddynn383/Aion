#pragma once

#include "aion/core/model/Model.h"

#include <optional>
#include <set>
#include <span>
#include <string_view>
#include <vector>

// Evaluarea și editarea animațiilor (timeline-ul, ADR 0034). Cheile au valori absolute, pe canale (o proprietate a
// unui obiect); între două chei se interpolează cu easing-ul cheii de la început. Înainte de prima cheie se ține prima
// valoare, după ultima — ultima. Proprietățile fără canal rămân cum sunt în model (poza de bază).
namespace aion::model {

// --- Easing ---

// Valoarea curbei la momentul `t` ∈ [0, 1] (x-ul curbei Bézier e timpul; se rezolvă t → parametru cu Newton și
// bisecție). Rezultatul poate ieși din [0, 1] la curbele „back”.
[[nodiscard]] double ease(const Easing& easing, double t);
// Presetările, în ordinea din editor: linear, ease, easeIn, easeOut, easeInOut, easeInBack, easeOutBack, step.
[[nodiscard]] const std::vector<Easing>& easingPresets();
[[nodiscard]] std::optional<Easing> easingPreset(std::string_view name);

// --- Proprietățile ---

// „position.x” … „scale.z”, „color”.
[[nodiscard]] std::string_view propertyName(Property property);
[[nodiscard]] std::optional<Property> propertyFromName(std::string_view name);
[[nodiscard]] constexpr bool isTransform(Property p) { return p != Property::Color; }

// Valorile de transformare ale unei instanțe, ca în Inspector: poziția pivotului (în coordonatele părintelui),
// rotația (grade, Euler XYZ), scara. `pivot` e pivotul în coordonatele definiției (`pivotLocal`).
struct TransformValues {
    math::Vec3d position { 0.0 };
    math::Vec3d rotation { 0.0 };
    math::Vec3d scale { 1.0 };

    [[nodiscard]] double get(Property p) const;
    void set(Property p, double value);
};
[[nodiscard]] TransformValues transformValues(const math::Transform& transform, const math::Vec3d& pivot);
// Transformarea din valori: rotația și scara se fac în jurul pivotului (care ajunge în `position`).
[[nodiscard]] math::Transform composeTransform(const TransformValues& values, const math::Vec3d& pivot);
// Unghiul echivalent cu `degrees` (± 360°) cel mai apropiat de `reference` (fără salturi de 360° între chei).
[[nodiscard]] double unwrapDegrees(double degrees, double reference);

// --- Evaluarea ---

[[nodiscard]] const ObjectTrack* findTrack(const Animation& animation, InstanceId instance);
[[nodiscard]] const Channel* findChannel(const ObjectTrack& track, Property property);
[[nodiscard]] std::optional<double> evaluateChannel(const Channel& channel, double time);
[[nodiscard]] std::optional<Color> evaluateColor(const ObjectTrack& track, double time);
// Valorile de transformare la `time`: canalele pistei peste valorile de bază.
[[nodiscard]] TransformValues evaluateValues(const ObjectTrack& track, double time, const TransformValues& base);
// Transformarea instanței la `time`, pornind de la transformarea ei din model (`base`) și pivotul ei (local);
// nullopt dacă pista nu are canale de transformare.
[[nodiscard]] std::optional<math::Transform> evaluateInstance(
    const ObjectTrack& track, double time, const math::Transform& base, const math::Vec3d& pivot);
[[nodiscard]] std::optional<CameraKey> evaluateCamera(const Animation& animation, double time);

// --- Editarea ---

// Adaugă sau înlocuiește cheia de la momentul dat (o cheie la mai puțin de jumătate de cadru e „aceeași” și își
// păstrează easing-ul). O cheie nouă primește `easing`, dacă e dat, altfel easing-ul implicit.
void setChannelKey(Animation& animation, InstanceId instance, Property property, double time, double value,
    const std::optional<Easing>& easing = std::nullopt);
void setColorKey(Animation& animation, InstanceId instance, double time, const Color& value);
void setCameraKey(Animation& animation, CameraKey key);
// Cheile de transformare din `after` (valorile față de pivot): toate cele 9 canale, sau (`onlyChanged`) doar cele care
// diferă de `before` — auto-key-ul scrie doar ce s-a mișcat; un canal nou pornește atunci cu valoarea din `before` la
// începutul pistei (prima cheie a obiectului, sau 0 s). Unghiurile se despachetează față de `before`. Întoarce
// proprietățile scrise.
std::vector<Property> keyTransform(Animation& animation, InstanceId instance, double time, const TransformValues& before,
    const TransformValues& after, bool onlyChanged);

// O cheie de pe timeline: a camerei (`instance` nul), a unui canal, sau — pe rândul sumar al obiectului
// (`property` gol) — toate cheile lui de la acel moment.
struct KeyRef {
    InstanceId instance;
    std::optional<Property> property;
    double time = 0.0;
    bool operator==(const KeyRef&) const = default;
};
// Cheile atinse de referințe (fiecare o dată), ca referințe pe canal (sau pe cameră).
[[nodiscard]] std::vector<KeyRef> expandKeys(const Animation& animation, std::span<const KeyRef> refs);
// Șterge cheile. Canalele și pistele goale dispar.
void removeKeys(Animation& animation, std::span<const KeyRef> refs);
// Mută cheile cu `delta` secunde (toate deodată, ținute în [0, durată]); o cheie mutată peste alta o înlocuiește.
// Întoarce referințele noi (pe canal).
std::vector<KeyRef> moveKeys(Animation& animation, std::span<const KeyRef> refs, double delta);
// Easing-ul cheilor (pentru tranziția care pleacă din ele).
void setKeysEasing(Animation& animation, std::span<const KeyRef> refs, const Easing& easing);
// Valoarea unei chei de pe un canal numeric (Inspector-ul timeline-ului).
bool setKeyValue(Animation& animation, const KeyRef& ref, double value);

// Copiere / lipire: cheile, cu timpul relativ la cea mai devreme.
struct ClipboardKey {
    KeyRef ref; // pe canal sau pe cameră; `ref.time` relativ
    Key key;
    CameraKey camera;
};
[[nodiscard]] std::vector<ClipboardKey> copyKeys(const Animation& animation, std::span<const KeyRef> refs);
// Lipește cheile la `time`, pe aceleași canale: ale aceluiași obiect, sau toate pe `target` (dacă e dat). Întoarce
// referințele cheilor lipite.
std::vector<KeyRef> pasteKeys(Animation& animation, std::span<const ClipboardKey> keys, double time, InstanceId target = {});

// --- Tastatura timeline-ului (Animare) ---

// Momentele cu chei, sortate și fără dubluri: ale instanțelor date, sau ale tuturor pistelor și ale camerei dacă
// mulțimea e goală.
[[nodiscard]] std::vector<double> keyTimes(const Animation& animation, const std::set<InstanceId>& instances);
// Momentul cheii următoare (direction > 0) sau anterioare (direction < 0) față de `time`; o cheie la mai puțin de
// jumătate de cadru de `time` e chiar cea de sub playhead și se sare. Nullopt dacă nu mai e nicio cheie.
[[nodiscard]] std::optional<double> adjacentKeyTime(const Animation& animation, const std::set<InstanceId>& instances,
    double time, int direction);
// Momentul aflat la `frames` cadre de `time`, pe grila cadrelor, ținut în [0, durată]. De pe un timp dintre cadre,
// un pas înainte duce la cadrul următor, unul înapoi la cel anterior.
[[nodiscard]] double stepFrames(const Animation& animation, double time, int frames);

// --- Migrarea fișierelor vechi (formatVersion 1: cheile țineau matricea întreagă) ---

struct MatrixKey {
    double time = 0.0;
    math::Transform value;
    Easing easing {};
};
// Descompune cheile în canale (unghiurile despachetate între chei). Se păstrează canalele care variază între chei sau
// a căror valoare diferă de cea din `base` (transformarea instanței din model); pentru celelalte nu e nevoie de canal.
void setMatrixKeys(Animation& animation, InstanceId instance, std::span<const MatrixKey> keys,
    const math::Transform& base, const math::Vec3d& pivot);

} // namespace aion::model
