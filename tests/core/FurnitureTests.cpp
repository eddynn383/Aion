#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Bevel.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/MaterialLibrary.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Parametric.h"
#include "aion/core/model/Quantities.h"
#include "aion/core/model/Transfer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

// O placă PAL 18 (cutia 600 × 400 × 18, cu grosimea pe Z), cu datele ei de piesă.
model::DefinitionId boardDefinition(
    Model& m, Vec3d size = { 600, 400, 18 }, int thicknessAxis = 2, int grainAxis = 0)
{
    model::Definition def { "Placă", false };
    model::PartSpec spec;
    spec.typeKey = "pal";
    spec.nominalThickness = 18.0;
    spec.thicknessAxis = static_cast<std::uint8_t>(thicknessAxis);
    spec.grainAxis = static_cast<std::uint8_t>(grainAxis);
    def.part = spec;
    const model::DefinitionId d = m.addDefinition(def);
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, size, 4);
    return d;
}

geom::FaceId faceWithNormal(const geom::Mesh& mesh, Vec3d normal)
{
    for (geom::FaceId f : mesh.faces().keys()) {
        if (glm::dot(mesh.face(f).plane.normal, normal) > 0.999) {
            return f;
        }
    }
    FAIL("nicio față cu normala dată");
    return {};
}

} // namespace

TEST_CASE("Mobilier: tipurile de plăci și grosimile permise", "[core][furniture]")
{
    const model::BoardType* pal = model::findBoardType("pal");
    REQUIRE(pal != nullptr);
    CHECK(pal->thicknesses == std::vector<double> { 16.0, 18.0, 25.0 });
    const model::BoardType* hdf = model::findBoardType("hdf");
    REQUIRE(hdf != nullptr);
    CHECK(hdf->thicknesses == std::vector<double> { 3.0, 4.0 });
    CHECK(model::findBoardType("necunoscut") == nullptr);

    // La egalitate se alege grosimea mai mare.
    CHECK(model::nearestThickness("pal", 17.0) == 18.0);
    CHECK(model::nearestThickness("pal", 20.0) == 18.0);
    CHECK(model::nearestThickness("hdf", 10.0) == 4.0);
    CHECK(model::nearestThickness("necunoscut", 17.0) == 17.0);
}

TEST_CASE("Mobilier: fiecare tip de cant e compatibil cu tipuri de placă existente", "[core][furniture]")
{
    for (const model::BandType& band : model::bandTypes()) {
        INFO(band.key);
        CHECK_FALSE(band.boardTypes.empty());
        for (const std::string& board : band.boardTypes) {
            CHECK(model::findBoardType(board) != nullptr);
            CHECK(model::bandFitsBoard(band.key, board));
        }
    }
    CHECK_FALSE(model::bandFitsBoard("abs.2", "hdf"));
}

TEST_CASE("Mobilier: categoriile și cheile materialelor incluse", "[core][furniture]")
{
    const auto library = model::builtinMaterials();
    const auto find = [&](const std::string& name) {
        const auto it = std::find_if(
            library.begin(), library.end(), [&](const model::Material& m) { return m.name == name; });
        REQUIRE(it != library.end());
        return *it;
    };
    CHECK(find("Oțel periat").category == "metal");
    CHECK(find("Aluminiu").category == "metal");
    CHECK(find("Sticlă fumurie").category == "glass");
    CHECK(find("Piatră").category == "stone");
    CHECK(find("Lemn de nuc").category == "board-decor");
    CHECK(find("Nuc masiv").category == "wood");
    CHECK(find("Alb mat").category.empty());
    // Cheile sunt unice și nevide.
    for (std::size_t i = 0; i < library.size(); ++i) {
        CHECK_FALSE(library[i].libraryKey.empty());
        CHECK(model::libraryMaterialByKey(library[i].libraryKey) == i);
    }
    // Indicii vechi nu s-au mutat (scripturile folosesc `material:front,24`).
    CHECK(library[24].name == "Cărămidă aparentă");
    CHECK(model::builtinTexture(24) == "brick");
    CHECK(model::builtinTexture(34) == "wood");
}

TEST_CASE("Mobilier: feroneria construită în cod", "[core][furniture]")
{
    Model m;
    const model::InstanceId hinge = model::createHardware(m, m.root(), "hinge.overlay.110", {});
    REQUIRE(hinge);
    const model::Definition& def = m.definition(m.instance(hinge).definition);
    REQUIRE(def.part);
    CHECK(def.part->kind == model::PartSpec::Kind::Hardware);
    CHECK(def.part->typeKey == "hinge.overlay.110");
    CHECK(m.mesh(m.instance(hinge).definition).faces().size() == 6);
    CHECK(model::findHardwareType("slide.450")->unit == model::Unit::Pair);
    CHECK_FALSE(model::createHardware(m, m.root(), "necunoscut", {}));

    // Două balamale folosesc același material de oțel.
    const model::InstanceId second = model::createHardware(m, m.root(), "hinge.overlay.110", {});
    CHECK(m.instance(second).material == m.instance(hinge).material);
    CHECK(m.materials().size() == 1);
}

TEST_CASE(
    "Mobilier: un fișier v5 își primește categoriile materialelor la încărcare", "[core][furniture][io]")
{
    Model m;
    model::Material steel { "Oțel periat", { 170, 172, 176, 255 } };
    model::Material mine { "Materialul meu", { 10, 20, 30, 255 } };
    m.addMaterial(steel);
    m.addMaterial(mine);
    std::string text = io::toJson(m);
    const std::string current = "\"formatVersion\": 6";
    REQUIRE(text.find(current) != std::string::npos);
    text.replace(text.find(current), current.size(), "\"formatVersion\": 5");
    const io::LoadResult loaded = io::fromJson(text);
    INFO(loaded.error);
    REQUIRE(loaded.model);
    for (const model::Material& mat : loaded.model->materials().values()) {
        if (mat.name == "Oțel periat") {
            CHECK(mat.category == "metal");
            CHECK(mat.libraryKey == "otel-periat");
        } else {
            CHECK(mat.category.empty());
            CHECK(mat.libraryKey.empty());
        }
    }
}

TEST_CASE(
    "Mobilier: la inserare, un decor nu se unește cu un lemn masiv cu același nume", "[core][furniture]")
{
    // Sursa: un obiect cu decorul „Nuc (decor)” din bibliotecă.
    Model source;
    const auto decorIndex = model::libraryMaterialByKey("nuc-decor");
    REQUIRE(decorIndex);
    const model::MaterialId decor = source.addMaterial(model::builtinMaterials()[*decorIndex]);
    const model::DefinitionId d = source.addDefinition({ "Placă", false });
    model::Instance inst;
    inst.parent = source.root();
    inst.definition = d;
    inst.material = decor;
    source.addInstance(inst);

    // Ținta are un material al utilizatorului cu același nume, dar lemn masiv.
    Model target;
    model::Material mine { "Nuc (decor)", { 90, 60, 40, 255 } };
    mine.category = "wood";
    target.addMaterial(mine);
    model::insertModel(target, target.root(), source, "Obiect", {});
    CHECK(target.materials().size() == 2);

    // Inserat a doua oară: același decor (după cheie), fără dubluri.
    model::insertModel(target, target.root(), source, "Obiect", {});
    CHECK(target.materials().size() == 2);
}

TEST_CASE("Mobilier: măsurarea unei plăci pe axele ei", "[core][furniture]")
{
    Model m;
    const model::DefinitionId d = boardDefinition(m);
    model::PartMeasure p = model::measurePart(m, d, {});
    CHECK(p.length == Approx(600.0));
    CHECK(p.width == Approx(400.0));
    CHECK(p.thickness == Approx(18.0));
    CHECK(p.box);
    CHECK_FALSE(p.extraShells);

    SECTION("rotită 37° în jurul lui Z: aceleași dimensiuni")
    {
        const auto r = model::measurePart(
            m, d, Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, 37.0 * std::numbers::pi / 180.0));
        CHECK(r.length == Approx(600.0));
        CHECK(r.width == Approx(400.0));
        CHECK(r.thickness == Approx(18.0));
    }
    SECTION("scalată 1,5 pe lungime: lungimea crește")
    {
        const auto r = model::measurePart(m, d, Transform::scaling({ 0, 0, 0 }, { 1.5, 1, 1 }));
        CHECK(r.length == Approx(900.0));
    }
    SECTION("fața de grosime trasă cu Push/Pull până la 36")
    {
        geom::pushPull(m.mesh(d), faceWithNormal(m.mesh(d), { 0, 0, 1 }), 18.0);
        const auto r = model::measurePart(m, d, {});
        CHECK(r.thickness == Approx(36.0));
        CHECK(r.box);
    }
    SECTION("Bevel pe o muchie: aceleași dimensiuni, nu mai e cutie simplă")
    {
        geom::Mesh& mesh = m.mesh(d);
        const geom::FaceId top = faceWithNormal(mesh, { 0, 0, 1 });
        const geom::EdgeId edge = mesh.face(top).outer.uses.front().edge;
        geom::bevelEdges(mesh, { edge }, 5.0, 1);
        const auto r = model::measurePart(m, d, {});
        CHECK(r.length == Approx(600.0));
        CHECK(r.thickness == Approx(18.0));
        CHECK_FALSE(r.box);
    }
    SECTION("o linie desenată în interior: geometrie în plus")
    {
        geom::drawLine(m.mesh(d), { -10, -10, 5 }, { 10, 10, 9 });
        CHECK(model::measurePart(m, d, {}).extraShells);
    }
}

TEST_CASE("Mobilier: fibra pe latura scurtă dă lungimea", "[core][furniture]")
{
    Model m;
    const model::DefinitionId d = boardDefinition(m, { 600, 400, 18 }, 2, 1);
    const auto r = model::measurePart(m, d, {});
    CHECK(r.length == Approx(400.0));
    CHECK(r.width == Approx(600.0));
}

TEST_CASE("Mobilier: „Fă placă” găsește axa grosimii și fibra", "[core][furniture]")
{
    Model m;
    const model::DefinitionId d = m.addDefinition({ "Laterală", false });
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, { 600, 18, 400 }, 4);
    model::Instance inst;
    inst.parent = m.root();
    inst.definition = d;
    const model::InstanceId id = m.addInstance(inst);
    REQUIRE(model::makeBoard(m, id, "pal"));
    const model::PartSpec& part = *m.definition(d).part;
    CHECK(part.typeKey == "pal");
    CHECK(part.nominalThickness == 18.0);
    CHECK(part.thicknessAxis == 1);
    CHECK(part.grainAxis == 0);

    // Un obiect cu alte obiecte în el nu devine placă.
    const model::DefinitionId body = m.addDefinition({ "Corp", false });
    model::Instance child;
    child.parent = body;
    child.definition = d;
    m.addInstance(child);
    geom::buildPrimitive(m.mesh(body), geom::Primitive::Box, { 100, 100, 100 }, 4);
    model::Instance placed;
    placed.parent = m.root();
    placed.definition = body;
    CHECK_FALSE(model::makeBoard(m, m.addInstance(placed), "pal"));
}

// --- Lista de debitare v1.5 (pasul 4) ---

namespace {

// O placă etichetată, plasată în `parent`: `size` în axele lumii, cu axa grosimii și a fibrei date.
model::InstanceId placeBoard(Model& m, model::DefinitionId parent, const std::string& name, Vec3d size,
    int thick, int grain, Vec3d at, const std::string& type = "pal", double nominal = 18.0)
{
    model::Definition def { name, false };
    model::PartSpec spec;
    spec.typeKey = type;
    spec.nominalThickness = nominal;
    spec.thicknessAxis = static_cast<std::uint8_t>(thick);
    spec.grainAxis = static_cast<std::uint8_t>(grain);
    def.part = spec;
    const model::DefinitionId d = m.addDefinition(def);
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, size, 4);
    model::Instance inst;
    inst.parent = parent;
    inst.definition = d;
    inst.transform = Transform::translation(at);
    inst.name = name;
    return m.addInstance(inst);
}

struct ReferenceCabinet {
    Model m;
    model::DefinitionId body;
    model::InstanceId placed;
    std::vector<model::InstanceId> doors;
};

// Dulapul de referință (docs/plans/mobilier-pe-corp.md, §4): 1000 × 2000 × 600, PAL 18, spate HDF 3, 2 uși cu
// câte 4 balamale și un mâner în grupul ușii.
ReferenceCabinet referenceCabinet()
{
    ReferenceCabinet c;
    Model& m = c.m;
    c.body = m.addDefinition({ "Dulap", false });
    placeBoard(m, c.body, "Lateral stânga", { 18, 600, 2000 }, 0, 2, { 9, 300, 0 });
    placeBoard(m, c.body, "Lateral dreapta", { 18, 600, 2000 }, 0, 2, { 991, 300, 0 });
    placeBoard(m, c.body, "Bază", { 964, 600, 18 }, 2, 0, { 500, 300, 0 });
    placeBoard(m, c.body, "Capac", { 964, 600, 18 }, 2, 0, { 500, 300, 1982 });
    placeBoard(m, c.body, "Spate", { 964, 3, 1964 }, 1, 2, { 500, 598.5, 18 }, "hdf", 3.0);
    for (int i = 0; i < 2; ++i) {
        const model::DefinitionId door = m.addDefinition({ "Ușă", false });
        placeBoard(m, door, "Ușă", { 498.5, 18, 2000 }, 1, 2, { 249.25, -9, 0 });
        for (int h = 0; h < 4; ++h) {
            model::createHardware(
                m, door, "hinge.overlay.110", Transform::translation({ 30, 0, 100.0 + h * 600 }));
        }
        model::createHardware(m, door, "handle.128", Transform::translation({ 460, -30, 1000 }));
        model::Instance inst;
        inst.parent = c.body;
        inst.definition = door;
        inst.transform = Transform::translation({ i * 501.5, 0, 0 });
        inst.name = "Ușă " + std::to_string(i + 1);
        c.doors.push_back(m.addInstance(inst));
    }
    model::Instance inst;
    inst.parent = m.root();
    inst.definition = c.body;
    c.placed = m.addInstance(inst);
    return c;
}

const model::CutPart* row(const model::CutList& list, const std::string& name, bool hidden = false)
{
    for (const model::CutPart& p : list.parts) {
        if (std::find(p.usedIn.begin(), p.usedIn.end(), name) != p.usedIn.end() && p.hidden == hidden) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Lista de debitare v1.5: dulapul de referință", "[core][furniture][quantities]")
{
    const ReferenceCabinet c = referenceCabinet();
    const model::CutList list = model::cutList(c.m);
    const model::CutPart* sides = row(list, "Lateral stânga");
    REQUIRE(sides != nullptr);
    CHECK(sides == row(list, "Lateral dreapta")); // același rând, indiferent de nume
    CHECK(sides->count == 2);
    CHECK(sides->length == Approx(2000.0));
    CHECK(sides->width == Approx(600.0));
    CHECK(sides->section == model::CutSection::Board);
    CHECK(sides->flags.empty());
    const model::CutPart* doors = row(list, "Ușă");
    REQUIRE(doors != nullptr);
    CHECK(doors->count == 2);
    CHECK(doors->length == Approx(2000.0));
    CHECK(doors->width == Approx(498.5));
    const model::CutPart* back = row(list, "Spate");
    REQUIRE(back != nullptr);
    CHECK(back->typeKey == "hdf");
    CHECK(back->thickness == Approx(3.0));
    // Feroneria: pe bucăți, nu la plăci.
    REQUIRE(list.hardware.size() == 2);
    for (const model::HardwareNeed& h : list.hardware) {
        CHECK(h.count == (h.typeKey == "hinge.overlay.110" ? 8 : 2));
    }
    // Plăcile brute: PAL 18 și HDF 3.
    REQUIRE(list.sheets.size() == 2);
}

TEST_CASE("Lista de debitare v1.5: piesele ascunse și bifele", "[core][furniture][quantities]")
{
    ReferenceCabinet c = referenceCabinet();
    model::Instance door = c.m.instance(c.doors[0]);
    door.hidden = true;
    c.m.setInstance(c.doors[0], door);

    model::CutList list = model::cutList(c.m);
    const model::CutPart* hiddenDoor = row(list, "Ușă", true);
    REQUIRE(hiddenDoor != nullptr);
    CHECK(hiddenDoor->count == 1);
    const model::CutPart* visibleDoor = row(list, "Ușă", false);
    REQUIRE(visibleDoor != nullptr);
    CHECK(visibleDoor->count == 1);
    // Balamalele și mânerul ușii ascunse: rânduri ascunse separate (4 + 1).
    int hiddenPieces = 0;
    std::set<std::string> hiddenKeys;
    for (const model::CutPart& p : list.parts) {
        if (p.hidden) {
            hiddenPieces += p.count;
            hiddenKeys.insert(p.rowKey);
        }
    }
    CHECK(hiddenPieces == 6);

    // Cu rândurile ascunse debifate, totalurile sunt ale dulapului fără ușa ascunsă.
    model::CutListOptions options;
    options.excluded = hiddenKeys;
    const model::CutList without = model::cutList(c.m, options);
    CHECK(without.hiddenPieces == 6);
    for (const model::HardwareNeed& h : without.hardware) {
        CHECK(h.count == (h.typeKey == "hinge.overlay.110" ? 4 : 1));
    }
    const std::string csv = model::toCsv(without, options);
    CHECK(csv.find("ascunsă") == std::string::npos);

    // Fără excluderi, totalurile sunt aceleași ca atunci când nimic nu e ascuns.
    const model::CutList all = model::cutList(c.m);
    const model::CutList reference = model::cutList(referenceCabinet().m);
    CHECK(all.sheets.size() == reference.sheets.size());
    CHECK(all.sheets[0].partsArea == Approx(reference.sheets[0].partsArea));

    // Raportul general numără în continuare doar ce se vede.
    const model::QuantityReport report = model::quantityReport(c.m);
    for (const model::ObjectQuantity& o : report.objects) {
        CHECK(o.name.find("Ușă 1") == std::string::npos);
    }
}

TEST_CASE("Lista de debitare v1.5: obiectele fără date, după categoria materialului",
    "[core][furniture][quantities]")
{
    Model m;
    const auto box = [&](const std::string& name, Vec3d size, const std::string& materialKey) {
        const model::DefinitionId d = m.addDefinition({ name, false });
        geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, size, 4);
        model::Instance inst;
        inst.parent = m.root();
        inst.definition = d;
        inst.name = name;
        inst.material = m.addMaterial(model::builtinMaterials()[*model::libraryMaterialByKey(materialKey)]);
        m.addInstance(inst);
    };
    box("Mâner", { 20, 25, 200 }, "otel-periat");
    box("Blat masiv", { 1200, 600, 40 }, "nuc-masiv");
    box("Raft", { 800, 300, 18 }, "nuc-decor");
    const model::CutList list = model::cutList(m);
    CHECK(row(list, "Mâner")->section == model::CutSection::Other);
    CHECK(row(list, "Blat masiv")->section == model::CutSection::Undefined);
    const model::CutPart* shelf = row(list, "Raft");
    CHECK(shelf->section == model::CutSection::Board);
    CHECK(std::find(shelf->flags.begin(), shelf->flags.end(), "tip nedefinit") != shelf->flags.end());
}

TEST_CASE("Lista de debitare v1.5: un corp scalat dă dimensiunile măsurate", "[core][furniture][quantities]")
{
    ReferenceCabinet c = referenceCabinet();
    model::Instance placed = c.m.instance(c.placed);
    placed.transform = Transform::scaling({ 0, 0, 0 }, { 1.2, 1.2, 1.2 });
    c.m.setInstance(c.placed, placed);
    const model::CutList list = model::cutList(c.m);
    const model::CutPart* sides = row(list, "Lateral stânga");
    REQUIRE(sides != nullptr);
    CHECK(sides->length == Approx(2400.0));
    CHECK(sides->thickness == Approx(21.6));
    CHECK(std::find(sides->flags.begin(), sides->flags.end(), "grosime nepotrivită") != sides->flags.end());
}

TEST_CASE("Lista de debitare v1.5: CSV-ul nu lasă formulele să ruleze", "[core][furniture][quantities]")
{
    Model m;
    placeBoard(m, m.root(), "=1+1", { 600, 400, 18 }, 2, 0, { 0, 0, 0 });
    placeBoard(m, m.root(), "\tTab", { 300, 400, 18 }, 2, 0, { 0, 0, 100 });
    const std::string csv = model::toCsv(model::cutList(m), {});
    CHECK(csv.find(";'=1+1;") != std::string::npos);
    CHECK(csv.find(";'\tTab;") != std::string::npos);
}

// --- Placa parametrică (pasul 5) ---

namespace {

struct BoardDoc {
    Model model;
    commands::CommandStack stack { model };
    model::InstanceId board;

    BoardDoc()
    {
        stack.setValidation(true, geom::ValidationLevel::Full);
        stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
        stack.setFinalizer([](Model& m, std::span<const model::DefinitionId> changed) {
            model::finalizeBoards(m, changed);
        });
        stack.run("Placă", [&](Model& m) {
            board = model::createBoard(
                m, m.root(), "pal", 18.0, 600.0, 400.0, Transform::translation({ 1000, 0, 0 }));
        });
    }
    model::DefinitionId def() const { return model.instance(board).definition; }
    const model::Definition& definition() const { return model.definition(def()); }
    double param(const char* key) const { return definition().parameters.at(key); }
    model::BoardStatus status() const
    {
        return model::boardState(model, board, model.instance(board).transform);
    }
    Vec3d world(Vec3d local) const { return model.instance(board).transform.applyToPoint(local); }
};

} // namespace

TEST_CASE("Placa parametrică: creare, decor păstrat, generatorul nu e în listă", "[core][furniture][board]")
{
    BoardDoc d;
    CHECK(model::findGenerator("board") == nullptr);
    CHECK(std::none_of(model::generators().begin(), model::generators().end(),
        [](const model::GeneratorInfo& g) { return g.id == "board"; }));
    CHECK(d.model.mesh(d.def()).faces().size() == 6);
    CHECK(d.definition().part->nominalThickness == 18.0);
    CHECK(d.status().state == model::BoardState::Matches);

    const auto walnut = model::libraryMaterialByKey("nuc-decor");
    d.stack.run("Decor", [&](Model& m) {
        model::Instance inst = m.instance(d.board);
        inst.material = m.addMaterial(model::builtinMaterials()[*walnut]);
        m.setInstance(d.board, inst);
    });
    const model::MaterialId decor = d.model.instance(d.board).material;
    d.stack.run("Lungime", [&](Model& m) { model::setBoardValue(m, d.def(), "length", 800.0); });
    CHECK(d.param("length") == Approx(800.0));
    CHECK(d.model.instance(d.board).material == decor);
    CHECK(d.status().state == model::BoardState::Matches);
}

TEST_CASE("Placa parametrică: Push/Pull pe capăt actualizează parametrii (finalizatorul)",
    "[core][furniture][board]")
{
    BoardDoc d;
    const Vec3d farCorner = d.world({ 0, 0, 0 });
    SECTION("capătul +X")
    {
        d.stack.run("Push/Pull", [&](Model& m) {
            geom::Mesh& mesh = m.mesh(d.def());
            geom::pushPull(mesh, faceWithNormal(mesh, { 1, 0, 0 }), 200.0);
        });
        CHECK(d.param("length") == Approx(800.0));
        CHECK(d.status().state == model::BoardState::Matches);
        CHECK(glm::length(d.world({ 0, 0, 0 }) - farCorner) < 1e-6);
        CHECK(d.stack.undoCount() == 2); // un singur pas pentru Push/Pull + preluarea dimensiunilor
    }
    SECTION("capătul −X: originea se mută, placa nu se mișcă în lume")
    {
        const Vec3d plusEnd = d.world({ 600, 0, 0 });
        d.stack.run("Push/Pull", [&](Model& m) {
            geom::Mesh& mesh = m.mesh(d.def());
            geom::pushPull(mesh, faceWithNormal(mesh, { -1, 0, 0 }), 200.0);
        });
        CHECK(d.param("length") == Approx(800.0));
        CHECK(d.status().state == model::BoardState::Matches);
        CHECK(glm::length(d.world({ 800, 0, 0 }) - plusEnd) < 1e-6);
        REQUIRE(d.stack.undo());
        CHECK(d.param("length") == Approx(600.0));
    }
    SECTION("lățimea")
    {
        d.stack.run("Push/Pull", [&](Model& m) {
            geom::Mesh& mesh = m.mesh(d.def());
            geom::pushPull(mesh, faceWithNormal(mesh, { 0, 1, 0 }), 100.0);
        });
        CHECK(d.param("width") == Approx(500.0));
    }
    SECTION("fața de grosime: placa devine „modificată manual”")
    {
        d.stack.run("Push/Pull", [&](Model& m) {
            geom::Mesh& mesh = m.mesh(d.def());
            geom::pushPull(mesh, faceWithNormal(mesh, { 0, 0, 1 }), 2.0);
        });
        CHECK(d.status().state == model::BoardState::Modified);
        CHECK(d.status().reason == model::BoardReason::Thickness);
        d.stack.run("Readu", [&](Model& m) { model::restoreBoard(m, d.board); });
        CHECK(d.status().state == model::BoardState::Matches);
        REQUIRE(d.stack.undo());
        CHECK(d.status().reason == model::BoardReason::Thickness);
    }
    SECTION("Bevel: formă neregulată, apoi „Readu la parametri”")
    {
        d.stack.run("Bevel", [&](Model& m) {
            geom::Mesh& mesh = m.mesh(d.def());
            const geom::FaceId top = faceWithNormal(mesh, { 0, 0, 1 });
            geom::bevelEdges(mesh, { mesh.face(top).outer.uses.front().edge }, 5.0, 1);
        });
        CHECK(d.status().reason == model::BoardReason::Geometry);
        d.stack.run("Readu", [&](Model& m) { model::restoreBoard(m, d.board); });
        CHECK(model::isSimpleBox(d.model.mesh(d.def())));
    }
}

TEST_CASE("Placa parametrică: scara, oglindirea și părintele scalat", "[core][furniture][board]")
{
    BoardDoc d;
    d.stack.run("Scale", [&](Model& m) {
        model::Instance inst = m.instance(d.board);
        inst.transform = inst.transform * Transform::scaling({ 0, 0, 0 }, { 1.5, 1, 1 });
        m.setInstance(d.board, inst);
    });
    CHECK(d.status().reason == model::BoardReason::Scaled);
    d.stack.run("Readu", [&](Model& m) { model::restoreBoard(m, d.board); });
    CHECK(d.status().state == model::BoardState::Matches);

    d.stack.run("Oglindește", [&](Model& m) {
        model::Instance inst = m.instance(d.board);
        inst.transform = Transform::scaling({ 0, 0, 0 }, { -1, 1, 1 }) * inst.transform;
        m.setInstance(d.board, inst);
    });
    CHECK(d.status().state == model::BoardState::Matches);
    // Readu păstrează oglindirea (semnul scării).
    d.stack.run("Readu", [&](Model& m) { model::restoreBoard(m, d.board); });
    CHECK(glm::determinant(glm::dmat3(d.model.instance(d.board).transform.matrix())) < 0.0);

    const auto inScaledParent = model::boardState(d.model, d.board,
        Transform::scaling({ 0, 0, 0 }, { 1.2, 1.2, 1.2 }) * d.model.instance(d.board).transform);
    CHECK(inScaledParent.reason == model::BoardReason::ScaledParent);
}

TEST_CASE("Placa parametrică: pivotul rămâne pe loc la schimbarea dimensiunilor", "[core][furniture][board]")
{
    BoardDoc d;
    SECTION("pivotul implicit (centrul): placa crește simetric")
    {
        const Vec3d center = d.world({ 300, 200, 9 });
        d.stack.run("Lungime", [&](Model& m) { model::setBoardValue(m, d.def(), "length", 800.0); });
        CHECK(glm::length(d.world({ 400, 200, 9 }) - center) < 1e-6);
    }
    SECTION("pivotul explicit la capăt: capătul rămâne pe loc")
    {
        d.stack.run("Pivot", [&](Model& m) {
            model::Instance inst = m.instance(d.board);
            inst.pivot = Vec3d { 600, 0, 0 };
            m.setInstance(d.board, inst);
        });
        const Vec3d end = d.world({ 600, 0, 0 });
        d.stack.run("Lungime", [&](Model& m) { model::setBoardValue(m, d.def(), "length", 800.0); });
        CHECK(glm::length(d.world({ 800, 0, 0 }) - end) < 1e-6);
        CHECK(d.model.instance(d.board).pivot->x == Approx(800.0));
    }
    SECTION("patru instanțe legate: toate compensate, un pas de undo")
    {
        std::vector<model::InstanceId> copies { d.board };
        std::vector<Vec3d> centers { d.world({ 300, 200, 9 }) };
        d.stack.run("Copii", [&](Model& m) {
            for (int i = 1; i < 4; ++i) {
                copies.push_back(model::copyLinked(m, d.board, Transform::translation({ 0, 1000.0 * i, 0 })));
            }
        });
        for (std::size_t i = 1; i < copies.size(); ++i) {
            centers.push_back(d.model.instance(copies[i]).transform.applyToPoint({ 300, 200, 9 }));
        }
        const std::size_t steps = d.stack.undoCount();
        d.stack.run("Lungime", [&](Model& m) { model::setBoardValue(m, d.def(), "length", 1000.0); });
        CHECK(d.stack.undoCount() == steps + 1);
        for (std::size_t i = 0; i < copies.size(); ++i) {
            CHECK(
                glm::length(d.model.instance(copies[i]).transform.applyToPoint({ 500, 200, 9 }) - centers[i])
                < 1e-6);
        }
    }
}

TEST_CASE("Placa parametrică: mutarea originii păstrează animațiile și scenele", "[core][furniture][board]")
{
    BoardDoc d;
    // Placa rotită 90° în jurul lui Y (ca o laterală), cu pivot explicit.
    const Vec3d pivot { 100, 50, 9 };
    d.stack.run("Așezare", [&](Model& m) {
        model::Instance inst = m.instance(d.board);
        inst.transform = Transform::translation({ 500, 0, 0 })
            * Transform::rotation({ 0, 0, 0 }, { 0, 1, 0 }, std::numbers::pi / 2);
        inst.pivot = pivot;
        m.setInstance(d.board, inst);
    });
    const Transform before = d.model.instance(d.board).transform;
    const model::TransformValues values
        = model::transformValues(before, pivot); // ce ar fi salvat într-o cheie
    const Vec3d corner = before.applyToPoint({ 600, 400, 18 });
    d.stack.run("Push/Pull", [&](Model& m) {
        geom::Mesh& mesh = m.mesh(d.def());
        geom::pushPull(mesh, faceWithNormal(mesh, { -1, 0, 0 }), 150.0);
    });
    const model::Instance& after = d.model.instance(d.board);
    // Același colț fizic (acum la x = 750 în coordonatele plăcii) e în același loc.
    CHECK(glm::length(after.transform.applyToPoint({ 750, 400, 18 }) - corner) < 1e-6);
    // Cheia de animație (valorile față de pivot) dă aceeași poză cu pivotul nou.
    const Transform keyed = model::composeTransform(values, *after.pivot);
    CHECK(glm::length(keyed.applyToPoint({ 750, 400, 18 }) - corner) < 1e-6);
}

TEST_CASE("Placa parametrică: tipul nou aduce grosimea permisă și scoate canturile incompatibile",
    "[core][furniture][board]")
{
    BoardDoc d;
    d.stack.run("Cant", [&](Model& m) {
        model::Definition def = m.definition(d.def());
        def.part->bands[0] = { "abs.2", "", false };
        m.setDefinition(d.def(), def);
    });
    d.stack.run("Tip", [&](Model& m) { model::setBoardType(m, d.def(), "hdf"); });
    CHECK(d.definition().part->typeKey == "hdf");
    CHECK(d.definition().part->nominalThickness == 4.0);
    CHECK(d.definition().part->bands[0].bandKey.empty());
    CHECK(d.status().state == model::BoardState::Matches);
}

TEST_CASE(
    "Finalizatorul: nu atinge geometria obișnuită; o excepție anulează operația", "[core][furniture][board]")
{
    const auto cube = [](Model& m) {
        const model::DefinitionId c = m.addDefinition({ "Cub", false });
        geom::buildPrimitive(m.mesh(c), geom::Primitive::Box, { 100, 100, 100 }, 4);
        return c;
    };
    Model with;
    Model without;
    commands::CommandStack withStack(with);
    commands::CommandStack withoutStack(without);
    withStack.setFinalizer(
        [](Model& m, std::span<const model::DefinitionId> c) { model::finalizeBoards(m, c); });
    model::DefinitionId a;
    model::DefinitionId b;
    withStack.run("Cub", [&](Model& m) { a = cube(m); });
    withoutStack.run("Cub", [&](Model& m) { b = cube(m); });
    withStack.run("Push/Pull",
        [&](Model& m) { geom::pushPull(m.mesh(a), faceWithNormal(m.mesh(a), { 1, 0, 0 }), 50.0); });
    withoutStack.run("Push/Pull",
        [&](Model& m) { geom::pushPull(m.mesh(b), faceWithNormal(m.mesh(b), { 1, 0, 0 }), 50.0); });
    CHECK(with.sameAs(without));

    const Model snapshot = with;
    withStack.setFinalizer(
        [](Model&, std::span<const model::DefinitionId>) { throw std::runtime_error("eroare de test"); });
    const std::size_t steps = withStack.undoCount();
    CHECK_FALSE(withStack.run("Push/Pull",
        [&](Model& m) { geom::pushPull(m.mesh(a), faceWithNormal(m.mesh(a), { 0, 1, 0 }), 50.0); }));
    CHECK(withStack.lastError() == "eroare de test");
    CHECK(withStack.undoCount() == steps);
    CHECK(with.sameAs(snapshot));
}
