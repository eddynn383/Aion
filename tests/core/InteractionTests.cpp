#include "aion/core/geom/Operations.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/model/Interaction.h"
#include "aion/core/model/Operations.h"
#include "aion/core/view/ModelPicking.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using math::Transform;
using math::Vec3d;
using model::Behavior;
using model::Model;

namespace {

geom::FaceId faceAt(const geom::Mesh& mesh, Vec3d p)
{
    for (geom::FaceId f : mesh.faces().keys()) {
        if (geom::faceContainsPoint(mesh, f, p)) {
            return f;
        }
    }
    FAIL("nicio față în punctul dat");
    return {};
}

model::InstanceId box(Model& m, Vec3d o, Vec3d size, const std::string& name)
{
    geom::Mesh& root = m.mesh(m.root());
    geom::drawPolygon(root, { o, o + Vec3d { size.x, 0, 0 }, o + Vec3d { size.x, size.y, 0 }, o + Vec3d { 0, size.y, 0 } });
    geom::pushPull(root, faceAt(root, o + Vec3d { size.x / 2, size.y / 2, 0 }), -size.z);
    geom::Selection all;
    for (geom::FaceId f : root.faces().keys()) {
        all.faces.insert(f);
    }
    return model::makeGroup(m, m.root(), all, false, name);
}

bool near(Vec3d a, Vec3d b)
{
    return glm::length(a - b) < 1e-6;
}

void setBehavior(Model& m, model::InstanceId id, Behavior::Kind kind, Behavior::Anchor anchor, double amount)
{
    model::Instance inst = m.instance(id);
    inst.behavior.kind = kind;
    inst.behavior.anchor = anchor;
    inst.behavior.amount = amount;
    m.setInstance(id, inst);
}

} // namespace

TEST_CASE("Interacțiuni: ușa pe balama se deschide spre față și se închide la loc", "[core][model][interaction]")
{
    Model m;
    const auto door = box(m, { 1000, 0, 0 }, { 800, 18, 2000 }, "Ușă");
    setBehavior(m, door, Behavior::Kind::Rotate, Behavior::Anchor::Left, 90.0);
    const Transform closed = m.instance(door).transform;
    const Vec3d corner { 1800, 0, 0 }; // colțul opus balamalei (balamaua e la x = 1000)

    model::toggleBehavior(m, door);
    CHECK(m.instance(door).behavior.open);
    const auto [lo, hi] = model::bounds(m, m.instance(door).definition);
    const Vec3d far = m.instance(door).transform.applyToPoint({ hi.x, lo.y, 0.0 });
    CHECK(near(far, { 1000, -800, 0 })); // s-a rotit 90° spre −Y
    // La jumătate: 45°.
    const Vec3d half = model::transformAtOpenness(m, door, 0.5).applyToPoint({ hi.x, lo.y, 0.0 });
    CHECK(near(half, { 1000 + 800 * std::sqrt(0.5), -800 * std::sqrt(0.5), 0 }));

    model::toggleBehavior(m, door);
    CHECK_FALSE(m.instance(door).behavior.open);
    CHECK(near(m.instance(door).transform.applyToPoint(corner), closed.applyToPoint(corner)));
}

TEST_CASE("Interacțiuni: sertarul glisează; comportamentul se salvează în .aion", "[core][model][interaction]")
{
    Model m;
    const auto drawer = box(m, { 0, 0, 300 }, { 500, 450, 150 }, "Sertar");
    setBehavior(m, drawer, Behavior::Kind::Slide, Behavior::Anchor::Front, 400.0);
    model::toggleBehavior(m, drawer);
    CHECK(near(m.instance(drawer).transform.applyToPoint({ 0, 0, 0 }), { 0, -400, 0 }));

    const io::LoadResult loaded = io::readArchive(io::writeArchive(m, {}));
    REQUIRE(loaded.model);
    CHECK(loaded.model->sameAs(m));
    const Behavior& b = loaded.model->instance(drawer).behavior;
    CHECK(b.kind == Behavior::Kind::Slide);
    CHECK(b.open);
    CHECK(b.amount == 400.0);
}

TEST_CASE("Interacțiuni: click pe ușa dintr-un dulap găsește ușa (cea mai adâncă instanță interactivă)",
    "[core][view][interaction]")
{
    Model m;
    const auto door = box(m, { 0, 0, 0 }, { 600, 18, 700 }, "Ușă");
    setBehavior(m, door, Behavior::Kind::Rotate, Behavior::Anchor::Right, 100.0);
    box(m, { 0, 18, 0 }, { 600, 500, 700 }, "Corp");
    // Dulapul: un grup cu ușa și corpul.
    geom::Selection both;
    for (model::InstanceId id : m.childrenOf(m.root())) {
        both.instances.insert(id);
    }
    model::makeGroup(m, m.root(), both, false, "Dulap");

    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Front, { { 300, 0, 350 }, 0, 0, 3000 }));
    const auto screen = camera.project({ 300, 0, 350 });
    REQUIRE(screen);
    const auto hit = view::interactiveInstanceAt(m, camera, *screen);
    REQUIRE(hit);
    CHECK(m.instance(*hit).behavior.kind == Behavior::Kind::Rotate);

    // Pe lângă dulap: nimic.
    CHECK_FALSE(view::interactiveInstanceAt(m, camera, { 5, 5 }));
}
