#include "aion/core/geom/Operations.h"
#include "aion/core/inference/Inference.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/Picking.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <chrono>
#include <cstdio>
#include <random>

using namespace aion;
using geom::Mesh;
using math::Vec2d;
using math::Vec3d;

namespace {

// Un câmp de plăci separate (n × n pătrate cu laturile lor), construit direct, fără căutările lui drawPolygon:
// 100 × 100 = 10 000 de fețe și 40 000 de muchii.
Mesh plates(int n, double size, double gap)
{
    Mesh mesh;
    const auto plane = math::Plane::fromPoints({ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 });
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            const double x = i * (size + gap);
            const double y = j * (size + gap);
            const double z = ((i * 7 + j * 3) % 5) * 40.0; // înălțimi diferite: vârfuri și muchii la adâncimi diferite
            const geom::VertexId v[4] = { mesh.addVertex({ x, y, z }), mesh.addVertex({ x + size, y, z }),
                mesh.addVertex({ x + size, y + size, z }), mesh.addVertex({ x, y + size, z }) };
            geom::Loop loop;
            for (int k = 0; k < 4; ++k) {
                loop.uses.push_back({ mesh.addEdge(v[k], v[(k + 1) % 4]), false });
            }
            mesh.addFace(math::Plane::fromPoints({ x, y, z }, { x + 1, y, z }, { x, y + 1, z }).value(), loop);
        }
    }
    (void)plane;
    return mesh;
}

view::Camera cameraOver(Vec3d target, double distance)
{
    view::Camera camera;
    camera.setViewportSize(1200, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { target, 0, 0, distance }));
    return camera;
}

bool same(const view::PickResult& a, const view::PickResult& b)
{
    return a.kind == b.kind && a.vertex == b.vertex && a.edge == b.edge && a.face == b.face
        && glm::length(a.point - b.point) < 1e-6;
}

} // namespace

TEST_CASE("Indexul de picking dă aceleași rezultate ca buclele simple", "[core][view][pick]")
{
    Mesh mesh = plates(30, 300.0, 120.0);
    REQUIRE(mesh.validate(geom::ValidationLevel::Topology).ok());
    const view::Camera camera = cameraOver({ 6000, 6000, 0 }, 16000);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> x(0.0, 1200.0);
    std::uniform_real_distribution<double> y(0.0, 800.0);
    int hits = 0;
    for (int k = 0; k < 400; ++k) {
        const Vec2d screen { x(rng), y(rng) };
        view::setPickIndexThreshold(static_cast<std::size_t>(-1)); // fără index
        const view::PickResult plain = view::pick(mesh, camera, screen, { 9.0, 7.0 });
        view::setPickIndexThreshold(0); // cu index
        const view::PickResult indexed = view::pick(mesh, camera, screen, { 9.0, 7.0 });
        INFO("punctul " << screen.x << ", " << screen.y);
        CHECK(same(plain, indexed));
        hits += plain.hit() ? 1 : 0;
    }
    CHECK(hits > 100);
    // După o schimbare a geometriei, indexul se reface (vârful mutat se alege în noua poziție).
    const geom::VertexId v = mesh.vertices().keys().front();
    mesh.setPosition(v, { -500, -500, 0 });
    const auto s = camera.project({ -500, -500, 0 });
    REQUIRE(s);
    view::setPickIndexThreshold(0);
    const view::PickResult moved = view::pick(mesh, camera, *s);
    CHECK(moved.kind == view::PickResult::Kind::Vertex);
    CHECK(moved.vertex == v);
    view::setPickIndexThreshold(4000);
}

TEST_CASE("Indexul de picking: un model cu 100 000 de fețe răspunde repede", "[.perf][core][view][pick]")
{
    Mesh mesh = plates(316, 300.0, 120.0); // ~100 000 de fețe, ~400 000 de muchii
    const view::Camera camera = cameraOver({ 66000, 66000, 0 }, 60000);
    inference::Engine engine;
    const auto time = [&](const char* label, std::size_t threshold) {
        view::setPickIndexThreshold(threshold);
        const auto start = std::chrono::steady_clock::now();
        for (int k = 0; k < 20; ++k) {
            (void)view::pick(mesh, camera, { 600.0 + k, 400.0 });
            (void)engine.infer(mesh, camera, { 600.0 + k, 400.0 }, std::nullopt);
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() / 20.0;
        std::printf("%s: %.2f ms pe pick + inferență\n", label, ms);
        return ms;
    };
    view::setPickIndexThreshold(0);
    const auto buildStart = std::chrono::steady_clock::now();
    (void)view::pick(mesh, camera, { 10.0, 10.0 });
    std::printf("construcția indexului: %.0f ms\n",
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - buildStart).count());
    const double plain = time("fără index", static_cast<std::size_t>(-1));
    (void)time("index (construcție + căutare)", 0); // primul apel construiește indexul
    const double indexed = time("cu index", 0);
    CHECK(indexed * 5.0 < plain);
    view::setPickIndexThreshold(4000);
}
