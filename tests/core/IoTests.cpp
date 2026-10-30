#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/io/Exchange.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Transfer.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <numbers>
#include <regex>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

void requireValid(const Model& m)
{
    const auto report = m.validate(geom::ValidationLevel::Full);
    INFO(report.text());
    REQUIRE(report.ok());
}

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

// Un model care atinge tot ce se salvează: geometrie cu găuri și curbe, grupuri, obiecte cu copii, tag-uri.
Model sampleModel()
{
    Model m;
    geom::Mesh& root = m.mesh(m.root());
    // Casa: bloc, acoperiș în două ape, fereastră prin tot blocul.
    geom::drawPolygon(root, { { 0, 0, 0 }, { 6000, 0, 0 }, { 6000, 4000, 0 }, { 0, 4000, 0 } });
    geom::pushPull(root, faceAt(root, { 3000, 2000, 0 }), -2700);
    geom::drawLine(root, { 0, 2000, 2700 }, { 6000, 2000, 2700 });
    geom::transformVertices(root,
        { root.findVertexNear({ 0, 2000, 2700 }, 1e-6), root.findVertexNear({ 6000, 2000, 2700 }, 1e-6) },
        Transform::translation({ 0, 0, 1500 }));
    geom::drawPolygon(root, { { 6000, 1400, 1000 }, { 6000, 2600, 1000 }, { 6000, 2600, 2000 }, { 6000, 1400, 2000 } });
    geom::pushPull(root, faceAt(root, { 6000, 2000, 1500 }), -6000);

    // Un cilindru ca obiect cu două copii, pe un tag.
    std::vector<Vec3d> circle;
    for (int k = 0; k < 24; ++k) {
        const double a = 2.0 * std::numbers::pi * k / 24;
        circle.push_back({ 8000 + 400 * std::cos(a), 1000 + 400 * std::sin(a), 0 });
    }
    geom::drawCurve(root, circle, true, true);
    geom::pushPull(root, faceAt(root, { 8000, 1000, 0 }), -900);
    geom::Selection cylinder;
    for (geom::FaceId f : root.faces().keys()) {
        if (geom::faceInteriorPoint(root, f).x > 7000) {
            cylinder.faces.insert(f);
        }
    }
    const model::InstanceId stool = model::makeGroup(m, m.root(), cylinder, true, "Taburet");
    const model::InstanceId copy = model::copyLinked(m, stool, Transform::translation({ 0, 1500, 0 }));
    const model::TagId furniture = m.addTag({ "Mobilier", { 245, 158, 11, 255 }, true });
    model::Instance inst = m.instance(copy);
    inst.tag = furniture;
    inst.locked = true;
    inst.name = "Taburet 2";
    inst.pivot = math::Vec3d { 10, 20, 0 }; // un pivot ales (se salvează)
    m.setInstance(copy, inst);
    m.addTag({ "Ascuns", { 16, 185, 129, 255 }, false });
    // Materiale: o față a casei și un taburet întreg.
    const model::MaterialId brick = m.addMaterial({ "Cărămidă", { 168, 82, 58, 255 }, 0.9f, 0.0f });
    const model::MaterialId glass = m.addMaterial({ "Sticlă", { 170, 205, 225, 90 }, 0.05f, 0.0f });
    root.setFaceMaterial(faceAt(root, { 0, 1000, 1000 }), brick);
    // O textură (octeții unei imagini) pe cărămidă, cu poziționarea ei pe față.
    const model::TextureId texture = m.addTexture({ "Cărămidă", "procedural:brick", { 0x89, 'P', 'N', 'G', 9, 8, 7 } });
    model::Material textured = m.material(brick);
    textured.texture = texture;
    textured.textureSize = { 600, 300 };
    m.setMaterial(brick, textured);
    geom::TextureTransform placed;
    placed.offset = { 120, -40 };
    placed.rotation = 15;
    placed.scale = { 2, 0.5 };
    root.setFaceTexture(faceAt(root, { 0, 1000, 1000 }), placed);
    inst = m.instance(stool);
    inst.material = glass;
    m.setInstance(stool, inst);
    // Ghidaje, adnotări, o secțiune și o scenă.
    m.addGuide({ m.root(), { 0, 0, 0 }, { 1, 0, 0 } });
    m.addGuide({ m.root(), { 1000, 1000, 0 }, { 0, 0, 0 } });
    m.addAnnotation({ model::Annotation::Kind::Dimension, m.root(), { 0, 0, 0 }, { 6000, 0, 0 }, { 0, -500, 0 }, "" });
    m.addAnnotation({ model::Annotation::Kind::Text, m.root(), { 3000, 0, 1000 }, {}, { 0, -800, 600 }, "Ușa" });
    m.addSection({ "Secțiune 1", { 3000, 2000, 1200 }, { 0, 0, 1 }, false });
    m.addScene({ "Fațada", { 3000, 2000, 1000 }, 0.4, 0.2, 15000, false, "clay", false, {}, {}, {} });
    // O cameră și două lumini.
    model::CameraObject camera;
    camera.name = "Intrare";
    camera.position = { -3000, -5000, 1700 };
    camera.target = { 3000, 2000, 1200 };
    camera.focalLength = 24.0;
    camera.formatWidth = 1600;
    camera.formatHeight = 1200;
    m.addCamera(camera);
    model::Light lamp;
    lamp.name = "Bec";
    lamp.position = { 3000, 2000, 2500 };
    lamp.color = { 255, 200, 150, 255 };
    lamp.intensity = 1.5;
    m.addLight(lamp);
    model::Light spot;
    spot.name = "Spot";
    spot.kind = model::Light::Kind::Spot;
    spot.direction = { 0.0, 0.6, -0.8 };
    spot.cone = 30.0;
    spot.castShadows = true;
    m.addLight(spot);
    requireValid(m);
    return m;
}

std::pair<Vec3d, Vec3d> worldBounds(const Model& m)
{
    return model::bounds(m, m.root());
}

std::filesystem::path tempPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("Fișiere .aion: salvare → încărcare dă exact același model", "[core][io]")
{
    const Model original = sampleModel();
    const std::vector<std::uint8_t> png { 0x89, 'P', 'N', 'G', 1, 2, 3 };
    const std::vector<std::uint8_t> archive = io::writeArchive(original, png);
    REQUIRE_FALSE(archive.empty());

    const io::LoadResult loaded = io::readArchive(archive);
    INFO(loaded.error);
    REQUIRE(loaded.model);
    requireValid(*loaded.model);
    CHECK(loaded.model->sameAs(original));
    CHECK(io::readThumbnail(archive) == png);

    // Și pe disc.
    const auto path = tempPath("aion-roundtrip.aion");
    std::string error;
    REQUIRE(io::saveFile(path, original, {}, &error));
    const io::LoadResult fromDisk = io::loadFile(path);
    REQUIRE(fromDisk.model);
    CHECK(fromDisk.model->sameAs(original));
    std::filesystem::remove(path);

    // Un model încărcat se poate edita mai departe (ID-urile noi nu se suprapun).
    Model edited = *loaded.model;
    geom::drawLine(edited.mesh(edited.root()), { -1000, 0, 0 }, { -1000, 3000, 0 });
    requireValid(edited);
}

TEST_CASE("Fișiere .aion: erori clare pentru fișiere greșite", "[core][io]")
{
    CHECK_FALSE(io::fromJson("nu e json").model);
    CHECK_FALSE(io::fromJson(R"({"format":"altceva"})").model);
    const io::LoadResult newer = io::fromJson(R"({"format":"aion","formatVersion":99})");
    CHECK_FALSE(newer.model);
    CHECK(newer.error.find("mai nouă") != std::string::npos);
    const std::vector<std::uint8_t> junk { 1, 2, 3, 4 };
    CHECK_FALSE(io::readArchive(junk).model);
    CHECK_FALSE(io::loadFile(tempPath("aion-nu-exista.aion")).model);
}

TEST_CASE("OBJ: export și import înapoi păstrează forma (fețele coplanare se reunesc)", "[core][io]")
{
    Model m;
    geom::Mesh& root = m.mesh(m.root());
    geom::drawPolygon(root, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 1000, 0 }, { 0, 1000, 0 } });
    geom::pushPull(root, root.faces().keys().front(), -1500);

    const std::string obj = io::exportObj(m);
    CHECK(obj.find("mtllib model.mtl") != std::string::npos);
    CHECK(io::exportMtl(m).find("newmtl aion_default") != std::string::npos);

    Model back;
    const io::ImportResult r = io::importObj(back, back.root(), obj, "Cutie");
    INFO(r.error);
    REQUIRE(r.instance);
    requireValid(back);
    const model::DefinitionId def = back.instance(r.instance).definition;
    CHECK(back.mesh(def).faces().size() == 6);
    CHECK(back.mesh(def).edges().size() == 12);
    const auto [lo, hi] = worldBounds(back);
    CHECK(glm::length(lo - Vec3d { 0, 0, 0 }) < 0.01);
    CHECK(glm::length(hi - Vec3d { 2000, 1000, 1500 }) < 0.01);

    CHECK_FALSE(io::importObj(back, back.root(), "v 0 0 0\nf 1 2 3\n", "rău").instance);
}

TEST_CASE("glTF: export GLB și import înapoi; copiile unui obiect rămân legate", "[core][io]")
{
    Model original = sampleModel();
    // Copiile unui obiect rămân un singur mesh doar dacă au același material (materialul obiectului se exportă
    // în mesh): le dăm taburetelor același material.
    model::MaterialId glass;
    for (const model::Instance& inst : original.instances().values()) {
        glass = inst.material ? inst.material : glass;
    }
    for (model::InstanceId id : original.instances().keys()) {
        model::Instance inst = original.instance(id);
        inst.material = glass;
        original.setInstance(id, inst);
    }
    // Diacriticele din cale trebuie să meargă și pe Windows (căile nu trec prin codepage-ul local).
    const auto path = tempPath("aion-export-ăîșț.glb");
    std::string error;
    REQUIRE(io::exportGltf(original, path, true, {}, &error));
    REQUIRE(std::filesystem::file_size(path) > 1000);

    Model back;
    const io::ImportResult r = io::importGltf(back, back.root(), path);
    INFO(r.error);
    REQUIRE(r.instance);
    requireValid(back);
    const auto [olo, ohi] = worldBounds(original);
    const auto [blo, bhi] = worldBounds(back);
    CHECK(glm::length(olo - blo) < 0.5);
    CHECK(glm::length(ohi - bhi) < 0.5);

    // Taburetul (un mesh folosit de două noduri) e un obiect cu două copii.
    bool sharedFound = false;
    for (model::DefinitionId d : back.definitions().keys()) {
        if (back.instancesOf(d).size() == 2 && !back.definition(d).group) {
            sharedFound = true;
        }
    }
    CHECK(sharedFound);
    std::filesystem::remove(path);

    // glTF text: materialele modelului, UV-urile și imaginea texturii (inclusă în buffer).
    const auto text = tempPath("aion-export-texturi.gltf");
    REQUIRE(io::exportGltf(original, text, false, {}, &error));
    std::string json;
    {
        // Închis înainte de remove (pe Windows un fișier deschis nu se poate șterge).
        std::ifstream in(text);
        json.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    CHECK(json.find("TEXCOORD_0") != std::string::npos);
    CHECK(json.find("image/png") != std::string::npos);
    CHECK(json.find("Cărămidă") != std::string::npos);
    CHECK(json.find("baseColorTexture") != std::string::npos);
    std::filesystem::remove(text);
}

namespace {

// Un model cu un obiect care pornește două animații (Comută / la infinit și Înapoi / de 3 ori).
Model playingModel()
{
    Model m;
    const model::DefinitionId d = m.addDefinition(model::Definition { "Ușă", true });
    geom::drawLine(m.mesh(d), { 0, 0, 0 }, { 100, 0, 0 });
    model::Instance inst;
    inst.parent = m.root();
    inst.definition = d;
    model::Action toggle;
    toggle.kind = model::ActionKind::PlayAnimation;
    toggle.animation = model::AnimationId { 1 };
    toggle.direction = model::PlayDirection::Toggle;
    toggle.repeat = 0;
    model::Action reverse = toggle;
    reverse.direction = model::PlayDirection::Reverse;
    reverse.repeat = 3;
    inst.rules.push_back({ model::Trigger::Click, "", { toggle, reverse } });
    m.addInstance(inst);
    return m;
}

const std::vector<model::Action>& playActions(const Model& m)
{
    return m.instances().values()[0].rules[0].actions;
}

} // namespace

TEST_CASE("Fișiere .aion: acțiunile își păstrează sensul și repetarea", "[core][io]")
{
    const Model m = playingModel();
    const io::LoadResult loaded = io::fromJson(io::toJson(m));
    INFO(loaded.error);
    REQUIRE(loaded.model);
    const std::vector<model::Action>& actions = playActions(*loaded.model);
    REQUIRE(actions.size() == 2);
    CHECK(actions[0].direction == model::PlayDirection::Toggle);
    CHECK(actions[0].repeat == 0);
    CHECK(actions[1].direction == model::PlayDirection::Reverse);
    CHECK(actions[1].repeat == 3);
    CHECK(loaded.model->sameAs(m));
}

TEST_CASE("Fișiere .aion: un fișier v4 dă Înainte / O dată", "[core][io]")
{
    CHECK(io::kFormatVersion == 6);
    std::string text = io::toJson(playingModel());
    const std::string current = "\"formatVersion\": 6";
    REQUIRE(text.find(current) != std::string::npos);
    text.replace(text.find(current), current.size(), "\"formatVersion\": 4");
    text = std::regex_replace(text, std::regex(R"("direction": \d+,)"), "");
    text = std::regex_replace(text, std::regex(R"("repeat": \d+,)"), "");
    REQUIRE(text.find("\"direction\"") == std::string::npos);
    REQUIRE(text.find("\"repeat\"") == std::string::npos);
    const io::LoadResult loaded = io::fromJson(text);
    INFO(loaded.error);
    REQUIRE(loaded.model);
    for (const model::Action& a : playActions(*loaded.model)) {
        CHECK(a.direction == model::PlayDirection::Forward);
        CHECK(a.repeat == 1);
    }
}

// --- Datele de mobilier (ADR 0044) ---

namespace {

// O placă etichetată, un decor cu categorie, un corp salvat (cheie) și o piesă cu rol.
Model furnitureModel()
{
    Model m;
    model::Material decor { "Nuc (decor)", { 104, 70, 46, 255 } };
    decor.category = "board-decor";
    decor.libraryKey = "nuc-decor";
    const model::MaterialId decorId = m.addMaterial(decor);

    model::Definition board { "Placă", false };
    board.generator = "board";
    board.parameters = { { "length", 600.0 }, { "width", 400.0 } };
    model::PartSpec spec;
    spec.typeKey = "pal";
    spec.nominalThickness = 18.0;
    spec.grainAxis = 0;
    spec.bands[0] = { "abs.2", "" };
    spec.bands[3] = { "abs.0.4", "alb-decor" };
    board.part = spec;
    const model::DefinitionId boardId = m.addDefinition(board);
    geom::buildPrimitive(m.mesh(boardId), geom::Primitive::Box, { 600, 400, 18 }, 4);

    model::Definition body { "Corp", false };
    body.sourceKey = "4f6c2a1e-0000-4000-8000-000000000001";
    const model::DefinitionId bodyId = m.addDefinition(body);

    model::Instance shelf;
    shelf.parent = bodyId;
    shelf.definition = boardId;
    shelf.material = decorId;
    shelf.roleKey = "space.1.shelf.1";
    shelf.nameKey = "part.shelf";
    shelf.relativePosition = 0.25;
    m.addInstance(shelf);

    model::Instance placed;
    placed.parent = m.root();
    placed.definition = bodyId;
    m.addInstance(placed);

    model::Definition hinge { "Balama", false };
    model::PartSpec hw;
    hw.kind = model::PartSpec::Kind::Hardware;
    hw.typeKey = "hinge.overlay.110";
    hinge.part = hw;
    m.addDefinition(hinge);
    return m;
}

} // namespace

TEST_CASE("Fișiere .aion v6: datele de piesă, cheile și categoriile se păstrează", "[core][io][furniture]")
{
    const Model original = furnitureModel();
    const io::LoadResult loaded = io::readArchive(io::writeArchive(original));
    INFO(loaded.error);
    REQUIRE(loaded.model);
    CHECK(loaded.model->sameAs(original));
    CHECK(io::toJson(original).find("\"formatVersion\": 6") != std::string::npos);
}

TEST_CASE("Fișiere .aion v6: un fișier v5 se deschide fără date de piesă", "[core][io][furniture]")
{
    std::string text = io::toJson(sampleModel());
    const std::string current = "\"formatVersion\": 6";
    REQUIRE(text.find(current) != std::string::npos);
    text.replace(text.find(current), current.size(), "\"formatVersion\": 5");
    const io::LoadResult loaded = io::fromJson(text);
    INFO(loaded.error);
    REQUIRE(loaded.model);
    for (const model::Definition& d : loaded.model->definitions().values()) {
        CHECK_FALSE(d.part);
        CHECK(d.sourceKey.empty());
    }
    for (const model::Instance& i : loaded.model->instances().values()) {
        CHECK(i.roleKey.empty());
        CHECK_FALSE(i.relativePosition);
    }

    // Un fișier dintr-o versiune viitoare e refuzat cu mesajul clar.
    const io::LoadResult newer = io::fromJson(R"({"format":"aion","formatVersion":7})");
    CHECK_FALSE(newer.model);
    CHECK(newer.error.find("mai nouă") != std::string::npos);
}

TEST_CASE("Fișiere .aion v6: un tip de piesă necunoscut se scrie înapoi neschimbat", "[core][io][furniture]")
{
    std::string text = io::toJson(furnitureModel());
    const std::string kind = "\"kind\": \"hardware\"";
    REQUIRE(text.find(kind) != std::string::npos);
    text.replace(text.find(kind), kind.size(), "\"kind\": \"accessory\"");
    const std::string type = "\"type\": \"pal\"";
    REQUIRE(text.find(type) != std::string::npos);
    text.replace(text.find(type), type.size(), "\"type\": \"compact-hpl\"");
    const io::LoadResult loaded = io::fromJson(text);
    INFO(loaded.error);
    REQUIRE(loaded.model);
    const std::string again = io::toJson(*loaded.model);
    CHECK(again.find("\"kind\": \"accessory\"") != std::string::npos);
    CHECK(again.find("\"type\": \"compact-hpl\"") != std::string::npos);
}

TEST_CASE("Datele de piesă: Salvează în bibliotecă și inserarea le păstrează", "[core][io][furniture]")
{
    Model m = furnitureModel();
    model::InstanceId placed;
    for (model::InstanceId id : m.childrenOf(m.root())) {
        placed = id;
    }
    REQUIRE(placed);
    const Model extracted = model::extractInstance(m, placed);
    CHECK(extracted.definition(extracted.root()).sourceKey == "4f6c2a1e-0000-4000-8000-000000000001");
    Model target;
    const model::InstanceId inserted
        = model::insertModel(target, target.root(), extracted, "Corp", Transform::translation({ 0, 0, 0 }));
    const model::Definition& def = target.definition(target.instance(inserted).definition);
    CHECK(def.sourceKey == "4f6c2a1e-0000-4000-8000-000000000001");
    const std::vector<model::InstanceId> pieces = target.childrenOf(target.instance(inserted).definition);
    REQUIRE(pieces.size() == 1);
    const model::Instance& shelf = target.instance(pieces.front());
    CHECK(shelf.roleKey == "space.1.shelf.1");
    const model::Definition& boardDef = target.definition(shelf.definition);
    REQUIRE(boardDef.part);
    CHECK(boardDef.part->typeKey == "pal");
    CHECK(boardDef.part->bands[3].bandKey == "abs.0.4");
    CHECK(boardDef.generator == "board");
}
