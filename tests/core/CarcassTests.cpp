#include "aion/core/model/Carcass.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/Parametric.h"
#include "aion/core/model/Quantities.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

// Corpurile din bibliotecă (pasul 6; docs/plans/mobilier-pe-corp.md, §4).

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

struct Body {
    Model m;
    model::InstanceId placed;
    model::DefinitionId def;
};

Body makeBody(std::string_view generator, const std::map<std::string, double>& parameters = {})
{
    Body b;
    b.placed = model::createParametric(b.m, b.m.root(), generator, parameters, Transform {});
    b.def = b.m.instance(b.placed).definition;
    return b;
}

// Piesa cu rolul dat (în corp sau într-un grup cu rol din corp); nulă dacă lipsește.
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

int countRoles(const Model& m, model::DefinitionId body, std::string_view prefix)
{
    int n = 0;
    for (model::InstanceId id : m.childrenOf(body)) {
        n += m.instance(id).roleKey.starts_with(prefix) ? 1 : 0;
    }
    return n;
}

const model::CutPart* row(const model::CutList& list, const std::string& name)
{
    for (const model::CutPart& p : list.parts) {
        if (std::find(p.usedIn.begin(), p.usedIn.end(), name) != p.usedIn.end()) {
            return &p;
        }
    }
    return nullptr;
}

// Centrul unei plăci, în coordonatele corpului.
Vec3d center(const Model& m, model::InstanceId id)
{
    const model::Instance& inst = m.instance(id);
    const model::Definition& def = m.definition(inst.definition);
    const Vec3d size { def.parameters.at("length"), def.parameters.at("width"), def.part->nominalThickness };
    return inst.transform.applyToPoint(size * 0.5);
}

} // namespace

TEST_CASE("Corpuri: generatoarele noi și cele vechi", "[core][carcass]")
{
    for (const char* id : { "base", "wall", "tall", "appliance", "wardrobe", "rack" }) {
        const model::GeneratorInfo* g = model::findGenerator(id);
        REQUIRE(g != nullptr);
        CHECK(g->category == "Corpuri");
        CHECK_FALSE(g->legacy);
        CHECK(model::isCarcassGenerator(id));
    }
    CHECK(model::findGenerator("cabinet")->legacy);
    CHECK(model::findGenerator("shelf")->legacy);
    CHECK_FALSE(model::isCarcassGenerator("cabinet"));
}

TEST_CASE("Corpuri: dulapul de referință și lista lui de debitare", "[core][carcass][quantities]")
{
    Body b = makeBody("wardrobe");
    const model::CutList list = model::cutList(b.m);
    const model::CutPart* sides = row(list, "Lateral stânga");
    REQUIRE(sides != nullptr);
    CHECK(sides == row(list, "Lateral dreapta"));
    CHECK(sides->count == 2);
    CHECK(sides->length == Approx(2000.0));
    CHECK(sides->width == Approx(600.0));
    CHECK(sides->thickness == Approx(18.0));
    const model::CutPart* bottom = row(list, "Bază");
    REQUIRE(bottom != nullptr);
    CHECK(bottom == row(list, "Capac"));
    CHECK(bottom->count == 2);
    CHECK(bottom->length == Approx(964.0));
    CHECK(bottom->width == Approx(600.0));
    const model::CutPart* back = row(list, "Spate");
    REQUIRE(back != nullptr);
    CHECK(back->typeKey == "hdf");
    CHECK(back->thickness == Approx(3.0));
    CHECK(back->length == Approx(1964.0));
    CHECK(back->width == Approx(964.0));
    const model::CutPart* doors = row(list, "Ușă");
    REQUIRE(doors != nullptr);
    CHECK(doors->count == 2);
    CHECK(doors->length == Approx(2000.0));
    CHECK(doors->width == Approx(498.5));
    REQUIRE(list.hardware.size() == 2);
    for (const model::HardwareNeed& h : list.hardware) {
        CHECK(h.count == (h.typeKey == "hinge.overlay.110" ? 8 : 2));
    }
    // Plăcile sunt cutii pe locul lor: laterala dreaptă la x ∈ [982, 1000].
    CHECK(center(b.m, role(b.m, b.def, "side.right")).x == Approx(991.0));
    CHECK(center(b.m, role(b.m, b.def, "back")).y == Approx(598.5));
    CHECK_FALSE(model::rolePiecesDiffer(b.m, b.def));
}

TEST_CASE("Corpuri: balamalele după înălțimea ușii", "[core][carcass]")
{
    Body low = makeBody("base");
    CHECK(countRoles(low.m, low.m.instance(role(low.m, low.def, "door.1")).definition, "door.1.hinge.") == 2);
    Body mid = makeBody("base", { { "height", 1000 } });
    CHECK(countRoles(mid.m, mid.m.instance(role(mid.m, mid.def, "door.1")).definition, "door.1.hinge.") == 3);
    Body tall = makeBody("wardrobe");
    CHECK(countRoles(tall.m, tall.m.instance(role(tall.m, tall.def, "door.2")).definition, "door.2.hinge.")
        == 4);
    // Ușa 1 din 2 are balamalele pe stânga, ușa 2 pe dreapta.
    CHECK(tall.m.instance(role(tall.m, tall.def, "door.1")).behavior.anchor == model::Behavior::Anchor::Left);
    CHECK(
        tall.m.instance(role(tall.m, tall.def, "door.2")).behavior.anchor == model::Behavior::Anchor::Right);
}

TEST_CASE("Corpuri: refacerea păstrează decorul, cantul și separatorul", "[core][carcass]")
{
    Body b = makeBody("base");
    const model::InstanceId panel = role(b.m, b.def, "door.1.panel");
    REQUIRE(panel);
    // Decor nuc pe ușă, cant ABS 2 pe o latură.
    model::Material nut;
    nut.name = "Nuc (decor)";
    const model::MaterialId nutId = b.m.addMaterial(nut);
    model::Instance inst = b.m.instance(panel);
    inst.material = nutId;
    b.m.setInstance(panel, inst);
    model::Definition panelDef = b.m.definition(inst.definition);
    panelDef.part->bands[0] = { "abs.2", "" };
    b.m.setDefinition(inst.definition, panelDef);
    // Un separator la 1/3 din lățimea interioară (564).
    REQUIRE(model::addDivider(b.m, b.def, { 18 + 564.0 / 3, 100, 300 }, true) == model::DividerResult::Added);
    REQUIRE(countRoles(b.m, b.def, "divider.click.") == 1);

    REQUIRE(model::setParameter(b.m, b.def, "width", 800));
    const model::InstanceId after = role(b.m, b.def, "door.1.panel");
    CHECK(after == panel);
    CHECK(b.m.instance(after).material == nutId);
    CHECK(b.m.definition(b.m.instance(after).definition).part->bands[0].bandKey == "abs.2");
    CHECK(b.m.definition(b.m.instance(after).definition).parameters.at("width") == Approx(800.0));
    const model::InstanceId divider = role(b.m, b.def, "divider.click.1");
    REQUIRE(divider);
    CHECK(*b.m.instance(divider).relativePosition == Approx(1.0 / 3));
    CHECK(center(b.m, divider).x == Approx(18 + 764.0 / 3));
}

TEST_CASE("Corpuri: polițele în minus se șterg, celelalte rămân", "[core][carcass]")
{
    Body b = makeBody("rack", { { "shelves", 3 } });
    const model::InstanceId first = role(b.m, b.def, "space.1.shelf.1");
    REQUIRE(role(b.m, b.def, "space.1.shelf.3"));
    REQUIRE(model::setParameter(b.m, b.def, "shelves", 2));
    CHECK_FALSE(role(b.m, b.def, "space.1.shelf.3"));
    CHECK(role(b.m, b.def, "space.1.shelf.1") == first);
    CHECK(countRoles(b.m, b.def, "space.1.shelf.") == 2);
}

TEST_CASE("Corpuri: separatorul pus cu clic împarte polițele", "[core][carcass]")
{
    Body b = makeBody("rack", { { "shelves", 2 } });
    CHECK(countRoles(b.m, b.def, "space.") == 2);
    // Fără ⌥: la mijlocul spațiului.
    REQUIRE(model::addDivider(b.m, b.def, { 200, 100, 500 }, false) == model::DividerResult::Added);
    CHECK(countRoles(b.m, b.def, "space.1.shelf.") == 2);
    CHECK(countRoles(b.m, b.def, "space.2.shelf.") == 2);
    CHECK(center(b.m, role(b.m, b.def, "divider.click.1")).x == Approx(400.0));
    // Prea aproape de separator.
    CHECK(model::addDivider(b.m, b.def, { 420, 100, 500 }, true) == model::DividerResult::TooClose);
    CHECK(model::addDivider(b.m, b.def, { 5, 100, 500 }, true) == model::DividerResult::Outside);
}

TEST_CASE("Corpuri: un separator mutat de mână rămâne unde e", "[core][carcass]")
{
    Body b = makeBody("rack");
    REQUIRE(model::addDivider(b.m, b.def, { 300, 100, 500 }, true) == model::DividerResult::Added);
    const model::InstanceId divider = role(b.m, b.def, "divider.click.1");
    // Mutat de mână cu centrul la jumătatea lățimii interioare (764 / 2 + 18 = 400).
    model::Instance inst = b.m.instance(divider);
    const double dx = 400.0 - center(b.m, divider).x;
    inst.transform = Transform::translation({ dx, 0, 0 }) * inst.transform;
    b.m.setInstance(divider, inst);
    REQUIRE(model::setParameter(b.m, b.def, "height", 1500));
    CHECK(*b.m.instance(divider).relativePosition == Approx(0.5));
    CHECK(center(b.m, divider).x == Approx(400.0));
}

TEST_CASE("Corpuri: o ușă deschisă se închide la refacere", "[core][carcass]")
{
    Body b = makeBody("base");
    const model::InstanceId door = role(b.m, b.def, "door.1");
    model::Instance inst = b.m.instance(door);
    const Transform closed = inst.transform;
    inst.behavior.open = true;
    inst.transform = Transform::rotation({ 0, -18, 0 }, { 0, 0, 1 }, 1.5) * closed;
    b.m.setInstance(door, inst);
    CHECK_FALSE(model::rolePiecesDiffer(b.m, b.def)); // deschiderea nu e o modificare de mână
    REQUIRE(model::setParameter(b.m, b.def, "depth", 500));
    CHECK_FALSE(b.m.instance(door).behavior.open);
    CHECK(b.m.instance(door).transform == closed);
}

TEST_CASE("Corpuri: piesele utilizatorului și copiile nu se dublează", "[core][carcass]")
{
    Body b = makeBody("rack", { { "shelves", 2 } });
    const model::InstanceId shelf = role(b.m, b.def, "space.1.shelf.1");
    const model::CopyResult copy
        = model::copyDeep(b.m, b.def, b.m, { shelf }, Transform::translation({ 0, 0, 100 }));
    REQUIRE(copy.roots.size() == 1);
    CHECK(b.m.instance(copy.roots[0]).roleKey.empty());
    const std::size_t before = b.m.childrenOf(b.def).size();
    REQUIRE(model::setParameter(b.m, b.def, "width", 900));
    CHECK(b.m.childrenOf(b.def).size() == before); // copia rămâne, polițele nu se dublează
    CHECK(b.m.instances().contains(copy.roots[0]));
    CHECK(countRoles(b.m, b.def, "space.1.shelf.") == 2);

    // Două piese cu același rol: cea cu id-ul mai mare își pierde rolul.
    model::Instance dup = b.m.instance(copy.roots[0]);
    dup.roleKey = "space.1.shelf.2";
    b.m.setInstance(copy.roots[0], dup);
    REQUIRE(model::setParameter(b.m, b.def, "width", 950));
    CHECK(b.m.instance(copy.roots[0]).roleKey.empty());
    CHECK(countRoles(b.m, b.def, "space.1.shelf.") == 2);
}

TEST_CASE("Corpuri: modificările de mână se văd înainte de refacere", "[core][carcass]")
{
    Body b = makeBody("base");
    CHECK_FALSE(model::rolePiecesDiffer(b.m, b.def));
    const model::InstanceId side = role(b.m, b.def, "side.left");
    model::Instance inst = b.m.instance(side);
    inst.transform = Transform::translation({ 0, 0, 10 }) * inst.transform;
    b.m.setInstance(side, inst);
    CHECK(model::rolePiecesDiffer(b.m, b.def));
}

TEST_CASE("Corpuri: „Transformă în corp obișnuit” oprește refacerea", "[core][carcass]")
{
    Body b = makeBody("base");
    model::makePlainBody(b.m, b.def);
    CHECK_FALSE(model::setParameter(b.m, b.def, "width", 800));
    CHECK(role(b.m, b.def, "side.left")); // piesele rămân
}

TEST_CASE("Corpuri: un dulap vechi rămâne la fel și se actualizează la cerere", "[core][carcass]")
{
    Body b = makeBody("cabinet");
    const std::size_t oldParts = b.m.childrenOf(b.def).size();
    REQUIRE(model::setParameter(b.m, b.def, "width", 1200)); // builder-ul vechi
    CHECK(b.m.childrenOf(b.def).size() == oldParts);
    CHECK(b.m.definition(b.def).generator == "cabinet");

    REQUIRE(model::upgradeGenerator(b.m, b.def));
    const model::Definition& def = b.m.definition(b.def);
    CHECK(def.generator == "wardrobe");
    CHECK(def.parameters.at("width") == Approx(1200.0));
    CHECK(def.parameters.at("depth") == Approx(582.0));
    const model::InstanceId back = role(b.m, b.def, "back");
    REQUIRE(back);
    CHECK(b.m.definition(b.m.instance(back).definition).part->typeKey == "hdf");
    CHECK(countRoles(b.m, b.m.instance(role(b.m, b.def, "door.1")).definition, "door.1.hinge.") == 4);
    // Piesele vechi (fără rol) au dispărut.
    for (model::InstanceId id : b.m.childrenOf(b.def)) {
        CHECK_FALSE(b.m.instance(id).roleKey.empty());
    }
    CHECK_FALSE(model::upgradeGenerator(b.m, b.def));
}

TEST_CASE("Corpuri: corpul pentru electrocasnice are nișa între două polițe fixe", "[core][carcass]")
{
    Body b = makeBody("appliance");
    REQUIRE(role(b.m, b.def, "niche.bottom"));
    REQUIRE(role(b.m, b.def, "niche.top"));
    CHECK(center(b.m, role(b.m, b.def, "niche.bottom")).z == Approx(850 + 9));
    CHECK(center(b.m, role(b.m, b.def, "niche.top")).z == Approx(850 + 18 + 600 + 9));
    REQUIRE(role(b.m, b.def, "door.lower"));
    REQUIRE(role(b.m, b.def, "door.upper"));
    CHECK(model::addDivider(b.m, b.def, { 300, 100, 300 }, true) == model::DividerResult::NotCarcass);
}

TEST_CASE("Corpuri: grosimea corpului se aduce la cea permisă", "[core][carcass]")
{
    Body b = makeBody("base");
    REQUIRE(model::setParameter(b.m, b.def, "board", 20));
    CHECK(b.m.definition(b.def).parameters.at("board") == Approx(18.0));
    REQUIRE(model::setParameter(b.m, b.def, "board", 23));
    CHECK(b.m.definition(b.def).parameters.at("board") == Approx(25.0));
    const model::InstanceId side = role(b.m, b.def, "side.left");
    CHECK(b.m.definition(b.m.instance(side).definition).part->nominalThickness == Approx(25.0));
}

TEST_CASE("Corpuri: numele pieselor", "[core][carcass]")
{
    Body b = makeBody("base");
    CHECK(model::partName(b.m, role(b.m, b.def, "side.left")) == "Lateral stânga");
    CHECK(model::partName(b.m, role(b.m, b.def, "door.1.hinge.1")) == "Balama aplicată 110°");
    CHECK(model::partName(b.m, b.placed) == "Corp la sol");
}

// --- Cantul (pasul 7) ---

TEST_CASE("Cant: ABS 2 pe toate laturile ușilor se scade și se numără în metri", "[core][carcass][bands]")
{
    Body b = makeBody("wardrobe");
    const model::BandPreset& all = model::builtinBandPresets()[1];
    for (const char* door : { "door.1.panel", "door.2.panel" }) {
        const model::DefinitionId d = b.m.instance(role(b.m, b.def, door)).definition;
        CHECK(model::applyBandPreset(b.m, d, all.sides) == 0);
    }
    const model::CutList list = model::cutList(b.m);
    const model::CutPart* doors = row(list, "Ușă");
    REQUIRE(doors != nullptr);
    CHECK(doors->count == 2);
    CHECK(doors->cutLength == Approx(1996.0));
    CHECK(doors->cutWidth == Approx(494.5));
    REQUIRE(list.bands.size() == 1);
    CHECK(list.bands[0].bandKey == "abs.2");
    CHECK(list.bands[0].length == Approx(4 * 2498.5 + 8 * 50));
}

TEST_CASE("Cant: ABS 0,4 nu se scade; HDF nu primește cant", "[core][carcass][bands]")
{
    Body b = makeBody("wardrobe");
    const model::DefinitionId d = b.m.instance(role(b.m, b.def, "door.1.panel")).definition;
    for (int k = 0; k < 4; ++k) {
        REQUIRE(model::setBand(b.m, d, k, "abs.0.4"));
    }
    const model::CutList list = model::cutList(b.m);
    bool found = false;
    for (const model::CutPart& p : list.parts) {
        if (p.bands[0] == "abs.0.4") {
            found = true;
            CHECK(p.cutLength == Approx(2000.0));
            CHECK(p.cutWidth == Approx(498.5));
        }
    }
    CHECK(found);
    const model::DefinitionId back = b.m.instance(role(b.m, b.def, "back")).definition;
    CHECK_FALSE(model::setBand(b.m, back, 0, "abs.2"));
    CHECK(model::applyBandPreset(b.m, back, model::builtinBandPresets()[1].sides) == 4);
    CHECK(model::boardBands(b.m, back)[0].bandKey.empty());
}

TEST_CASE("Cant: laturile urmează fibra, nu axele locale", "[core][carcass][bands]")
{
    Model m;
    const model::InstanceId id = model::createBoard(m, m.root(), "pal", 18, 600, 400, Transform {});
    const model::DefinitionId d = m.instance(id).definition;
    REQUIRE(model::setBand(m, d, 0, "abs.2")); // L1: o latură lungă (paralelă cu fibra, pe X)
    CHECK(model::boardBands(m, d)[0].bandKey == "abs.2");
    CHECK(m.definition(d).part->bands[0].bandKey == "abs.2"); // −V = −Y
    // Fibra pe Y (placa desprinsă, cu fibra schimbată): L1 e acum latura de la −X.
    model::detachBoard(m, d);
    model::Definition def = m.definition(d);
    def.part->grainAxis = 1;
    def.part->bands = {};
    m.setDefinition(d, def);
    REQUIRE(model::setBand(m, d, 0, "abs.1"));
    CHECK(m.definition(d).part->bands[2].bandKey == "abs.1"); // −U = −X
    // Oglindirea instanței nu mută cantul: datele sunt în coordonatele plăcii.
    model::Instance inst = m.instance(id);
    inst.transform = Transform::scaling(Vec3d { 0.0 }, { -1, 1, 1 }) * inst.transform;
    m.setInstance(id, inst);
    CHECK(model::boardBands(m, d)[0].bandKey == "abs.1");
}

TEST_CASE("Cant: decorul „ca placa” pe copii legate cu decoruri diferite", "[core][carcass][bands]")
{
    Model m;
    const model::InstanceId a = model::createBoard(m, m.root(), "pal", 18, 600, 400, Transform {});
    const model::DefinitionId d = m.instance(a).definition;
    REQUIRE(model::setBand(m, d, 0, "abs.2"));
    model::Instance second = m.instance(a);
    second.transform = Transform::translation({ 0, 1000, 0 });
    model::Material white;
    white.name = "Alb (decor)";
    model::Material nut;
    nut.name = "Nuc (decor)";
    second.material = m.addMaterial(nut);
    m.addInstance(second);
    model::Instance first = m.instance(a);
    first.material = m.addMaterial(white);
    m.setInstance(a, first);
    const model::CutList list = model::cutList(m);
    CHECK(list.bands.size() == 2);
}
