#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/SnapSources.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

model::DefinitionId boxDefinition(Model& m, Vec3d size)
{
    model::Definition def { "Cutie", true };
    def.shape = "box";
    const model::DefinitionId d = m.addDefinition(def);
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, size, 24);
    return d;
}

model::InstanceId place(Model& m, model::DefinitionId parent, model::DefinitionId def, const Transform& t)
{
    model::Instance inst;
    inst.parent = parent;
    inst.definition = def;
    inst.transform = t;
    return m.addInstance(inst);
}

model::DefinitionId groupDefinition(Model& m)
{
    return m.addDefinition(model::Definition { "Grup", false });
}

} // namespace

TEST_CASE("snapScene: o cutie în rădăcină dă o sursă cu transformarea instanței", "[core][inference][snapscene]")
{
    Model m;
    const model::DefinitionId d = boxDefinition(m, { 400, 200, 300 });
    const model::InstanceId id = place(m, m.root(), d, Transform::translation({ 3000, 0, 0 }));
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    REQUIRE(scene.sources.size() == 1);
    CHECK(scene.sources[0].instance == id);
    CHECK(scene.sources[0].mesh == &m.mesh(d));
    CHECK(glm::length(scene.sources[0].toContext.applyToPoint({ 400, 200, 300 }) - Vec3d { 3400, 200, 300 }) < 1e-6);
}

TEST_CASE("snapScene: imbricat și oglindit", "[core][inference][snapscene]")
{
    Model m;
    const model::DefinitionId box = boxDefinition(m, { 400, 200, 300 });
    const model::DefinitionId grp = groupDefinition(m);
    const model::InstanceId g = place(m, m.root(), grp, Transform::translation({ 1000, 0, 0 }));
    place(m, grp, box, Transform::scaling({ 0, 0, 0 }, { -1, 1, 1 }));
    const inference::SnapScene scene = model::snapScene(m, {}, {});
    REQUIRE(scene.sources.size() == 1);
    CHECK(scene.sources[0].instance == g);
    CHECK(scene.sources[0].mesh == &m.mesh(box));
    CHECK(glm::length(scene.sources[0].toContext.applyToPoint({ 0, 0, 0 }) - Vec3d { 1000, 0, 0 }) < 1e-6);
    CHECK(glm::determinant(glm::dmat3(scene.sources[0].toContext.matrix())) < 0.0);
}

TEST_CASE("snapScene: în contextul unui grup", "[core][inference][snapscene]")
{
    Model m;
    const model::DefinitionId box = boxDefinition(m, { 400, 200, 300 });
    const model::DefinitionId grp = groupDefinition(m);
    geom::drawLine(m.mesh(m.root()), { 0, 0, 0 }, { 100, 0, 0 });
    const Transform tI = Transform::translation({ 500, 0, 0 });
    const model::InstanceId i = place(m, m.root(), grp, tI);
    place(m, grp, box, Transform::translation({ 7, 0, 0 }));
    const Transform tJ = Transform::translation({ 0, 2000, 0 });
    const model::InstanceId j = place(m, m.root(), box, tJ);

    const inference::SnapScene scene = model::snapScene(m, { i }, {});
    bool foundJ = false;
    bool foundRoot = false;
    for (const inference::SnapSource& s : scene.sources) {
        CHECK(s.mesh != &m.mesh(grp));
        CHECK(s.instance != i);
        if (s.instance == j) {
            foundJ = true;
            CHECK(glm::length(s.toContext.applyToPoint({ 0, 0, 0 }) - Vec3d { -500, 2000, 0 }) < 1e-6);
        }
        if (s.mesh == &m.mesh(m.root())) {
            foundRoot = true;
            CHECK_FALSE(s.instance);
        }
    }
    CHECK(foundJ);
    CHECK(foundRoot);
    // Instanța din interiorul lui I (copil al contextului editat) are ea însăși ca instanță de sus.
    CHECK(scene.sources.size() == 3);
}

TEST_CASE("snapScene: ascunse și excluse", "[core][inference][snapscene]")
{
    Model m;
    const model::DefinitionId box = boxDefinition(m, { 400, 200, 300 });
    const model::DefinitionId grp = groupDefinition(m);
    const model::InstanceId visible = place(m, m.root(), box, Transform());
    const model::InstanceId hid = place(m, m.root(), box, Transform());
    m.setInstance(hid, [&] { auto x = m.instance(hid); x.hidden = true; return x; }());
    const model::TagId tag = m.addTag(model::Tag { "T", {}, false });
    const model::InstanceId tagged = place(m, m.root(), box, Transform());
    m.setInstance(tagged, [&] { auto x = m.instance(tagged); x.tag = tag; return x; }());
    const model::InstanceId excl = place(m, m.root(), grp, Transform());
    place(m, grp, box, Transform());

    const inference::SnapScene scene = model::snapScene(m, {}, { excl });
    REQUIRE(scene.sources.size() == 1);
    CHECK(scene.sources[0].instance == visible);
}
