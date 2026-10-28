#include "aion/core/io/AionFile.h"
#include "aion/core/model/AnimationEval.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <numbers>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Animation;

namespace {

Vec3d origin(const Transform& t)
{
    return t.applyToPoint({ 0, 0, 0 });
}

} // namespace

TEST_CASE("Animații: canale pe proprietate, culoare și cameră, cu easing", "[core][model][animation]")
{
    using model::Property;
    Animation a;
    a.duration = 2.0;
    const model::InstanceId door { 3 };
    model::setChannelKey(a, door, Property::PositionX, 0.0, 0.0);
    model::setChannelKey(a, door, Property::PositionX, 2.0, 1000.0);
    model::setColorKey(a, door, 0.0, { 0, 0, 0, 255 });
    model::setColorKey(a, door, 2.0, { 200, 100, 0, 255 });

    const model::ObjectTrack* track = model::findTrack(a, door);
    REQUIRE(track);
    REQUIRE(track->channels.size() == 2); // doar proprietățile animate, în ordinea lor
    CHECK(track->channels[0].property == Property::PositionX);
    CHECK(track->channels[1].property == Property::Color);
    const model::Channel& x = *model::findChannel(*track, Property::PositionX);
    CHECK(*model::evaluateChannel(x, -1.0) == Approx(0.0));
    CHECK(*model::evaluateChannel(x, 1.0) == Approx(500.0)); // ease-in-out simetric
    CHECK(*model::evaluateChannel(x, 0.5) < 250.0); // pornește lent
    CHECK(*model::evaluateChannel(x, 5.0) == Approx(1000.0));
    const model::Color mid = *model::evaluateColor(*track, 1.0);
    CHECK(mid.r == 100);
    CHECK(mid.g == 50);

    // Transformarea: doar X vine din canal, restul (Y = 500) din poza de bază.
    const auto posed = model::evaluateInstance(*track, 1.0, Transform::translation({ 0, 500, 0 }), { 0, 0, 0 });
    REQUIRE(posed);
    CHECK(origin(*posed).x == Approx(500.0));
    CHECK(origin(*posed).y == Approx(500.0));

    // Liniar: exact proporțional.
    a.tracks.front().channels.front().keys.front().easing = *model::easingPreset("linear");
    CHECK(*model::evaluateChannel(*model::findChannel(*model::findTrack(a, door), Property::PositionX), 0.5)
        == Approx(250.0));

    // Aceeași poziție în timp înlocuiește cheia (și îi păstrează easing-ul); mutarea și ștergerea.
    model::setChannelKey(a, door, Property::PositionX, 2.0, 2000.0);
    CHECK(model::findChannel(*model::findTrack(a, door), Property::PositionX)->keys.size() == 2);
    const std::vector<model::KeyRef> moved = model::moveKeys(a, std::vector<model::KeyRef> { { door, Property::PositionX, 2.0 } }, -1.0);
    REQUIRE(moved.size() == 1);
    CHECK(moved.front().time == Approx(1.0));
    CHECK(model::findChannel(*model::findTrack(a, door), Property::PositionX)->keys.back().time == Approx(1.0));
    // Rândul sumar (fără proprietate): toate cheile obiectului de la acel moment.
    model::removeKeys(a, std::vector<model::KeyRef> { { door, std::nullopt, 0.0 } });
    CHECK(model::findChannel(*model::findTrack(a, door), Property::PositionX)->keys.size() == 1);
    CHECK(model::findChannel(*model::findTrack(a, door), Property::Color)->keys.size() == 1);
    model::removeKeys(a, std::vector<model::KeyRef> { { door, Property::PositionX, 1.0 }, { door, Property::Color, 2.0 } });
    CHECK(model::findTrack(a, door) == nullptr); // pista goală dispare

    // Camera.
    model::setCameraKey(a, { 0.0, { 0, 0, 0 }, 0.0, -0.3, 5000.0, *model::easingPreset("linear") });
    model::setCameraKey(a, { 2.0, { 1000, 0, 0 }, 0.0, -0.3, 5000.0, *model::easingPreset("linear") });
    CHECK(model::evaluateCamera(a, 1.0)->target.x == Approx(500.0));
    model::removeKeys(a, std::vector<model::KeyRef> { { {}, std::nullopt, 2.0 } });
    CHECK(a.camera.size() == 1);
}

TEST_CASE("Animații: rotația animată se face în jurul pivotului", "[core][model][animation]")
{
    using model::Property;
    Animation a;
    const model::InstanceId box { 1 };
    // Pivotul în colțul (0, 0, 0) al cutiei; rotație pe Z de la 0° la 90°.
    model::setChannelKey(a, box, Property::RotationZ, 0.0, 0.0);
    model::setChannelKey(a, box, Property::RotationZ, 1.0, 90.0);
    const model::ObjectTrack& track = *model::findTrack(a, box);
    const Vec3d pivot { 0, 0, 0 };
    const Transform end = *model::evaluateInstance(track, 1.0, Transform {}, pivot);
    CHECK(glm::length(end.applyToPoint(pivot)) < 1e-9);
    CHECK(glm::length(end.applyToPoint({ 1000, 0, 0 }) - Vec3d { 0, 1000, 0 }) < 1e-9);
    // La jumătate (45°), colțul opus e tot la 1000 mm de pivot: un arc în jurul lui.
    const Transform half = *model::evaluateInstance(track, 0.5, Transform {}, pivot);
    CHECK(glm::length(half.applyToPoint({ 1000, 0, 0 })) == Approx(1000.0));
    CHECK(half.applyToPoint({ 1000, 0, 0 }).y == Approx(1000.0 * std::sin(std::numbers::pi / 4)));

    // Descompunerea și recompunerea sunt inverse (cu scară și oglindire).
    const Transform t = Transform::translation({ 10, 20, 30 }) * Transform::rotation({ 0, 0, 0 }, glm::normalize(Vec3d { 1, 2, 3 }), 0.7)
        * Transform::scaling({ 0, 0, 0 }, { -2, 3, 0.5 });
    const Vec3d p { 100, 200, 300 };
    const Transform back = model::composeTransform(model::transformValues(t, p), p);
    for (const Vec3d q : { Vec3d { 0, 0, 0 }, Vec3d { 1000, 0, 0 }, Vec3d { 0, 0, 1000 }, Vec3d { 30, -40, 50 } }) {
        CHECK(glm::length(back.applyToPoint(q) - t.applyToPoint(q)) < 1e-6);
    }
}

TEST_CASE("Animații: auto-key scrie doar canalele schimbate, cu unghiurile despachetate", "[core][model][animation]")
{
    using model::Property;
    const model::InstanceId box { 1 };
    const Vec3d pivot { 500, 500, 500 };
    const model::TransformValues rest = model::transformValues(Transform {}, pivot);

    // Move pe X → doar position.x.
    Animation a;
    std::vector<Property> written = model::keyTransform(a, box, 1.0, rest,
        model::transformValues(Transform::translation({ 300, 0, 0 }), pivot), true);
    CHECK(written == std::vector<Property> { Property::PositionX });
    REQUIRE(model::findTrack(a, box)->channels.size() == 1);
    // Un canal nou pornește din valoarea de dinainte, la începutul pistei (aici 0 s): mișcarea se vede.
    const model::Channel& px = model::findTrack(a, box)->channels.front();
    REQUIRE(px.keys.size() == 2);
    CHECK(px.keys[0].time == Approx(0.0));
    CHECK(px.keys[0].value == Approx(500.0));
    CHECK(px.keys[1].value == Approx(800.0));
    // Rotația adăugată la 2 s pornește de la prima cheie a obiectului (0 s); X primește doar cheia de la 2 s.
    model::TransformValues moved = rest;
    moved.position.x = 800.0;
    model::TransformValues turned = moved;
    turned.rotation.z = 30.0;
    written = model::keyTransform(a, box, 2.0, moved, turned, true);
    CHECK(written == std::vector<Property> { Property::RotationZ });
    const model::Channel& rz = *model::findChannel(*model::findTrack(a, box), Property::RotationZ);
    REQUIRE(rz.keys.size() == 2);
    CHECK(rz.keys[0].time == Approx(0.0));
    CHECK(rz.keys[0].value == Approx(0.0));
    CHECK(rz.keys[1].time == Approx(2.0));

    // Rotate pe Z în jurul pivotului → doar rotation.z.
    Animation b;
    written = model::keyTransform(b, box, 1.0, rest,
        model::transformValues(Transform::rotation(pivot, { 0, 0, 1 }, std::numbers::pi / 2), pivot), true);
    CHECK(written == std::vector<Property> { Property::RotationZ });
    CHECK(model::findTrack(b, box)->channels.front().keys.back().value == Approx(90.0));

    // De la 170° (valoarea canalului) cu încă 20°: 190°, nu −170°.
    model::TransformValues before = rest;
    before.rotation.z = 170.0;
    model::TransformValues after = model::transformValues(
        Transform::rotation(pivot, { 0, 0, 1 }, glm::radians(190.0)), pivot); // descompus: −170°
    CHECK(after.rotation.z == Approx(-170.0));
    Animation c;
    written = model::keyTransform(c, box, 2.0, before, after, true);
    CHECK(written == std::vector<Property> { Property::RotationZ });
    CHECK(model::findTrack(c, box)->channels.front().keys.back().value == Approx(190.0));

    // Keyframe manual: toate cele 9 canale de transformare.
    Animation d;
    CHECK(model::keyTransform(d, box, 0.0, rest, rest, false).size() == 9);
}

TEST_CASE("Animații: easing Bézier ca în CSS, presetări și curbe proprii", "[core][model][animation]")
{
    const model::Easing linear = *model::easingPreset("linear");
    CHECK(model::ease(linear, 0.3) == Approx(0.3));
    CHECK(model::ease(*model::easingPreset("easeInOut"), 0.5) == Approx(0.5));
    CHECK(model::ease(*model::easingPreset("easeIn"), 0.5) < 0.5);
    CHECK(model::ease(*model::easingPreset("easeOut"), 0.5) > 0.5);
    // `ease` din CSS la jumătate: 0.8024 (valoarea din browsere).
    CHECK(model::ease(*model::easingPreset("ease"), 0.5) == Approx(0.8024033877).margin(1e-6));
    // Back: depășește 1 înainte de final, apoi revine.
    const model::Easing back = *model::easingPreset("easeOutBack");
    CHECK(model::ease(back, 0.6) > 1.0);
    CHECK(model::ease(back, 1.0) == Approx(1.0));
    // Step: salt la final.
    CHECK(model::ease(*model::easingPreset("step"), 0.99) == Approx(0.0));
    CHECK(model::ease(*model::easingPreset("step"), 1.0) == Approx(1.0));
    // O curbă proprie: monotonă pe x, capetele fixe.
    const model::Easing mine { "Ușa mea", 0.8, 0.0, 0.2, 1.0, false };
    double previous = 0.0;
    for (int i = 0; i <= 100; ++i) {
        const double y = model::ease(mine, i / 100.0);
        CHECK(y >= previous - 1e-9);
        previous = y;
    }
    CHECK(model::ease(mine, 0.0) == Approx(0.0));
    CHECK(model::ease(mine, 1.0) == Approx(1.0));
    CHECK(model::easingPresets().size() == 8);
    CHECK_FALSE(model::easingPreset("nu-există").has_value());
}

TEST_CASE("Animații: copiere / lipire, easing și valoarea unei chei", "[core][model][animation]")
{
    using model::Property;
    Animation a;
    a.duration = 5.0;
    const model::InstanceId door { 3 };
    const model::InstanceId gate { 4 };
    model::setChannelKey(a, door, Property::RotationZ, 1.0, 0.0);
    model::setChannelKey(a, door, Property::RotationZ, 1.5, 90.0);
    model::setColorKey(a, door, 1.5, { 255, 0, 0, 255 });

    const std::vector<model::KeyRef> refs { { door, std::nullopt, 1.0 }, { door, std::nullopt, 1.5 } };
    const std::vector<model::ClipboardKey> clip = model::copyKeys(a, refs);
    CHECK(clip.size() == 3);
    // Pe același obiect, la 3 s: cheile ajung la 3 și 3.5.
    const std::vector<model::KeyRef> pasted = model::pasteKeys(a, clip, 3.0);
    CHECK(pasted.size() == 3);
    const model::Channel& rz = *model::findChannel(*model::findTrack(a, door), Property::RotationZ);
    REQUIRE(rz.keys.size() == 4);
    CHECK(rz.keys[2].time == Approx(3.0));
    CHECK(rz.keys[3].value == Approx(90.0));
    // Pe alt obiect (poarta).
    model::pasteKeys(a, clip, 0.0, gate);
    CHECK(model::findChannel(*model::findTrack(a, gate), Property::RotationZ)->keys.size() == 2);
    CHECK(model::findChannel(*model::findTrack(a, gate), Property::Color)->keys.front().time == Approx(0.5));

    // Easing pe o cheie; valoarea unei chei.
    model::setKeysEasing(a, std::vector<model::KeyRef> { { door, Property::RotationZ, 1.0 } }, *model::easingPreset("linear"));
    CHECK(model::findChannel(*model::findTrack(a, door), Property::RotationZ)->keys.front().easing.name == "linear");
    CHECK(model::setKeyValue(a, { door, Property::RotationZ, 1.5 }, 45.0));
    CHECK(model::findChannel(*model::findTrack(a, door), Property::RotationZ)->keys[1].value == Approx(45.0));
    CHECK_FALSE(model::setKeyValue(a, { door, Property::Color, 1.5 }, 1.0));
}

TEST_CASE("Animații: migrarea cheilor vechi (matricea întreagă) în canale", "[core][model][animation]")
{
    using model::Property;
    Animation a;
    const model::InstanceId box { 1 };
    const Vec3d pivot { 0, 0, 0 };
    // Cheile vechi: X de la 0 la 1000, Y ținut la 0 (dar obiectul din model stă la Y = 500), rotația Z 170° → 190°.
    const auto key = [&](double t, double x, double degrees) {
        return model::MatrixKey { t, Transform::translation({ x, 0, 0 }) * Transform::rotation(pivot, { 0, 0, 1 }, glm::radians(degrees)),
            *model::easingPreset("linear") };
    };
    const std::vector<model::MatrixKey> keys { key(0.0, 0.0, 170.0), key(2.0, 1000.0, 190.0) };
    model::setMatrixKeys(a, box, keys, Transform::translation({ 0, 500, 0 }), pivot);
    const model::ObjectTrack& track = *model::findTrack(a, box);
    // X și rotația Z variază; Y e constant, dar diferit de model → o cheie; restul nu au canal.
    REQUIRE(track.channels.size() == 3);
    CHECK(track.channels[0].property == Property::PositionX);
    CHECK(track.channels[1].property == Property::PositionY);
    CHECK(track.channels[1].keys.size() == 1);
    CHECK(track.channels[2].property == Property::RotationZ);
    CHECK(track.channels[2].keys[0].value == Approx(170.0));
    CHECK(track.channels[2].keys[1].value == Approx(190.0)); // despachetat, nu −170
    // Pozele la chei sunt cele vechi.
    for (const model::MatrixKey& k : keys) {
        const Transform t = *model::evaluateInstance(track, k.time, Transform::translation({ 0, 500, 0 }), pivot);
        CHECK(glm::length(t.applyToPoint({ 300, 400, 0 }) - k.value.applyToPoint({ 300, 400, 0 })) < 1e-6);
    }
    CHECK(track.channels[0].keys[0].easing.name == "linear");
}

TEST_CASE("Animații: se salvează în .aion și au undo ca orice altă schimbare", "[core][model][animation]")
{
    using model::Property;
    model::Model m;
    Animation a;
    a.name = "Ușa se deschide";
    model::setChannelKey(a, model::InstanceId { 1 }, Property::RotationZ, 0.0, 0.0);
    model::setChannelKey(a, model::InstanceId { 1 }, Property::RotationZ, 1.5, 68.75,
        model::Easing { "Ușa mea", 0.1, 0.7, 0.3, 1.1, false });
    model::setColorKey(a, model::InstanceId { 1 }, 1.0, { 10, 20, 30, 200 });
    model::setCameraKey(a, { 0.5, { 1, 2, 3 }, 0.1, -0.2, 3000.0, *model::easingPreset("easeOut") });
    m.addAnimation(a);
    const io::LoadResult loaded = io::readArchive(io::writeArchive(m, {}));
    REQUIRE(loaded.model);
    CHECK(loaded.model->sameAs(m));
    REQUIRE(loaded.model->animations().size() == 1);
    CHECK(loaded.model->animations().values().front() == a);
    CHECK(loaded.model->animations().values().front().name == "Ușa se deschide");
}

TEST_CASE("Animații: tastatura — cheia următoare / anterioară și pașii de cadre", "[core][model][animation]")
{
    Animation a;
    a.duration = 4.0;
    a.fps = 10;
    const model::InstanceId door { 3 };
    const model::InstanceId car { 4 };
    model::setChannelKey(a, door, model::Property::PositionX, 0.5, 0.0);
    model::setChannelKey(a, door, model::Property::RotationZ, 2.0, 90.0);
    model::setColorKey(a, car, 1.0, { 0, 0, 0, 255 });
    model::setCameraKey(a, { 3.0, { 0, 0, 0 }, 0.0, 0.0, 1000.0, *model::easingPreset("linear") });

    // Fără selecție: cheile tuturor pistelor și ale camerei, sortate și fără dubluri.
    CHECK(model::keyTimes(a, {}) == std::vector<double> { 0.5, 1.0, 2.0, 3.0 });
    CHECK(model::keyTimes(a, { door }) == std::vector<double> { 0.5, 2.0 });

    CHECK(model::adjacentKeyTime(a, {}, 0.0, +1) == 0.5);
    CHECK(model::adjacentKeyTime(a, {}, 0.5, +1) == 1.0); // cheia de sub playhead nu contează
    CHECK_FALSE(model::adjacentKeyTime(a, {}, 0.52, -1).has_value()); // la sub jumătate de cadru e „aceeași”
    CHECK(model::adjacentKeyTime(a, {}, 0.6, -1) == 0.5);
    CHECK(model::adjacentKeyTime(a, { door }, 1.0, +1) == 2.0);
    CHECK(model::adjacentKeyTime(a, { door }, 1.0, -1) == 0.5);
    CHECK_FALSE(model::adjacentKeyTime(a, { door }, 2.0, +1).has_value());
    CHECK_FALSE(model::adjacentKeyTime(a, {}, 0.5, -1).has_value());

    // Pașii de cadre: rotunjiți la cadru (1/10 s) și ținuți în durata animației.
    CHECK(model::stepFrames(a, 1.0, 1) == Approx(1.1));
    CHECK(model::stepFrames(a, 1.0, -10) == Approx(0.0));
    CHECK(model::stepFrames(a, 1.04, 1) == Approx(1.1)); // de pe un timp între cadre: cadrul următor
    CHECK(model::stepFrames(a, 1.04, -1) == Approx(1.0));
    CHECK(model::stepFrames(a, 3.95, 10) == Approx(4.0));
    CHECK(model::stepFrames(a, 0.0, -1) == Approx(0.0));
}

TEST_CASE("Animații: un fișier vechi (formatVersion 1) se deschide cu animațiile pe canale", "[core][model][animation][io]")
{
    // Un obiect la Y = 500 și o animație în formatul vechi: X de la 0 la 1000 (matricea întreagă), culoarea, camera.
    model::Model m;
    model::InstanceId id;
    {
        model::Instance inst;
        inst.parent = m.root();
        inst.definition = m.addDefinition(model::Definition { "Cutie" });
        inst.transform = Transform::translation({ 0, 500, 0 });
        inst.pivot = Vec3d { 0, 0, 0 };
        id = m.addInstance(inst);
    }
    std::string text = io::toJson(m);
    const auto replace = [&](const std::string& from, const std::string& to) {
        const std::size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        text.replace(at, from.size(), to);
    };
    replace("\"formatVersion\": 2", "\"formatVersion\": 1");
    const auto matrix = [](double x) {
        return "1,0,0,0, 0,1,0,0, 0,0,1,0, " + std::to_string(x) + ",500,0,1";
    };
    replace("\"animations\": []",
        "\"animations\": [{\"id\": 1, \"name\": \"Veche\", \"duration\": 2, \"fps\": 30, \"loop\": false,"
        "\"tracks\": [{\"instance\": " + std::to_string(id.value) + ", \"transform\": [[0, 0, " + matrix(0) + "], [2, 1, "
            + matrix(1000) + "]], \"color\": [[0, 2, 10, 20, 30, 255]]}],"
        "\"camera\": [[0.5, 3, 1, 2, 3, 0.1, -0.2, 3000]]}]");
    const io::LoadResult loaded = io::fromJson(text);
    REQUIRE(loaded.model);
    REQUIRE(loaded.model->animations().size() == 1);
    const Animation& a = loaded.model->animations().values().front();
    const model::ObjectTrack& track = *model::findTrack(a, id);
    // Doar X variază (Y e cel din model, deci fără canal), plus culoarea.
    REQUIRE(track.channels.size() == 2);
    CHECK(track.channels[0].property == model::Property::PositionX);
    CHECK(track.channels[0].keys[1].value == Approx(1000.0));
    CHECK(track.channels[0].keys[0].easing.name == "linear"); // 0 = Linear în formatul vechi
    CHECK(track.channels[0].keys[1].easing.name == "easeInOut");
    CHECK(track.channels[1].property == model::Property::Color);
    CHECK(track.channels[1].keys[0].color.g == 20);
    CHECK(track.channels[1].keys[0].easing.name == "easeIn"); // 2 = EaseIn
    CHECK(a.camera.front().easing.name == "easeOut"); // 3 = EaseOut
    // Salvat din nou, rămâne la fel.
    const io::LoadResult again = io::fromJson(io::toJson(*loaded.model));
    REQUIRE(again.model);
    CHECK(again.model->animations().values().front() == a);
}

TEST_CASE("Animații: easing-ul implicit al animației pentru cheile noi", "[core][model][animation]")
{
    using model::Property;
    Animation a;
    a.defaultEasing = *model::easingPreset("linear");
    const model::InstanceId box { 1 };
    model::setChannelKey(a, box, Property::PositionX, 0.0, 0.0);
    model::setColorKey(a, box, 1.0, { 1, 2, 3, 255 });
    model::setChannelKey(a, box, Property::PositionX, 1.0, 10.0, *model::easingPreset("step")); // dat explicit
    const model::ObjectTrack& t = *model::findTrack(a, box);
    CHECK(model::findChannel(t, Property::PositionX)->keys[0].easing.name == "linear");
    CHECK(model::findChannel(t, Property::PositionX)->keys[1].easing.name == "step");
    CHECK(model::findChannel(t, Property::Color)->keys[0].easing.name == "linear");
    // O cheie înlocuită își păstrează easing-ul.
    a.defaultEasing = *model::easingPreset("easeOut");
    model::setChannelKey(a, box, Property::PositionX, 0.0, 5.0);
    CHECK(model::findChannel(*model::findTrack(a, box), Property::PositionX)->keys[0].easing.name == "linear");

    // Se salvează în fișier.
    model::Model m;
    m.addAnimation(a);
    const io::LoadResult loaded = io::fromJson(io::toJson(m));
    REQUIRE(loaded.model);
    CHECK(loaded.model->animations().values().front().defaultEasing.name == "easeOut");
}

TEST_CASE("Animații: cu auto-key oprit, o mutare schimbă poza de bază sau mută tot canalul", "[core][model][animation]")
{
    using model::Property;
    Animation a;
    const model::InstanceId car { 7 };
    const Vec3d pivot { 0, 0, 0 };
    // X e animat (0 → 1000), Y nu.
    model::setChannelKey(a, car, Property::PositionX, 0.0, 0.0);
    model::setChannelKey(a, car, Property::PositionX, 2.0, 1000.0);
    const Transform base = Transform::translation({ 0, 200, 0 });
    const model::ObjectTrack& track = *model::findTrack(a, car);
    const model::TransformValues before = model::evaluateValues(track, 1.0, model::transformValues(base, pivot));
    // La 1 s, obiectul e mutat cu 300 pe X și 50 pe Y.
    model::TransformValues after = before;
    after.position += Vec3d { 300, 50, 0 };
    const Transform newBase = model::adjustWithoutKeys(a, car, before, after, base, pivot);
    // Y (neanimat) intră în poza de bază; X (animat) își mută toate cheile cu 300.
    CHECK(newBase.applyToPoint(pivot).y == Approx(250.0));
    CHECK(newBase.applyToPoint(pivot).x == Approx(0.0));
    const model::Channel& x = *model::findChannel(*model::findTrack(a, car), Property::PositionX);
    CHECK(x.keys[0].value == Approx(300.0));
    CHECK(x.keys[1].value == Approx(1300.0));
    CHECK(model::findChannel(*model::findTrack(a, car), Property::PositionY) == nullptr); // nicio cheie nouă
}
