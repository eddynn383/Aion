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
