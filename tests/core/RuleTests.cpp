#include "aion/core/geom/Operations.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Rules.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aion;
using Catch::Approx;
using model::Action;
using model::ActionKind;
using model::Rule;
using model::Trigger;

namespace {

model::InstanceId makeObject(model::Model& m)
{
    geom::Mesh& root = m.mesh(m.root());
    geom::drawPolygon(root, { { 0, 0, 0 }, { 100, 0, 0 }, { 100, 100, 0 }, { 0, 100, 0 } });
    geom::Selection s;
    s.faces.insert(root.faces().keys().front());
    return model::makeGroup(m, m.root(), s, false, "Ușă");
}

void setRules(model::Model& m, model::InstanceId id, std::vector<Rule> rules)
{
    model::Instance inst = m.instance(id);
    inst.rules = std::move(rules);
    m.setInstance(id, inst);
}

double x(const model::Model& m, model::InstanceId id)
{
    return m.instance(id).transform.applyToPoint({ 0, 0, 0 }).x;
}

} // namespace

TEST_CASE("Interacțiuni: o mutare animată pornește la click și se termină în timp", "[core][rules]")
{
    model::Model m;
    const auto door = makeObject(m);
    Action move;
    move.kind = ActionKind::MoveBy;
    move.vector = { 1000, 0, 0 };
    move.duration = 1.0;
    setRules(m, door, { Rule { Trigger::Click, "", { move } } });

    model::RuleRuntime runtime;
    CHECK_FALSE(runtime.fire(m, door, Trigger::RightClick)); // alt eveniment: nimic
    REQUIRE(runtime.fire(m, door, Trigger::Click));
    runtime.update(m, 0.5);
    CHECK(x(m, door) == Approx(500.0)); // ease-in-out simetric
    CHECK_FALSE(runtime.update(m, 0.6));
    CHECK(x(m, door) == Approx(1000.0));
}

TEST_CASE("Interacțiuni: înlănțuire, așteptare, alternare, cereri către aplicație", "[core][rules]")
{
    model::Model m;
    const auto lamp = makeObject(m);
    Action wait;
    wait.kind = ActionKind::Wait;
    wait.duration = 1.0;
    Action hide;
    hide.kind = ActionKind::Hide;
    Action show;
    show.kind = ActionKind::Show;
    Action toggle;
    toggle.kind = ActionKind::Alternate;
    toggle.first = { hide };
    toggle.second = { show };
    Action play;
    play.kind = ActionKind::PlayAnimation;
    play.animation = model::AnimationId { 4 };
    setRules(m, lamp, { Rule { Trigger::Click, "", { toggle } }, Rule { Trigger::Key, "Space", { wait, hide, play } } });

    model::RuleRuntime runtime;
    runtime.fire(m, lamp, Trigger::Click);
    CHECK(m.instance(lamp).hidden); // prima dată: ascunde (instant)
    runtime.fire(m, lamp, Trigger::Click);
    CHECK_FALSE(m.instance(lamp).hidden); // a doua oară: arată

    CHECK_FALSE(runtime.fire(m, lamp, Trigger::Key, "A"));
    REQUIRE(runtime.fire(m, lamp, Trigger::Key, "Space"));
    runtime.update(m, 0.5);
    CHECK_FALSE(m.instance(lamp).hidden); // încă în așteptare
    runtime.update(m, 0.6);
    CHECK(m.instance(lamp).hidden);
    const auto requests = runtime.takeRequests();
    REQUIRE(requests.size() == 1);
    CHECK(requests.front().playAnimation == model::AnimationId { 4 });

    // Regulile se salvează.
    const io::LoadResult loaded = io::readArchive(io::writeArchive(m, {}));
    REQUIRE(loaded.model);
    CHECK(loaded.model->sameAs(m));
}

TEST_CASE("Reguli: „Mută la” din fișierele vechi (colțul minim) se migrează la poziția pivotului", "[core][model][rules][io]")
{
    model::Model m;
    model::Definition def { "Cutie", true };
    const model::DefinitionId d = m.addDefinition(def);
    geom::Mesh& mesh = m.mesh(d);
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    geom::pushPull(mesh, mesh.faces().keys().front(), -1000);
    model::Instance inst;
    inst.parent = m.root();
    inst.definition = d;
    model::Rule rule;
    model::Action move;
    move.kind = model::ActionKind::MoveTo;
    move.vector = { 3000, 0, 0 }; // în formatul vechi: unde ajunge colțul minim
    rule.actions.push_back(move);
    inst.rules.push_back(rule);
    const model::InstanceId id = m.addInstance(inst);
    std::string text = io::toJson(m);
    const std::string version = "\"formatVersion\": " + std::to_string(io::kFormatVersion);
    const auto at = text.find(version);
    REQUIRE(at != std::string::npos);
    text.replace(at, version.size(), "\"formatVersion\": 2");
    const io::LoadResult loaded = io::fromJson(text);
    REQUIRE(loaded.model);
    // Acum ținta e a pivotului (centrul): colțul tot la (3000, 0, 0) → pivotul la (3500, 500, 500).
    const model::Action& migrated = loaded.model->instance(id).rules.front().actions.front();
    CHECK(glm::length(migrated.vector - math::Vec3d { 3500, 500, 500 }) < 1e-6);
    // Un fișier nou rămâne cum e.
    const io::LoadResult same = io::fromJson(io::toJson(m));
    CHECK(glm::length(same.model->instance(id).rules.front().actions.front().vector - math::Vec3d { 3000, 0, 0 }) < 1e-6);
}
