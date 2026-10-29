#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Operations.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <vector>

using namespace aion;
using math::Transform;
using math::Vec3d;
using model::DefinitionId;
using model::InstanceId;
using model::Model;

namespace {

DefinitionId boxDefinition(Model& m, const char* name, Vec3d size)
{
    const DefinitionId d = m.addDefinition(model::Definition { name, false });
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, size, 24);
    return d;
}

DefinitionId groupDefinition(Model& m, const char* name)
{
    model::Definition d { name, true };
    d.collection = true;
    return m.addDefinition(d);
}

InstanceId place(Model& m, DefinitionId parent, DefinitionId definition, Vec3d at)
{
    model::Instance inst;
    inst.parent = parent;
    inst.definition = definition;
    inst.transform = Transform::translation(at);
    return m.addInstance(inst);
}

// Structura din Mobila.aion: grupul „Usi” (două uși care împart obiectul „Ușă”) și grupul „Corp” sunt folosite de
// ambele corpuri (Corp A, Corp B), fiecare cu definiția lui proprie.
struct Mobila {
    Model m;
    DefinitionId door, board, usi, corp, a, b;
    InstanceId doorL, doorR, boardIn, corpInA, usiA, corpInB, usiB, corpA, corpB;

    Mobila()
    {
        door = boxDefinition(m, "Ușă", { 300, 20, 700 });
        board = boxDefinition(m, "Placă", { 600, 400, 20 });
        usi = groupDefinition(m, "Usi");
        doorL = place(m, usi, door, { -150, 0, 0 });
        doorR = place(m, usi, door, { 150, 0, 0 });
        corp = groupDefinition(m, "Corp");
        boardIn = place(m, corp, board, { 0, 0, 0 });
        a = groupDefinition(m, "Corp A");
        corpInA = place(m, a, corp, { 0, 0, 0 });
        usiA = place(m, a, usi, { 0, -210, 0 });
        b = groupDefinition(m, "Corp B");
        corpInB = place(m, b, corp, { 0, 0, 0 });
        usiB = place(m, b, usi, { 0, -210, 0 });
        corpA = place(m, m.root(), a, { 0, 0, 0 });
        corpB = place(m, m.root(), b, { 1000, 0, 0 });
    }
};

void collect(const Model& m, InstanceId id, std::set<InstanceId>& instances, std::set<DefinitionId>& definitions)
{
    instances.insert(id);
    const DefinitionId d = m.instance(id).definition;
    definitions.insert(d);
    for (InstanceId c : m.childrenOf(d)) {
        collect(m, c, instances, definitions);
    }
}

model::Rule clickRule(std::vector<model::Action> actions)
{
    model::Rule r;
    r.trigger = model::Trigger::Click;
    r.actions = std::move(actions);
    return r;
}

model::Action moveTarget(InstanceId target)
{
    model::Action a;
    a.kind = model::ActionKind::MoveBy;
    a.target = target;
    a.vector = { 0, 0, 100 };
    return a;
}

} // namespace

TEST_CASE("copyDeep: copia nu împarte nicio definiție cu originalul", "[core][copying]")
{
    Mobila s;
    const Transform delta = Transform::translation({ 0, 5000, 0 });
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, delta);
    REQUIRE(r.roots.size() == 1);
    std::set<InstanceId> copyInstances;
    std::set<DefinitionId> copyDefinitions;
    collect(s.m, r.roots[0], copyInstances, copyDefinitions);
    for (DefinitionId d : copyDefinitions) {
        for (InstanceId i : s.m.instancesOf(d)) {
            CHECK(copyInstances.contains(i));
        }
    }
    std::set<InstanceId> originalInstances;
    std::set<DefinitionId> originalDefinitions;
    collect(s.m, s.corpA, originalInstances, originalDefinitions);
    CHECK(r.instances.size() == originalInstances.size());
    CHECK(copyInstances.size() == originalInstances.size());
    CHECK(r.instances.at(s.corpA) == r.roots[0]);
    CHECK(s.m.instance(r.roots[0]).transform == delta * s.m.instance(s.corpA).transform);
    CHECK(s.m.instance(r.roots[0]).parent == s.m.root());
}

TEST_CASE("copyDeep: legăturile din interior rămân", "[core][copying]")
{
    Mobila s;
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, Transform::translation({ 0, 5000, 0 }));
    const InstanceId left = r.instances.at(s.doorL);
    const InstanceId right = r.instances.at(s.doorR);
    CHECK(s.m.instance(left).definition == s.m.instance(right).definition);
    CHECK(s.m.instance(left).definition != s.door);
    CHECK(s.m.instance(r.instances.at(s.usiA)).definition != s.usi);
}

TEST_CASE("copyDeep: regulile vin cu copiile, cu țintele mutate", "[core][copying]")
{
    Mobila s;
    const DefinitionId cube = boxDefinition(s.m, "Cub", { 100, 100, 100 });
    const InstanceId x = place(s.m, s.m.root(), cube, { 0, 3000, 0 });
    const InstanceId y = place(s.m, s.m.root(), cube, { 500, 3000, 0 });
    const InstanceId z = place(s.m, s.m.root(), cube, { 1000, 3000, 0 });
    model::Instance xi = s.m.instance(x);
    xi.rules.push_back(clickRule({ moveTarget(y), moveTarget(z) }));
    s.m.setInstance(x, xi);

    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { x, y }, Transform::translation({ 0, 1000, 0 }));
    const model::Instance& copy = s.m.instance(r.instances.at(x));
    REQUIRE(copy.rules.size() == 1);
    REQUIRE(copy.rules[0].actions.size() == 2);
    CHECK(copy.rules[0].actions[0].target == r.instances.at(y));
    CHECK(copy.rules[0].actions[1].target == z);
    CHECK(s.m.instance(x).rules[0].actions[0].target == y); // originalul rămâne
}

TEST_CASE("copyDeep: sursa poate fi chiar ținta", "[core][copying]")
{
    Mobila s;
    const Model snapshot = s.m;
    Model other = s.m;
    const Transform delta = Transform::translation({ 0, 5000, 0 });
    (void)model::copyDeep(s.m, s.m.root(), s.m, { s.corpA, s.corpB }, delta);
    (void)model::copyDeep(other, other.root(), snapshot, { s.corpA, s.corpB }, delta);
    CHECK(s.m.sameAs(other));
}

TEST_CASE("copyDeep: dintr-o copie a modelului, după ștergerea originalului", "[core][copying]")
{
    Mobila s;
    model::Instance usiA = s.m.instance(s.usiA);
    usiA.rules.push_back(clickRule({ moveTarget(s.doorL) }));
    s.m.setInstance(s.usiA, usiA);
    const Model snapshot = s.m;
    model::eraseInstance(s.m, s.corpA);
    REQUIRE_FALSE(s.m.definitions().contains(s.a));

    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), snapshot, { s.corpA }, Transform {});
    REQUIRE(r.roots.size() == 1);
    const auto [lo, hi] = model::bounds(s.m, s.m.instance(r.roots[0]).definition);
    const auto [olo, ohi] = model::bounds(snapshot, s.a);
    CHECK(lo == olo);
    CHECK(hi == ohi);
    const model::Instance& usi = s.m.instance(r.instances.at(s.usiA));
    REQUIRE(usi.rules.size() == 1);
    CHECK(usi.rules[0].actions[0].target == r.instances.at(s.doorL));
}

TEST_CASE("copyLinked: grupuri și colecții rămân legate", "[core][copying]")
{
    Mobila s;
    const InstanceId copy = model::copyLinked(s.m, s.corpA, Transform::translation({ 0, 5000, 0 }));
    CHECK(s.m.instance(copy).definition == s.m.instance(s.corpA).definition);
    CHECK(s.m.definition(s.a).collection); // o colecție rămâne colecție

    const DefinitionId solo = s.m.addDefinition(model::Definition { "Grup", true });
    geom::buildPrimitive(s.m.mesh(solo), geom::Primitive::Box, { 100, 100, 100 }, 24);
    const InstanceId g = place(s.m, s.m.root(), solo, { 0, 8000, 0 });
    const InstanceId linked = model::copyLinked(s.m, g, Transform::translation({ 500, 0, 0 }));
    CHECK(s.m.instance(linked).definition == solo);
    CHECK_FALSE(s.m.definition(solo).group); // un grup cu copii legate devine obiect cu copii
}

TEST_CASE("makeUniqueDeep: separă ușile lui Corp B", "[core][copying]")
{
    Mobila s;
    CHECK(model::canMakeUnique(s.m, s.corpB));
    CHECK(model::canMakeUnique(s.m, s.usiB));
    const model::CopyResult r = model::makeUniqueDeep(s.m, s.corpB);
    REQUIRE(r.roots == std::vector<InstanceId> { s.corpB });
    CHECK(r.instances.at(s.corpB) == s.corpB);
    CHECK(s.m.instance(s.corpB).definition == s.b); // definiția proprie rămâne
    CHECK(s.m.instance(s.usiB).definition != s.usi); // instanțele directe își păstrează id-urile
    CHECK(s.m.instance(s.corpInB).definition != s.corp);
    CHECK(s.m.instance(s.usiA).definition == s.usi); // Corp A neschimbat
    CHECK(s.m.instance(s.doorL).parent == s.usi);
    const InstanceId newDoor = r.instances.at(s.doorL);
    CHECK(newDoor != s.doorL);
    CHECK(s.m.instance(newDoor).parent == s.m.instance(s.usiB).definition);
    CHECK_FALSE(model::canMakeUnique(s.m, s.corpB));
    CHECK_FALSE(model::canMakeUnique(s.m, s.corpA));
}

TEST_CASE("makeUniqueDeep: undo readuce totul", "[core][copying]")
{
    Mobila s;
    commands::CommandStack stack { s.m };
    const Model before = s.m;
    REQUIRE(stack.run("Fă unic", [&](Model& m) { (void)model::makeUniqueDeep(m, s.corpB); }));
    CHECK_FALSE(s.m.sameAs(before));
    stack.undo();
    CHECK(s.m.sameAs(before));
}

TEST_CASE("instanceOutline: muchiile în coordonatele părintelui", "[core][copying]")
{
    Mobila s;
    const auto edges = model::instanceOutline(s.m, { s.corpB });
    // Placa (12 muchii) și cele două uși (câte 12), toate mutate cu Corp B (x + 1000).
    REQUIRE(edges.size() == 36);
    double minX = 1e9;
    for (const auto& [p, q] : edges) {
        minX = std::min({ minX, p.x, q.x });
    }
    CHECK(minX > 600.0); // cel mai din stânga: ușa stângă, la 1000 − 150 − 150
}
