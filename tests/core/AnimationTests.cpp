#include "aion/core/io/AionFile.h"
#include "aion/core/model/AnimationEval.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

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

TEST_CASE("Animații: keyframe-uri de transformare, culoare și cameră, cu easing", "[core][model][animation]")
{
    Animation a;
    a.duration = 2.0;
    const model::InstanceId door { 3 };
    model::setTransformKey(a, door, 0.0, Transform {});
    model::setTransformKey(a, door, 2.0, Transform::translation({ 1000, 0, 0 }));
    model::setColorKey(a, door, 0.0, { 0, 0, 0, 255 });
    model::setColorKey(a, door, 2.0, { 200, 100, 0, 255 });

    const model::ObjectTrack* track = model::findTrack(a, door);
    REQUIRE(track);
    CHECK(origin(*model::evaluateTransform(*track, -1.0)).x == Approx(0.0));
    CHECK(origin(*model::evaluateTransform(*track, 1.0)).x == Approx(500.0)); // ease-in-out simetric
    CHECK(origin(*model::evaluateTransform(*track, 0.5)).x < 250.0); // pornește lent
    CHECK(origin(*model::evaluateTransform(*track, 5.0)).x == Approx(1000.0));
    const model::Color mid = *model::evaluateColor(*track, 1.0);
    CHECK(mid.r == 100);
    CHECK(mid.g == 50);

    // Liniar: exact proporțional.
    a.tracks.front().transform.front().easing = model::Easing::Linear;
    CHECK(origin(*model::evaluateTransform(*model::findTrack(a, door), 0.5)).x == Approx(250.0));

    // Aceeași poziție în timp înlocuiește cheia; mutarea și ștergerea.
    model::setTransformKey(a, door, 2.0, Transform::translation({ 2000, 0, 0 }));
    CHECK(model::findTrack(a, door)->transform.size() == 2);
    model::moveKey(a, model::KeyKind::Transform, door, 2.0, 1.0);
    CHECK(model::findTrack(a, door)->transform.back().time == Approx(1.0));
    model::removeKey(a, model::KeyKind::Transform, door, 0.0);
    model::removeKey(a, model::KeyKind::Transform, door, 1.0);
    model::removeKey(a, model::KeyKind::Color, door, 0.0);
    model::removeKey(a, model::KeyKind::Color, door, 2.0);
    CHECK(model::findTrack(a, door) == nullptr); // pista goală dispare

    // Camera.
    model::setCameraKey(a, { 0.0, { 0, 0, 0 }, 0.0, -0.3, 5000.0, model::Easing::Linear });
    model::setCameraKey(a, { 2.0, { 1000, 0, 0 }, 0.0, -0.3, 5000.0, model::Easing::Linear });
    CHECK(model::evaluateCamera(a, 1.0)->target.x == Approx(500.0));
}

TEST_CASE("Animații: se salvează în .aion și au undo ca orice altă schimbare", "[core][model][animation]")
{
    model::Model m;
    Animation a;
    a.name = "Ușa se deschide";
    model::setTransformKey(a, model::InstanceId { 1 }, 0.0, Transform {});
    model::setTransformKey(a, model::InstanceId { 1 }, 1.5, Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, 1.2));
    model::setCameraKey(a, { 0.5, { 1, 2, 3 }, 0.1, -0.2, 3000.0, model::Easing::EaseOut });
    m.addAnimation(a);
    const io::LoadResult loaded = io::readArchive(io::writeArchive(m, {}));
    REQUIRE(loaded.model);
    CHECK(loaded.model->sameAs(m));
    REQUIRE(loaded.model->animations().size() == 1);
    CHECK(loaded.model->animations().values().front().name == "Ușa se deschide");
}

TEST_CASE("Animații: tastatura — cheia următoare / anterioară și pașii de cadre", "[core][model][animation]")
{
    Animation a;
    a.duration = 4.0;
    a.fps = 10;
    const model::InstanceId door { 3 };
    const model::InstanceId car { 4 };
    model::setTransformKey(a, door, 0.5, Transform {});
    model::setTransformKey(a, door, 2.0, Transform::translation({ 1000, 0, 0 }));
    model::setColorKey(a, car, 1.0, { 0, 0, 0, 255 });
    model::setCameraKey(a, { 3.0, { 0, 0, 0 }, 0.0, 0.0, 1000.0, model::Easing::Linear });

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
