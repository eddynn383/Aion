#include "aion/core/commands/CommandStack.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Parametric.h"
#include "aion/core/model/Transfer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace aion;
using Catch::Approx;
using math::Transform;
using model::Model;

TEST_CASE("Obiecte dinamice: parametrii refac geometria, copiile legate se schimbă împreună", "[core][parametric]")
{
    Model m;
    commands::CommandStack stack(m);
    stack.setValidation(true, geom::ValidationLevel::Full);
    stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });

    model::InstanceId door;
    REQUIRE(stack.run("Ușă", [&](Model& model) {
        door = model::createParametric(model, model.root(), "door", { { "width", 800 } }, Transform::translation({ 1000, 0, 0 }));
    }));
    REQUIRE(door);
    const model::DefinitionId def = m.instance(door).definition;
    CHECK(m.definition(def).generator == "door");
    CHECK(m.definition(def).parameters.at("width") == Approx(800.0));
    CHECK(m.definition(def).parameters.at("height") == Approx(2100.0)); // implicit
    CHECK(model::placementOf(m, door).size.x == Approx(800.0));
    CHECK(model::placementOf(m, door).size.z == Approx(2100.0));
    CHECK(m.childrenOf(def).size() >= 4); // tocul (3 bucăți), foaia, clanța

    // O copie legată; lățimea nouă se vede la amândouă, iar valorile sunt limitate la min / max.
    model::InstanceId copy;
    REQUIRE(stack.run("Copie", [&](Model& model) { copy = model::copyInstance(model, door, Transform::translation({ 0, 2000, 0 })); }));
    REQUIRE(stack.run("Lățime", [&](Model& model) { model::setParameter(model, def, "width", 1200); }));
    CHECK(model::placementOf(m, door).size.x == Approx(1200.0));
    CHECK(model::placementOf(m, copy).size.x == Approx(1200.0));
    REQUIRE(stack.run("Prea lată", [&](Model& model) { model::setParameter(model, def, "width", 99999); }));
    CHECK(m.definition(def).parameters.at("width") == Approx(2000.0));

    // Undo readuce parametrii și geometria.
    REQUIRE(stack.undo());
    REQUIRE(stack.undo());
    CHECK(model::placementOf(m, door).size.x == Approx(800.0));

    // Salvat și încărcat: generatorul și parametrii rămân.
    const io::LoadResult loaded = io::readArchive(io::writeArchive(m, {}));
    REQUIRE(loaded.model);
    CHECK(loaded.model->sameAs(m));
}

TEST_CASE("Obiecte dinamice: toate generatoarele construiesc geometrie validă", "[core][parametric]")
{
    for (const model::GeneratorInfo& g : model::generators()) {
        Model m;
        const model::InstanceId id = model::createParametric(m, m.root(), g.id, {}, {});
        INFO(g.id);
        REQUIRE(id);
        const model::Placement p = model::placementOf(m, id);
        CHECK(p.size.x > 100.0);
        CHECK(p.size.z > 100.0);
        CHECK(m.validate().ok());
        // Un parametru numeric de tip „buc.” schimbă numărul de părți.
        for (const model::ParameterSpec& spec : g.parameters) {
            if (spec.unit == "buc.") {
                const std::size_t before = m.childrenOf(m.instance(id).definition).size();
                model::setParameter(m, m.instance(id).definition, spec.key, spec.value + 1);
                CHECK(m.childrenOf(m.instance(id).definition).size() > before);
            }
        }
    }
}

TEST_CASE("Biblioteca: un obiect extras și inserat în alt model rămâne la fel (și dinamic)", "[core][parametric][library]")
{
    Model a;
    const model::InstanceId table = model::createParametric(a, a.root(), "table", { { "width", 1400 } },
        Transform::translation({ 3000, 2000, 0 }));
    const model::Placement original = model::placementOf(a, table);

    // Extras: în originea modelului nou, cu generatorul păstrat.
    const Model extracted = model::extractInstance(a, table);
    CHECK(extracted.definition(extracted.root()).generator == "table");
    const auto [lo, hi] = model::bounds(extracted, extracted.root());
    CHECK(lo.x == Approx(0.0).margin(1e-6));
    CHECK(hi.x - lo.x == Approx(1400.0));
    CHECK(extracted.validate().ok());

    // Inserat într-un model care are deja un material cu același nume: îl refolosește.
    Model b;
    commands::CommandStack stack(b);
    stack.setValidation(true, geom::ValidationLevel::Full);
    stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
    b.addMaterial({ "Lemn de stejar", { 1, 2, 3, 255 }, 0.5f, 0.0f });
    model::InstanceId inserted;
    REQUIRE(stack.run("Inserează", [&](Model& m) {
        inserted = model::insertModel(m, m.root(), extracted, "Masă", Transform::translation({ -500, 0, 0 }));
    }));
    CHECK(model::placementOf(b, inserted).size.x == Approx(original.size.x));
    CHECK(model::placementOf(b, inserted).size.z == Approx(original.size.z));
    CHECK(b.materials().size() == 1);
    CHECK(b.definition(b.instance(inserted).definition).generator == "table");
    // Încă dinamic: parametrii schimbă obiectul inserat.
    REQUIRE(stack.run("Lățime", [&](Model& m) { model::setParameter(m, m.instance(inserted).definition, "width", 1000); }));
    CHECK(model::placementOf(b, inserted).size.x == Approx(1000.0));
    REQUIRE(stack.undo());
    REQUIRE(stack.undo());
    CHECK(b.instances().empty());
}
