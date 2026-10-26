#include "forma/core/commands/CommandStack.h"
#include "forma/core/geom/Operations.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <utility>
#include <random>
#include <sstream>
#include <string>

using namespace forma::geom;
using forma::commands::CommandStack;

namespace {

// Puncte dintr-o rețea mică (pasul de 250 mm), pe câteva plane: multe coincidențe,
// suprapuneri coliniare și intersecții, adică exact cazurile care strică topologia.
class Generator {
public:
    explicit Generator(std::uint32_t seed)
        : m_rng(seed)
    {
    }

    int pick(int n) { return std::uniform_int_distribution<int>(0, n - 1)(m_rng); }

    Vec3d point(int plane)
    {
        const double a = pick(5) * 250.0;
        const double b = pick(5) * 250.0;
        switch (plane) {
        case 0:
            return { a, b, 0.0 };
        case 1:
            return { a, b, 500.0 };
        case 2:
            return { 500.0, a, b };
        default:
            return { a, 250.0, b };
        }
    }

    // inset > 0: dreptunghiul se retrage spre interior, în afara rețelei (devine des o gaură).
    std::vector<Vec3d> rectangle(int plane, double inset = 0.0)
    {
        Vec3d p = point(plane);
        Vec3d q = point(plane);
        const auto corner = [&](double u, double v) {
            switch (plane) {
            case 0:
                return Vec3d { u, v, 0.0 };
            case 1:
                return Vec3d { u, v, 500.0 };
            case 2:
                return Vec3d { 500.0, u, v };
            default:
                return Vec3d { u, 250.0, v };
            }
        };
        const auto uv = [&](const Vec3d& x) {
            switch (plane) {
            case 0:
            case 1:
                return std::pair { x.x, x.y };
            case 2:
                return std::pair { x.y, x.z };
            default:
                return std::pair { x.x, x.z };
            }
        };
        auto [u0, v0] = uv(p);
        auto [u1, v1] = uv(q);
        if (u0 > u1) {
            std::swap(u0, u1);
        }
        if (v0 > v1) {
            std::swap(v0, v1);
        }
        u0 += inset;
        v0 += inset;
        u1 -= inset;
        v1 -= inset;
        return { corner(u0, v0), corner(u1, v0), corner(u1, v1), corner(u0, v1) };
    }

private:
    std::mt19937 m_rng;
};

std::string describe(const Vec3d& p)
{
    std::ostringstream s;
    s << "(" << p.x << ", " << p.y << ", " << p.z << ")";
    return s.str();
}

std::uint32_t baseSeed()
{
    if (const char* env = std::getenv("FORMA_FUZZ_SEED")) {
        return static_cast<std::uint32_t>(std::strtoul(env, nullptr, 10));
    }
    return 20261026u;
}

} // namespace

TEST_CASE("Fuzz: operații aleatoare, validate() după fiecare și undo complet", "[core][geom][fuzz]")
{
    constexpr int kSequences = 250;
    constexpr int kOperationsPerSequence = 40; // 10 000 de operații în total

    const std::uint32_t seed0 = baseSeed();
    int totalOperations = 0;

    for (int sequence = 0; sequence < kSequences; ++sequence) {
        const std::uint32_t seed = seed0 + static_cast<std::uint32_t>(sequence);
        Generator gen(seed);
        Mesh mesh;
        CommandStack stack(mesh);
        std::string log;
        std::string failure;
        stack.setValidation(true, ValidationLevel::Full);
        stack.setOnInvalid([&](const ValidationReport& report) {
            if (failure.empty()) {
                failure = report.text();
            }
        });

        const Mesh initial = mesh;
        for (int op = 0; op < kOperationsPerSequence && failure.empty(); ++op) {
            const int kind = gen.pick(11);
            const int plane = gen.pick(4);
            if (kind < 6) {
                const Vec3d a = gen.point(plane);
                const Vec3d b = gen.point(gen.pick(3) == 0 ? gen.pick(4) : plane);
                log += "line " + describe(a) + " " + describe(b) + "\n";
                stack.run("Line", [&](Mesh& m) { drawLine(m, a, b); });
            } else if (kind < 8) {
                const auto r = gen.rectangle(plane);
                log += "rect " + describe(r[0]) + " " + describe(r[2]) + "\n";
                stack.run("Rectangle", [&](Mesh& m) { drawPolygon(m, r); });
            } else if (kind == 10) {
                const auto r = gen.rectangle(plane, 60.0);
                log += "inset rect " + describe(r[0]) + " " + describe(r[2]) + "\n";
                stack.run("Rectangle", [&](Mesh& m) { drawPolygon(m, r); });
            } else if (kind < 9 && !mesh.edges().empty()) {
                const EdgeId e = mesh.edges().keys()[static_cast<std::size_t>(gen.pick(static_cast<int>(mesh.edges().size())))];
                log += "erase edge " + std::to_string(e.value) + "\n";
                stack.run("Erase", [&](Mesh& m) { eraseEdge(m, e); });
            } else if (!mesh.faces().empty()) {
                const FaceId f = mesh.faces().keys()[static_cast<std::size_t>(gen.pick(static_cast<int>(mesh.faces().size())))];
                log += "erase face " + std::to_string(f.value) + "\n";
                stack.run("Erase face", [&](Mesh& m) { eraseFace(m, f); });
            }
            ++totalOperations;
        }

        INFO("sămânța " << seed << "\noperații:\n" << log);
        INFO(failure);
        REQUIRE(failure.empty());

        const Mesh final = mesh;
        while (stack.undo()) {
        }
        REQUIRE(failure.empty());
        REQUIRE(mesh.sameAs(initial));
        while (stack.redo()) {
        }
        REQUIRE(failure.empty());
        REQUIRE(mesh.sameAs(final));
    }
    CHECK(totalOperations >= 10000);
}
