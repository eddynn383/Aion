#include "forma/core/commands/CommandStack.h"

#include <catch2/catch_test_macros.hpp>

using namespace forma::geom;
using forma::commands::CommandStack;

namespace {

EdgeId addSegment(Mesh& mesh, Vec3d a, Vec3d b)
{
    return mesh.addEdge(mesh.addVertex(a), mesh.addVertex(b));
}

} // namespace

TEST_CASE("CommandStack: undo și redo în ordine", "[core][commands]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    stack.setValidation(true, ValidationLevel::Full);

    const Mesh empty = mesh;
    stack.run("Line 1", [](Mesh& m) { addSegment(m, { 0, 0, 0 }, { 100, 0, 0 }); });
    const Mesh one = mesh;
    stack.run("Line 2", [](Mesh& m) { addSegment(m, { 0, 50, 0 }, { 100, 50, 0 }); });
    const Mesh two = mesh;

    CHECK(stack.undoName() == "Line 2");
    REQUIRE(stack.undo());
    CHECK(mesh.sameAs(one));
    CHECK(stack.redoName() == "Line 2");
    REQUIRE(stack.undo());
    CHECK(mesh.sameAs(empty));
    CHECK_FALSE(stack.undo());

    REQUIRE(stack.redo());
    REQUIRE(stack.redo());
    CHECK(mesh.sameAs(two));
    CHECK_FALSE(stack.redo());
}

TEST_CASE("CommandStack: o comandă nouă golește redo", "[core][commands]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    stack.run("A", [](Mesh& m) { addSegment(m, { 0, 0, 0 }, { 100, 0, 0 }); });
    stack.undo();
    CHECK(stack.canRedo());
    stack.run("B", [](Mesh& m) { addSegment(m, { 0, 0, 10 }, { 100, 0, 10 }); });
    CHECK_FALSE(stack.canRedo());
    CHECK(stack.undoCount() == 1);
}

TEST_CASE("CommandStack: abort anulează tranzacția, iar o tranzacție goală nu intră în istoric",
    "[core][commands]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    stack.run("A", [](Mesh& m) { addSegment(m, { 0, 0, 0 }, { 100, 0, 0 }); });
    const Mesh before = mesh;

    stack.beginTransaction("Move");
    addSegment(mesh, { 0, 0, 50 }, { 100, 0, 50 });
    CHECK(stack.inTransaction());
    stack.abort();
    CHECK_FALSE(stack.inTransaction());
    CHECK(mesh.sameAs(before));
    CHECK(stack.undoCount() == 1);

    CHECK_FALSE(stack.run("Nimic", [](Mesh&) {}));
    CHECK(stack.undoCount() == 1);
}

TEST_CASE("CommandStack: validarea după comandă raportează un model stricat", "[core][commands]")
{
    Mesh mesh;
    CommandStack stack(mesh);
    int failures = 0;
    stack.setValidation(true);
    stack.setOnInvalid([&](const ValidationReport&) { ++failures; });

    stack.run("Vârf izolat", [](Mesh& m) { m.addVertex({ 0, 0, 0 }); });
    CHECK(failures == 1);
    stack.undo();
    CHECK(failures == 1); // după undo modelul e din nou valid
}
