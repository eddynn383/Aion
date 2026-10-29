#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/AnimationEval.h"
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

namespace {

model::Channel channel(model::Property p, std::vector<std::pair<double, double>> keys)
{
    model::Channel c;
    c.property = p;
    for (const auto& [t, v] : keys) {
        c.keys.push_back({ t, v, {}, {} });
    }
    return c;
}

// O pistă de rotație (0 → 90°) pe obiectul dat.
model::ObjectTrack swing(InstanceId instance)
{
    return { instance, { channel(model::Property::RotationZ, { { 0.0, 0.0 }, { 1.0, 90.0 } }) } };
}

model::Action play(model::AnimationId animation)
{
    model::Action a;
    a.kind = model::ActionKind::PlayAnimation;
    a.animation = animation;
    return a;
}

void addRule(Model& m, InstanceId id, model::Rule rule)
{
    model::Instance inst = m.instance(id);
    inst.rules.push_back(std::move(rule));
    m.setInstance(id, inst);
}

const model::Animation* animationNamed(const Model& m, const std::string& name)
{
    for (const model::Animation& a : m.animations().values()) {
        if (a.name == name) {
            return &a;
        }
    }
    return nullptr;
}

model::AnimationId idOf(const Model& m, const std::string& name)
{
    for (std::size_t i = 0; i < m.animations().size(); ++i) {
        if (m.animations().values()[i].name == name) {
            return m.animations().keys()[i];
        }
    }
    return {};
}

// Mobila cu „Animația 1” (ușile, plus un obiect X din afară), pornită de o regulă pe Usi lui A.
struct Animated : Mobila {
    InstanceId x;
    model::AnimationId doors;

    Animated()
    {
        x = place(m, m.root(), door, { 0, 4000, 0 });
        model::Animation a;
        a.name = "Animația 1";
        a.duration = 1.0;
        a.tracks = { swing(doorL), swing(doorR), swing(x) };
        doors = m.addAnimation(a);
        addRule(m, usiA, clickRule({ play(doors) }));
    }
};

} // namespace

TEST_CASE("copyAnimations: animația pornită de regulă se duplică", "[core][copying]")
{
    Animated s;
    const model::Animation original = s.m.animations().at(s.doors);
    const Transform delta = Transform::translation({ 0, 5000, 0 });
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, delta);
    model::copyAnimations(s.m, s.m, r, delta, model::CopyKind::Copy);

    const model::Animation* copy = animationNamed(s.m, "Animația 1 (copie)");
    REQUIRE(copy != nullptr);
    REQUIRE(copy->tracks.size() == 2);
    std::set<InstanceId> animated;
    for (const model::ObjectTrack& t : copy->tracks) {
        animated.insert(t.instance);
    }
    CHECK(animated == std::set<InstanceId> { r.instances.at(s.doorL), r.instances.at(s.doorR) });
    CHECK(copy->duration == 1.0);
    const model::Instance& usiCopy = s.m.instance(r.instances.at(s.usiA));
    REQUIRE(usiCopy.rules.size() == 1);
    CHECK(usiCopy.rules[0].actions[0].animation == idOf(s.m, "Animația 1 (copie)"));
    CHECK(s.m.instance(s.usiA).rules[0].actions[0].animation == s.doors);
    CHECK(s.m.animations().at(s.doors) == original);
}

TEST_CASE("copyAnimations: numele nu se repetă", "[core][copying]")
{
    Animated s;
    for (int k = 1; k <= 2; ++k) {
        const Transform delta = Transform::translation({ 0, 5000.0 * k, 0 });
        const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, delta);
        model::copyAnimations(s.m, s.m, r, delta, model::CopyKind::Copy);
    }
    CHECK(animationNamed(s.m, "Animația 1 (copie)") != nullptr);
    CHECK(animationNamed(s.m, "Animația 1 (copie 2)") != nullptr);
    CHECK(s.m.animations().size() == 3);
}

TEST_CASE("copyAnimations: filmul din timeline primește pistele copiei", "[core][copying]")
{
    Mobila s;
    model::Animation film;
    film.name = "Animația 2";
    film.tracks = { swing(s.doorL) };
    const model::AnimationId id = s.m.addAnimation(film);
    const Transform delta = Transform::translation({ 0, 5000, 0 });
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, delta);
    model::copyAnimations(s.m, s.m, r, delta, model::CopyKind::Copy);
    CHECK(s.m.animations().size() == 1);
    const model::Animation& after = s.m.animations().at(id);
    REQUIRE(after.tracks.size() == 2);
    CHECK(after.tracks[1].instance == r.instances.at(s.doorL));
    CHECK(after.tracks[1].channels == after.tracks[0].channels);
}

TEST_CASE("copyAnimations: pista obiectului de sus se mută cu copia", "[core][copying]")
{
    Animated s;
    model::Animation a = s.m.animations().at(s.doors);
    a.tracks.push_back({ s.corpA, { channel(model::Property::PositionX, { { 0.0, 0.0 }, { 1.0, 1000.0 } }) } });
    s.m.setAnimation(s.doors, a);
    const Transform delta = Transform::translation({ 5000, 0, 0 });
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, delta);
    model::copyAnimations(s.m, s.m, r, delta, model::CopyKind::Copy);

    const model::Animation* copy = animationNamed(s.m, "Animația 1 (copie)");
    REQUIRE(copy != nullptr);
    const model::ObjectTrack* top = model::findTrack(*copy, r.roots[0]);
    REQUIRE(top != nullptr);
    REQUIRE(top->channels[0].keys.size() == 2);
    CHECK(top->channels[0].keys[0].value == 5000.0);
    CHECK(top->channels[0].keys[1].value == 6000.0);
    const model::ObjectTrack* door = model::findTrack(*copy, r.instances.at(s.doorL));
    REQUIRE(door != nullptr);
    CHECK(door->channels == swing(s.doorL).channels);
}

TEST_CASE("copyAnimations: căile și camera nu se copiază", "[core][copying]")
{
    Animated s;
    model::Animation a = s.m.animations().at(s.doors);
    a.camera.push_back(model::CameraKey {});
    s.m.setAnimation(s.doors, a);
    model::MotionPath path;
    path.animation = s.doors;
    path.curve.points = { { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, false }, { { 1000, 0, 0 }, { 1000, 0, 0 }, { 1000, 0, 0 }, false } };
    s.m.addMotionPath(path);
    const Transform delta = Transform::translation({ 0, 5000, 0 });
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, delta);
    model::copyAnimations(s.m, s.m, r, delta, model::CopyKind::Copy);
    const model::AnimationId copy = idOf(s.m, "Animația 1 (copie)");
    REQUIRE(copy);
    CHECK(s.m.animations().at(copy).camera.empty());
    CHECK(s.m.motionPaths().size() == 1);
    CHECK(s.m.motionPaths().values()[0].animation == s.doors);
}

TEST_CASE("Fă unic: regulile din afară", "[core][copying]")
{
    Animated s;
    addRule(s.m, s.usiB, clickRule({ play(s.doors) })); // în definiția proprie a lui B
    addRule(s.m, s.corpB, clickRule({ play(s.doors) })); // obiectul făcut unic
    const DefinitionId cube = boxDefinition(s.m, "Întrerupător", { 50, 50, 50 });
    const InstanceId toggle = place(s.m, s.m.root(), cube, { 0, -2000, 0 });
    model::Action switchAction = play(s.doors);
    switchAction.duration = 0.25; // „aceleași setări”: blocul în plus le păstrează
    addRule(s.m, toggle, clickRule({ switchAction }));

    const model::CopyResult r = model::makeUniqueDeep(s.m, s.corpB);
    model::copyAnimations(s.m, s.m, r, Transform {}, model::CopyKind::Unique);
    const model::AnimationId copy = idOf(s.m, "Animația 1 (copie)");
    REQUIRE(copy);
    CHECK(s.m.animations().at(copy).tracks.size() == 2); // ușile lui B
    CHECK(s.m.animations().at(s.doors).tracks.size() == 3); // ușile lui A și X, neschimbate
    CHECK(s.m.instance(s.usiA).rules[0].actions[0].animation == s.doors);
    CHECK(s.m.instance(s.usiB).rules[0].actions[0].animation == copy);
    CHECK(s.m.instance(s.corpB).rules[0].actions[0].animation == copy);
    const std::vector<model::Action>& both = s.m.instance(toggle).rules[0].actions;
    REQUIRE(both.size() == 2);
    CHECK(both[0].animation == s.doors);
    CHECK(both[1].animation == copy);
    CHECK(both[1].duration == 0.25);
}

TEST_CASE("copyAnimations: regulă cu animație ștearsă", "[core][copying]")
{
    Mobila s;
    addRule(s.m, s.usiA, clickRule({ play(model::AnimationId { 999 }) }));
    const model::CopyResult r = model::copyDeep(s.m, s.m.root(), s.m, { s.corpA }, Transform {});
    model::copyAnimations(s.m, s.m, r, Transform {}, model::CopyKind::Copy);
    CHECK(s.m.animations().empty());
    CHECK(s.m.instance(r.instances.at(s.usiA)).rules[0].actions[0].animation == model::AnimationId { 999 });
}

TEST_CASE("Fă unic: regula de pe un obiect păstrat din interior duplică animația", "[core][copying]")
{
    // Ca în Mobila.aion: regula stă pe Usi lui B (o instanță din definiția proprie a lui B, care nu se copiază).
    Animated s;
    addRule(s.m, s.usiB, clickRule({ play(s.doors) }));
    const model::CopyResult r = model::makeUniqueDeep(s.m, s.corpB);
    model::copyAnimations(s.m, s.m, r, Transform {}, model::CopyKind::Unique);
    const model::AnimationId copy = idOf(s.m, "Animația 1 (copie)");
    REQUIRE(copy);
    CHECK(s.m.animations().at(s.doors).tracks.size() == 3); // ușile lui A și X: fără ușile lui B
    CHECK(s.m.animations().at(copy).tracks.size() == 2);
    CHECK(s.m.instance(s.usiB).rules[0].actions[0].animation == copy);
    CHECK(s.m.instance(s.usiA).rules[0].actions[0].animation == s.doors);
}

TEST_CASE("Fă unic: animația doar a obiectului însuși nu se dublează", "[core][copying]")
{
    Mobila s;
    model::Animation slide;
    slide.name = "Slide B";
    slide.duration = 1.0;
    slide.tracks = { { s.corpB, { channel(model::Property::PositionX, { { 0.0, 1000.0 }, { 1.0, 1500.0 } }) } } };
    const model::AnimationId id = s.m.addAnimation(slide);
    addRule(s.m, s.corpB, clickRule({ play(id) }));
    const model::CopyResult r = model::makeUniqueDeep(s.m, s.corpB);
    model::copyAnimations(s.m, s.m, r, Transform {}, model::CopyKind::Unique);
    CHECK(s.m.animations().size() == 1);
    CHECK(s.m.animations().at(id).tracks.size() == 1);
    CHECK(s.m.instance(s.corpB).rules[0].actions[0].animation == id);
}

TEST_CASE("Fă unic: pistele obiectelor păstrate din interior trec în copie", "[core][copying]")
{
    // Câte un sertar propriu în A și în B (instanțe păstrate la Fă unic) și ușile comune, în aceeași animație.
    Mobila s;
    const InstanceId drawerA = place(s.m, s.a, s.board, { 0, 0, 300 });
    const InstanceId drawerB = place(s.m, s.b, s.board, { 0, 0, 300 });
    model::Animation open;
    open.name = "Deschide";
    open.duration = 1.0;
    open.tracks = { swing(drawerA), swing(drawerB), swing(s.doorL), swing(s.doorR) };
    const model::AnimationId id = s.m.addAnimation(open);
    addRule(s.m, s.usiA, clickRule({ play(id) }));
    addRule(s.m, s.usiB, clickRule({ play(id) }));
    const model::CopyResult r = model::makeUniqueDeep(s.m, s.corpB);
    model::copyAnimations(s.m, s.m, r, Transform {}, model::CopyKind::Unique);
    const model::AnimationId copy = idOf(s.m, "Deschide (copie)");
    REQUIRE(copy);
    const auto animatedBy = [&](model::AnimationId a) {
        std::set<InstanceId> out;
        for (const model::ObjectTrack& t : s.m.animations().at(a).tracks) {
            out.insert(t.instance);
        }
        return out;
    };
    CHECK(animatedBy(id) == std::set<InstanceId> { drawerA, s.doorL, s.doorR });
    CHECK(animatedBy(copy) == std::set<InstanceId> { drawerB, r.instances.at(s.doorL), r.instances.at(s.doorR) });
    CHECK(s.m.instance(s.usiB).rules[0].actions[0].animation == copy);
    CHECK(s.m.instance(s.usiA).rules[0].actions[0].animation == id);
}

TEST_CASE("Fă unic: mânerul din definiția comună rămâne pe original", "[core][copying]")
{
    // Un mâner în grupul comun „Usi”, frate cu ușile, care pornește animația ușilor.
    Animated s;
    const DefinitionId knob = boxDefinition(s.m, "Mâner", { 20, 20, 20 });
    const InstanceId handle = place(s.m, s.usi, knob, { 0, -30, 400 });
    addRule(s.m, handle, clickRule({ play(s.doors) }));
    const model::CopyResult r = model::makeUniqueDeep(s.m, s.corpB);
    model::copyAnimations(s.m, s.m, r, Transform {}, model::CopyKind::Unique);
    const model::AnimationId copy = idOf(s.m, "Animația 1 (copie)");
    REQUIRE(copy);
    // Mânerul lui A (în definiția originală) pornește doar ușile lui A; copia lui (la B) doar ușile lui B.
    REQUIRE(s.m.instance(handle).rules[0].actions.size() == 1);
    CHECK(s.m.instance(handle).rules[0].actions[0].animation == s.doors);
    const InstanceId handleB = r.instances.at(handle);
    REQUIRE(s.m.instance(handleB).rules[0].actions.size() == 1);
    CHECK(s.m.instance(handleB).rules[0].actions[0].animation == copy);
}

TEST_CASE("canMakeUnique cu folosirile numărate o dată dă același rezultat", "[core][copying]")
{
    Mobila s;
    const auto check = [&] {
        const model::DefinitionUses uses = model::definitionUses(s.m);
        for (InstanceId id : s.m.instances().keys()) {
            INFO(id.value);
            CHECK(model::canMakeUnique(s.m, id, uses) == model::canMakeUnique(s.m, id));
        }
        CHECK(uses.at(s.usi.value) == s.m.instancesOf(s.usi).size());
    };
    check();
    (void)model::makeUniqueDeep(s.m, s.corpB);
    check();
}
