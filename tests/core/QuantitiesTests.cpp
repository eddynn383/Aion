#include "aion/core/geom/Operations.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Quantities.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <numbers>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

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

// O cutie dx × dy × dz cu colțul în `o`, transformată imediat în grup (sau obiect) cu numele dat.
model::InstanceId box(Model& m, Vec3d o, Vec3d size, const std::string& name, bool component = false)
{
    geom::Mesh& root = m.mesh(m.root());
    geom::drawPolygon(root, { o, o + Vec3d { size.x, 0, 0 }, o + Vec3d { size.x, size.y, 0 }, o + Vec3d { 0, size.y, 0 } });
    geom::pushPull(root, faceAt(root, o + Vec3d { size.x / 2, size.y / 2, 0 }), -size.z);
    geom::Selection all;
    for (geom::FaceId f : root.faces().keys()) {
        all.faces.insert(f);
    }
    return model::makeGroup(m, m.root(), all, component, name);
}

} // namespace

TEST_CASE("Raport de cantități: arii, volume, materiale, copii", "[core][model][quantities]")
{
    Model m;
    const auto block = box(m, { 0, 0, 0 }, { 2000, 1000, 500 }, "Bloc");
    const model::MaterialId oak = m.addMaterial({ "Stejar", { 160, 120, 80, 255 }, 0.6f, 0.0f });
    model::Instance inst = m.instance(block);
    inst.material = oak;
    m.setInstance(block, inst);
    const auto stool = box(m, { 5000, 0, 0 }, { 400, 400, 450 }, "Taburet", true);
    model::copyLinked(m, stool, Transform::translation({ 1000, 0, 0 }));

    const model::QuantityReport r = model::quantityReport(m);
    REQUIRE(r.objects.size() == 2);
    const model::ObjectQuantity& b = r.objects[0].name == "Bloc" ? r.objects[0] : r.objects[1];
    const model::ObjectQuantity& t = r.objects[0].name == "Bloc" ? r.objects[1] : r.objects[0];
    CHECK(b.solid);
    CHECK(b.volume == Approx(2000.0 * 1000 * 500));
    CHECK(b.area == Approx(2 * (2000.0 * 1000 + 2000.0 * 500 + 1000.0 * 500)));
    CHECK(b.material == "Stejar");
    CHECK(b.size.x == Approx(2000));
    CHECK(t.count == 2);
    CHECK(t.component);
    CHECK(t.volume == Approx(2 * 400.0 * 400 * 450));
    CHECK(r.totalVolume == Approx(b.volume + t.volume));
    CHECK(r.horizontalArea == Approx(2000.0 * 1000 + 2 * 400.0 * 400)); // fețele de sus
    REQUIRE_FALSE(r.materials.empty());
    CHECK(r.materials.front().name == "Stejar"); // cea mai mare arie primul

    const std::string csv = model::toCsv(r);
    CHECK(csv.rfind("\xEF\xBB\xBF", 0) == 0);
    CHECK(csv.find("Bloc;Grup;1;Stejar") != std::string::npos);
    CHECK(csv.find("1,0000") != std::string::npos); // volumul blocului în m³, cu virgulă zecimală
}

TEST_CASE("Lista de debitare: dimensiunile pieselor, piese identice, necesar de plăci", "[core][model][quantities]")
{
    Model m;
    // Un corp de mobilier: două laterale (o componentă cu două copii, una rotită), un blat și un picior masiv.
    const auto side = box(m, { 0, 0, 0 }, { 18, 560, 720 }, "Lateral", true);
    const double quarter = std::numbers::pi / 2.0;
    model::copyLinked(m, side, Transform::translation({ 2000, 0, 0 }) * Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, quarter));
    box(m, { 0, 0, 720 }, { 800, 560, 18 }, "Blat");
    box(m, { 3000, 0, 0 }, { 120, 120, 700 }, "Picior");
    // O față grupată, fără grosime: apare, dar nu contează la plăci și la cant.
    {
        geom::Mesh& root = m.mesh(m.root());
        geom::drawPolygon(root, { { 5000, 0, 0 }, { 5600, 0, 0 }, { 5600, 300, 0 }, { 5000, 300, 0 } });
        geom::Selection face;
        face.faces.insert(faceAt(root, { 5300, 150, 0 }));
        model::makeGroup(m, m.root(), face, false, "Schiță");
    }

    model::CutListOptions options;
    const model::CutList list = model::cutList(m, options);
    REQUIRE(list.parts.size() == 4);
    const auto find = [&](const std::string& name) -> const model::CutPart& {
        for (const auto& p : list.parts) {
            if (p.name == name) {
                return p;
            }
        }
        FAIL("lipsește piesa " << name);
        return list.parts.front();
    };
    const model::CutPart& s = find("Lateral");
    CHECK(s.count == 2); // copia rotită are aceleași dimensiuni
    CHECK(s.length == Approx(720));
    CHECK(s.width == Approx(560));
    CHECK(s.thickness == Approx(18));
    const model::CutPart& top = find("Blat");
    CHECK(top.length == Approx(800));
    CHECK(top.thickness == Approx(18));
    const model::CutPart& leg = find("Picior");
    CHECK_FALSE(leg.panel); // 120 mm: nu e placă
    CHECK(find("Schiță").flat);
    CHECK(list.edgeBanding == Approx(2 * 2 * (720.0 + 560) + 2 * (800.0 + 560) + 2 * (700.0 + 120)));

    REQUIRE(list.sheets.size() == 1); // doar plăcile de 18 mm, din același material
    CHECK(list.sheets[0].thickness == Approx(18));
    CHECK(list.sheets[0].partsArea == Approx(2 * 720.0 * 560 + 800.0 * 560));
    CHECK(list.sheets[0].sheets == 1);

    const std::string csv = model::toCsv(list, options);
    // Formatul v1.5 (ADR 0044): secțiune; nume; tip; grosime; lungime; lățime; buc.; decor; …
    CHECK(csv.find("Dimensiuni: finite") != std::string::npos);
    CHECK(csv.find("Plăci;Lateral;;18,0;720,0;560,0;2;Implicit") != std::string::npos);
    CHECK(csv.find("Picior;;120,0;700,0;120,0;1;Implicit") != std::string::npos);
}
