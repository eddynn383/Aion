#include "aion/core/geom/Primitives.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/AnimationPlayer.h"
#include "aion/core/model/Operations.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aion;
using Catch::Approx;
using model::AnimationId;
using model::InstanceId;
using model::Model;
using model::PlayDirection;
using model::PlayRequest;

namespace {

model::ObjectTrack swing(InstanceId instance)
{
    model::Channel c;
    c.property = model::Property::RotationZ;
    c.keys = { { 0.0, 0.0, {}, {} }, { 1.0, 90.0, {}, {} } };
    return { instance, { c } };
}

// O scenă cu două uși (A, B) și câte o animație de 1 s pentru fiecare (rotație 0 → 90°).
struct Doors {
    Model base;
    InstanceId a;
    InstanceId b;
    AnimationId doorA;
    AnimationId doorB;
    Model runtime;
    model::AnimationPlayer player;

    Doors()
    {
        const model::DefinitionId d = base.addDefinition(model::Definition { "Ușă", false });
        geom::buildPrimitive(base.mesh(d), geom::Primitive::Box, { 400, 20, 800 }, 24);
        model::Instance inst;
        inst.parent = base.root();
        inst.definition = d;
        a = base.addInstance(inst);
        inst.transform = math::Transform::translation({ 1000, 0, 0 });
        b = base.addInstance(inst);
        model::Animation anim;
        anim.duration = 1.0;
        anim.name = "Ușa A";
        anim.tracks = { swing(a) };
        doorA = base.addAnimation(anim);
        anim.name = "Ușa B";
        anim.tracks = { swing(b) };
        doorB = base.addAnimation(anim);
        runtime = base;
    }

    double angle(InstanceId id) const
    {
        return model::transformValues(runtime.instance(id).transform, model::pivotLocal(runtime, id)).rotation.z;
    }

    void start(std::vector<PlayRequest> requests) { player.start(base, requests); }
    void run(double seconds)
    {
        // Pași de cadru, ca în aplicație.
        for (double t = 0.0; t < seconds - 1e-9; t += 1.0 / 60.0) {
            player.update(runtime, base, std::min(1.0 / 60.0, seconds - t));
        }
    }
};

PlayRequest req(AnimationId id, PlayDirection direction = PlayDirection::Forward, int repeat = 1, std::string action = "a")
{
    return { id, direction, repeat, std::move(action) };
}

} // namespace

TEST_CASE("Player: o dată înainte rulează o dată și rămâne deschisă", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA) });
    CHECK(s.player.active());
    s.run(0.5);
    CHECK(s.angle(s.a) == Approx(45.0).margin(1.0));
    s.run(1.0);
    CHECK(s.angle(s.a) == Approx(90.0).margin(1e-6));
    CHECK_FALSE(s.player.active());
    s.start({ req(s.doorA) }); // deja la capăt: nimic
    CHECK_FALSE(s.player.active());
    s.run(0.5);
    CHECK(s.angle(s.a) == Approx(90.0).margin(1e-6));
}

TEST_CASE("Player: înapoi o închide; la început nu face nimic", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA, PlayDirection::Reverse) });
    CHECK_FALSE(s.player.active());
    s.start({ req(s.doorA) });
    s.run(1.2);
    s.start({ req(s.doorA, PlayDirection::Reverse) });
    CHECK(s.player.active());
    s.run(1.2);
    CHECK(s.angle(s.a) == Approx(0.0).margin(1e-6));
    CHECK(s.player.position(s.doorA) == Approx(0.0));
}

TEST_CASE("Player: Comută deschide, apoi închide; în mișcare se întoarce", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA, PlayDirection::Toggle) });
    s.run(1.2);
    CHECK(s.player.position(s.doorA) == Approx(1.0));
    s.start({ req(s.doorA, PlayDirection::Toggle) });
    s.run(0.6);
    CHECK(s.player.position(s.doorA) == Approx(0.4).margin(0.02));
    s.start({ req(s.doorA, PlayDirection::Toggle) }); // în mișcare spre 0: se întoarce spre capăt
    s.run(0.2);
    CHECK(s.player.position(s.doorA) == Approx(0.6).margin(0.02));
    CHECK(s.player.playing(s.doorA));
}

TEST_CASE("Player: de N ori reia de la început", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA, PlayDirection::Forward, 3) });
    s.run(1.5);
    CHECK(s.player.position(s.doorA) == Approx(0.5).margin(0.02));
    CHECK(s.angle(s.a) == Approx(45.0).margin(2.0));
    s.run(1.6);
    CHECK_FALSE(s.player.active());
    CHECK(s.angle(s.a) == Approx(90.0).margin(1e-6));
}

TEST_CASE("Player: la infinit, același bloc o oprește pe loc", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA, PlayDirection::Forward, 0, "usa/a0") });
    s.run(10.3);
    CHECK(s.player.active());
    const double stopped = s.angle(s.a);
    s.start({ req(s.doorA, PlayDirection::Forward, 0, "usa/a0") });
    CHECK_FALSE(s.player.active());
    s.run(0.5);
    CHECK(s.angle(s.a) == Approx(stopped));
}

TEST_CASE("Player: Comută cu mai multe animații ia aceeași decizie", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA) });
    s.run(1.2); // A deschisă, B închisă
    s.start({ req(s.doorA, PlayDirection::Toggle, 1, "x/0"), req(s.doorB, PlayDirection::Toggle, 1, "x/1") });
    s.run(1.2);
    CHECK(s.angle(s.a) == Approx(90.0).margin(1e-6)); // A rămâne deschisă
    CHECK(s.angle(s.b) == Approx(90.0).margin(1e-6)); // B se deschide
    s.start({ req(s.doorA, PlayDirection::Toggle, 1, "x/0"), req(s.doorB, PlayDirection::Toggle, 1, "x/1") });
    s.run(1.2);
    CHECK(s.angle(s.a) == Approx(0.0).margin(1e-6));
    CHECK(s.angle(s.b) == Approx(0.0).margin(1e-6));
}

TEST_CASE("Player: animații în paralel, fără resetări", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA) });
    s.run(0.5);
    s.start({ req(s.doorB) });
    CHECK(s.player.position(s.doorA) == Approx(0.5).margin(0.02));
    s.run(0.6);
    CHECK(s.angle(s.a) == Approx(90.0).margin(1e-6));
    CHECK(s.player.position(s.doorB) == Approx(0.6).margin(0.02));
}

TEST_CASE("Player: ultima pornită câștigă", "[core][player]")
{
    Doors s;
    model::Animation other = s.base.animations().at(s.doorB);
    other.tracks = { swing(s.a) };
    other.tracks[0].channels[0].keys[1].value = -90.0;
    const AnimationId opposite = s.base.addAnimation(other);
    s.start({ req(s.doorA) });
    s.run(0.2);
    s.start({ req(opposite, PlayDirection::Forward, 1, "b") });
    s.run(0.3);
    CHECK(s.angle(s.a) < 0.0); // a doua (−90°) scrie ultima
}

TEST_CASE("Player: reset golește starea", "[core][player]")
{
    Doors s;
    s.start({ req(s.doorA) });
    s.run(0.5);
    s.player.reset();
    CHECK_FALSE(s.player.active());
    CHECK(s.player.position(s.doorA) == 0.0);
}

TEST_CASE("Player: animație ștearsă sau goală", "[core][player]")
{
    Doors s;
    model::Animation empty;
    empty.duration = 0.0;
    const AnimationId none = s.base.addAnimation(empty);
    s.start({ req(AnimationId { 999 }), req(none, PlayDirection::Toggle, 1, "b") });
    s.run(0.5);
    CHECK_FALSE(s.player.active());
    CHECK(s.angle(s.a) == Approx(0.0).margin(1e-6));
    CHECK_FALSE(s.player.camera(s.base));
    CHECK_FALSE(s.player.color(s.base, s.a));
}
