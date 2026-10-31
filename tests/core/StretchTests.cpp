#include "aion/core/model/Carcass.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Parametric.h"
#include "aion/core/model/Stretch.h"
#include "aion/core/model/Transfer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// „Întinde” (pasul 11; docs/plans/mobilier-pe-corp.md, §9).

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

// Corpul din U7: 600 mm, PAL 18, 2 uși, construit din plăci (un „Corp la sol” transformat în corp obișnuit).
struct U7 {
    Model m;
    model::InstanceId body;
    model::DefinitionId def;
};

U7 makeU7()
{
    U7 u;
    u.body = model::createParametric(
        u.m, u.m.root(), "base", { { "doors", 2 }, { "shelves", 0 } }, Transform {});
    u.def = u.m.instance(u.body).definition;
    model::makePlainBody(u.m, u.def);
    return u;
}

model::InstanceId role(const Model& m, model::DefinitionId body, const std::string& key)
{
    for (model::InstanceId id : m.childrenOf(body)) {
        if (m.instance(id).roleKey == key) {
            return id;
        }
        for (model::InstanceId inner : m.childrenOf(m.instance(id).definition)) {
            if (m.instance(inner).roleKey == key) {
                return inner;
            }
        }
    }
    return {};
}

double param(const Model& m, model::InstanceId id, const char* key)
{
    return m.definition(m.instance(id).definition).parameters.at(key);
}

// Capetele piesei pe X, în coordonatele corpului (prin grupul ușii, dacă e în el).
std::pair<double, double> spanX(const Model& m, model::DefinitionId body, model::InstanceId id)
{
    Transform t = m.instance(id).transform;
    if (m.instance(id).parent != body) {
        for (model::InstanceId g : m.childrenOf(body)) {
            if (m.instance(g).definition == m.instance(id).parent) {
                t = m.instance(g).transform * t;
            }
        }
    }
    const auto [lo, hi] = model::bounds(m, m.instance(id).definition);
    const double a = t.applyToPoint(lo).x;
    const double b = t.applyToPoint(hi).x;
    return { std::min(a, b), std::max(a, b) };
}

void checkBoardsMatch(const Model& m, model::DefinitionId d)
{
    for (model::InstanceId id : m.childrenOf(d)) {
        const model::Definition& def = m.definition(m.instance(id).definition);
        if (def.part && def.part->kind == model::PartSpec::Kind::Board) {
            CHECK(model::boardState(m, id, m.instance(id).transform).state == model::BoardState::Matches);
        }
        checkBoardsMatch(m, m.instance(id).definition);
    }
}

} // namespace

TEST_CASE("Întinde: corpul din plăci de 600 devine 800 în 2 locuri egale", "[core][stretch]")
{
    U7 u = makeU7();
    const std::vector<double> planes = model::equalPlanes(u.m, u.body, 0, 2);
    REQUIRE(planes.size() == 2);
    const model::StretchResult r = model::stretch(u.m, u.body, 0, planes, 200);
    REQUIRE(r.ok);
    CHECK(u.m.validate().ok());
    CHECK(param(u.m, role(u.m, u.def, "bottom"), "length") == Approx(764.0));
    CHECK(param(u.m, role(u.m, u.def, "top"), "length") == Approx(764.0));
    CHECK(param(u.m, role(u.m, u.def, "back"), "width") == Approx(764.0));
    CHECK(param(u.m, role(u.m, u.def, "door.1.panel"), "width") == Approx(398.5));
    CHECK(param(u.m, role(u.m, u.def, "door.2.panel"), "width") == Approx(398.5));
    const auto right = spanX(u.m, u.def, role(u.m, u.def, "side.right"));
    CHECK(right.first == Approx(782.0));
    CHECK(right.second == Approx(800.0));
    const auto door2 = spanX(u.m, u.def, role(u.m, u.def, "door.2.panel"));
    CHECK(door2.first == Approx(401.5));
    CHECK(door2.second == Approx(800.0));
    checkBoardsMatch(u.m, u.def);
    // Feroneria doar se mută: balamalele ușii 1 (stânga) rămân, mânerul ușii 2 (stânga ei) se mută cu 100.
    const auto hinge = spanX(u.m, u.def, role(u.m, u.def, "door.1.hinge.1"));
    CHECK(hinge.first == Approx(20.0)); // după fața interioară a lateralei (18) + 2
}

TEST_CASE("Întinde: un plan prin grosimea unei plăci e refuzat, modelul neschimbat", "[core][stretch]")
{
    U7 u = makeU7();
    const Model before = u.m;
    const model::StretchResult r = model::stretch(u.m, u.body, 0, { 9.0 }, 200);
    CHECK_FALSE(r.ok);
    CHECK(r.reason == model::StretchRefusal::ThroughThickness);
    CHECK(u.m.sameAs(before));
}

TEST_CASE("Întinde: o placă modificată de mână e refuzată", "[core][stretch]")
{
    U7 u = makeU7();
    const model::InstanceId bottom = role(u.m, u.def, "bottom");
    model::Instance inst = u.m.instance(bottom);
    inst.transform = inst.transform * Transform::scaling(Vec3d { 0.0 }, { 1.0, 1.0, 1.5 });
    u.m.setInstance(bottom, inst);
    const Model before = u.m;
    const model::StretchResult r = model::stretch(u.m, u.body, 0, { 300.0 }, 200);
    CHECK_FALSE(r.ok);
    CHECK(r.reason == model::StretchRefusal::ModifiedBoard);
    CHECK(r.piece == bottom);
    CHECK(u.m.sameAs(before));
}

TEST_CASE("Întinde: copiile legate ale corpului se schimbă toate; o poliță legată cu alt corp devine unică",
    "[core][stretch]")
{
    U7 u = makeU7();
    for (int i = 1; i < 4; ++i) {
        model::placeLinked(u.m, u.def, u.m.root(), Transform::translation({ i * 1000.0, 0, 0 }));
    }
    // O poliță a corpului folosită și într-un alt corp.
    const model::InstanceId shelf
        = model::createBoard(u.m, u.def, "pal", 18, 564, 300, Transform::translation({ 18, 0, 300 }));
    const model::DefinitionId other = u.m.addDefinition({ "Alt corp", false });
    model::Instance shared = u.m.instance(shelf);
    shared.parent = other;
    const model::InstanceId sharedShelf = u.m.addInstance(shared);
    model::Instance otherPlaced;
    otherPlaced.parent = u.m.root();
    otherPlaced.definition = other;
    otherPlaced.transform = Transform::translation({ 0, 2000, 0 });
    u.m.addInstance(otherPlaced);

    REQUIRE(model::stretch(u.m, u.body, 0, { 300.0 }, 200).ok);
    CHECK(u.m.instancesOf(u.def).size() == 4);
    CHECK(param(u.m, role(u.m, u.def, "bottom"), "length") == Approx(764.0));
    CHECK(param(u.m, shelf, "length") == Approx(764.0));
    CHECK(param(u.m, sharedShelf, "length") == Approx(564.0)); // celălalt corp neschimbat
    CHECK(u.m.instance(shelf).definition != u.m.instance(sharedShelf).definition);
    CHECK(u.m.validate().ok());
}

TEST_CASE("Întinde: un corp din bibliotecă își schimbă câmpul", "[core][stretch]")
{
    Model m;
    const model::InstanceId body = model::createParametric(m, m.root(), "base", {}, Transform {});
    REQUIRE(model::stretch(m, body, 0, { 300.0 }, 200).ok);
    CHECK(m.definition(m.instance(body).definition).parameters.at("width") == Approx(800.0));
}
