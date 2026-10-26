#include "aion/core/geom/Operations.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/io/Exchange.h"
#include "aion/core/model/Operations.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <cmath>
#include <filesystem>
#include <numbers>

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
    const model::InstanceId copy = model::copyInstance(m, stool, Transform::translation({ 0, 1500, 0 }));
    const model::TagId furniture = m.addTag({ "Mobilier", { 245, 158, 11, 255 }, true });
    model::Instance inst = m.instance(copy);
    inst.tag = furniture;
    inst.locked = true;
    inst.name = "Taburet 2";
    m.setInstance(copy, inst);
    m.addTag({ "Ascuns", { 16, 185, 129, 255 }, false });
    // Materiale: o față a casei și un taburet întreg.
    const model::MaterialId brick = m.addMaterial({ "Cărămidă", { 168, 82, 58, 255 }, 0.9f, 0.0f });
    const model::MaterialId glass = m.addMaterial({ "Sticlă", { 170, 205, 225, 90 }, 0.05f, 0.0f });
    root.setFaceMaterial(faceAt(root, { 0, 1000, 1000 }), brick);
    inst = m.instance(stool);
    inst.material = glass;
    m.setInstance(stool, inst);
    // Ghidaje, adnotări, o secțiune și o scenă.
    m.addGuide({ m.root(), { 0, 0, 0 }, { 1, 0, 0 } });
    m.addGuide({ m.root(), { 1000, 1000, 0 }, { 0, 0, 0 } });
    m.addAnnotation({ model::Annotation::Kind::Dimension, m.root(), { 0, 0, 0 }, { 6000, 0, 0 }, { 0, -500, 0 }, "" });
    m.addAnnotation({ model::Annotation::Kind::Text, m.root(), { 3000, 0, 1000 }, {}, { 0, -800, 600 }, "Ușa" });
    m.addSection({ "Secțiune 1", { 3000, 2000, 1200 }, { 0, 0, 1 }, false });
    m.addScene({ "Fațada", { 3000, 2000, 1000 }, 0.4, 0.2, 15000, false, "clay" });
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
    CHECK(io::exportMtl().find("newmtl aion_default") != std::string::npos);

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
    const Model original = sampleModel();
    const auto path = tempPath("aion-export.glb");
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
}
