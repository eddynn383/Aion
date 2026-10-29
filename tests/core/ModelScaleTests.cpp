#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Operations.h"
#include "aion/core/view/ModelPicking.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

using namespace aion;
using math::Transform;
using math::Vec3d;
using model::DefinitionId;
using model::InstanceId;
using model::Model;

namespace {

DefinitionId boxDefinition(Model& model, const char* name)
{
    const DefinitionId d = model.addDefinition(model::Definition { name, true });
    geom::buildPrimitive(model.mesh(d), geom::Primitive::Box, { 400, 400, 400 }, 24);
    return d;
}

InstanceId place(Model& model, DefinitionId parent, DefinitionId definition, Vec3d at)
{
    model::Instance inst;
    inst.parent = parent;
    inst.definition = definition;
    inst.transform = Transform::translation(at);
    return model.addInstance(inst);
}

// Copiii calculați direct din toate instanțele (cum făcea `childrenOf` înainte de index).
std::vector<InstanceId> childrenByScan(const Model& model, DefinitionId parent)
{
    std::vector<InstanceId> out;
    for (std::size_t i = 0; i < model.instances().size(); ++i) {
        if (model.instances().values()[i].parent == parent) {
            out.push_back(model.instances().keys()[i]);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void checkChildren(const Model& model)
{
    for (const DefinitionId d : model.definitions().keys()) {
        CHECK(model.childrenOf(d) == childrenByScan(model, d));
    }
}

} // namespace

TEST_CASE("childrenOf rămâne corect după adăugare, ștergere, mutare, undo / redo și copiere", "[core][model]")
{
    Model model;
    commands::CommandStack stack { model };
    const DefinitionId box = boxDefinition(model, "Cutie");
    const DefinitionId group = model.addDefinition(model::Definition { "Grup", true });
    InstanceId a;
    InstanceId b;
    stack.run("Obiecte", [&](Model& m) {
        a = place(m, m.root(), box, { 0, 0, 0 });
        b = place(m, m.root(), box, { 1000, 0, 0 });
        place(m, group, box, { 0, 1000, 0 });
        place(m, m.root(), group, { 0, 0, 0 });
        checkChildren(m); // în aceeași tranzacție, după adăugare
    });
    checkChildren(model);
    REQUIRE(model.childrenOf(model.root()).size() == 3);

    stack.run("Mută în grup", [&](Model& m) {
        model::Instance moved = m.instance(a);
        moved.parent = group;
        m.setInstance(a, moved);
        m.removeInstance(b);
        checkChildren(m);
    });
    checkChildren(model);
    CHECK(model.childrenOf(model.root()).size() == 1);
    CHECK(model.childrenOf(group).size() == 2);

    stack.undo();
    checkChildren(model);
    CHECK(model.childrenOf(model.root()).size() == 3);
    stack.redo();
    checkChildren(model);

    const Model copy = model;
    checkChildren(copy);
    Model assigned;
    assigned = model;
    checkChildren(assigned);
}

TEST_CASE("Cutia modelului cu o definiție folosită de multe ori", "[core][model]")
{
    Model model;
    const DefinitionId box = boxDefinition(model, "Cutie");
    const DefinitionId row = model.addDefinition(model::Definition { "Rând", true });
    for (int i = 0; i < 10; ++i) {
        place(model, row, box, { i * 500.0, 0, 0 });
    }
    for (int j = 0; j < 5; ++j) {
        place(model, model.root(), row, { 0, j * 500.0, 0 });
    }
    const auto [lo, hi] = model::bounds(model, model.root());
    // Cutia primitivei e centrată pe bază: −200..200 pe X și Y, 0..400 pe Z.
    CHECK(lo == Vec3d { -200, -200, 0 });
    CHECK(hi == Vec3d { 4700, 2200, 400 });
}

TEST_CASE("Alegerea unui obiect într-un model cu 2 000 de instanțe e rapidă", "[.perf][core][model]")
{
    Model model;
    const DefinitionId box = boxDefinition(model, "Placă");
    const DefinitionId cabinet = model.addDefinition(model::Definition { "Corp", true });
    for (int k = 0; k < 7; ++k) {
        place(model, cabinet, box, { 0, 0, k * 450.0 });
    }
    for (int i = 0; i < 300; ++i) {
        place(model, model.root(), cabinet, { (i % 20) * 1500.0, (i / 20) * 1500.0, 0 });
    }
    view::Camera camera;
    camera.setViewportSize(1200, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 15000, 11000, 1500 }, 0, 0, 40000 }));
    const auto start = std::chrono::steady_clock::now();
    int hits = 0;
    for (int k = 0; k < 20; ++k) {
        hits += view::pickInstance(model, model.root(), camera, { 400.0 + k * 20, 400.0 }).has_value() ? 1 : 0;
        (void)model::bounds(model, model.root());
    }
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20.0;
    std::printf("pickInstance + bounds, 2 100 de instanțe: %.2f ms\n", ms);
    CHECK(hits > 0);
    CHECK(ms < 20.0);
}
