#include "aion/core/geom/Operations.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/io/Recovery.h"
#include "aion/core/text/Dates.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

using namespace aion;

namespace {

// Un dosar gol, doar al testului.
std::filesystem::path freshDir(const char* name)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

model::Model square()
{
    model::Model m;
    geom::drawPolygon(m.mesh(m.root()), { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    return m;
}

std::vector<std::string> sessions(const std::vector<io::RecoveryEntry>& entries)
{
    std::vector<std::string> out;
    for (const io::RecoveryEntry& e : entries) {
        out.push_back(e.session);
    }
    return out;
}

} // namespace

TEST_CASE("Informațiile copiei de recuperare trec prin JSON", "[core][io][recovery]")
{
    const io::RecoveryInfo info { "/Users/ana/Proiecte/Casă.aion", 1'790'000'000 };
    const std::optional<io::RecoveryInfo> back = io::recoveryInfoFromJson(io::recoveryInfoToJson(info));
    REQUIRE(back.has_value());
    CHECK(back->source == info.source);
    CHECK(back->savedAt == info.savedAt);

    CHECK_FALSE(io::recoveryInfoFromJson("nu e json").has_value());
    CHECK_FALSE(io::recoveryInfoFromJson(R"({"source": "a.aion"})").has_value());
    const auto unsaved = io::recoveryInfoFromJson(R"({"savedAt": 5})");
    REQUIRE(unsaved.has_value());
    CHECK(unsaved->source.empty());
}

TEST_CASE("Copiile se scriu pe sesiune și se listează cele mai noi întâi", "[core][io][recovery]")
{
    const std::filesystem::path dir = freshDir("aion-recovery-list");
    REQUIRE(io::writeRecovery(dir, "veche", square(), { "/a/veche.aion", 100 }));
    REQUIRE(io::writeRecovery(dir, "noua", square(), { "", 300 }));
    REQUIRE(io::writeRecovery(dir, "mijloc", square(), { "/a/mijloc.aion", 200 }));

    const std::vector<io::RecoveryEntry> all = io::listRecoveries(dir);
    CHECK(sessions(all) == std::vector<std::string> { "noua", "mijloc", "veche" });
    CHECK(all[0].info.source.empty());
    CHECK(all[1].info.source == "/a/mijloc.aion");
    CHECK(all[0].model == io::recoveryModelPath(dir, "noua"));

    // Copia se încarcă înapoi ca model.
    const io::LoadResult loaded = io::loadFile(all[0].model);
    REQUIRE(loaded.model.has_value());
    CHECK(loaded.model->mesh(loaded.model->root()).faces().size() == 1);

    // Sesiunea curentă și cele care încă rulează nu se oferă; `include` se cheamă o dată pe sesiune.
    std::vector<std::string> asked;
    const auto others = io::listRecoveries(dir, [&](const std::string& s) {
        asked.push_back(s);
        return s != "mijloc";
    });
    CHECK(sessions(others) == std::vector<std::string> { "noua", "veche" });
    CHECK(std::set<std::string>(asked.begin(), asked.end()).size() == 3);
    CHECK(asked.size() == 3);

    std::filesystem::remove_all(dir);
}

TEST_CASE("O copie fără informații se oferă oricum, una fără model nu", "[core][io][recovery]")
{
    const std::filesystem::path dir = freshDir("aion-recovery-partial");
    // Scrierea întreruptă între model și .json: modelul există, informațiile lipsesc.
    REQUIRE(io::saveFile(io::recoveryModelPath(dir, "fara-info"), square()));
    // Doar informațiile (modelul a fost șters de mână): nimic de recuperat.
    {
        std::ofstream out(io::recoveryInfoPath(dir, "fara-model"));
        out << io::recoveryInfoToJson({ "/a/b.aion", 50 });
    }
    // Un lock rămas de la o sesiune nu e o copie.
    {
        std::ofstream out(io::recoveryLockPath(dir, "doar-lock"));
        out << "123";
    }

    const auto entries = io::listRecoveries(dir);
    REQUIRE(entries.size() == 1);
    CHECK(entries[0].session == "fara-info");
    CHECK(entries[0].info.source.empty());
    CHECK(entries[0].info.savedAt > 0); // timpul fișierului

    io::removeRecovery(dir, "fara-info");
    io::removeRecovery(dir, "fara-model");
    CHECK(io::listRecoveries(dir).empty());
    CHECK_FALSE(std::filesystem::exists(io::recoveryInfoPath(dir, "fara-model")));
    CHECK(std::filesystem::exists(io::recoveryLockPath(dir, "doar-lock")));

    std::filesystem::remove_all(dir);
}

TEST_CASE("Un dosar de recuperare care lipsește nu are copii", "[core][io][recovery]")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aion-recovery-lipsa";
    std::filesystem::remove_all(dir);
    CHECK(io::listRecoveries(dir).empty());
    io::removeRecovery(dir, "x"); // fără efect, fără excepții
}

TEST_CASE("Copiile prea multe sau prea vechi se curăță", "[core][io][recovery]")
{
    const std::vector<io::RecoveryEntry> entries {
        { "a", {}, { "", 1000 } },
        { "b", {}, { "", 900 } },
        { "c", {}, { "", 800 } },
        { "d", {}, { "", 100 } },
    };
    CHECK(io::recoveriesToPrune(entries, 10, 10'000, 1000).empty());
    CHECK(io::recoveriesToPrune(entries, 2, 10'000, 1000) == std::vector<std::string> { "c", "d" });
    CHECK(io::recoveriesToPrune(entries, 10, 500, 1000) == std::vector<std::string> { "d" });
    CHECK(io::recoveriesToPrune({}, 0, 0, 0).empty());
}

TEST_CASE("Momentele se scriu pentru oameni", "[core][text][dates]")
{
    using text::describeTime;
    const text::LocalTime now { 2026, 10, 29, 15, 40 };
    CHECK(describeTime({ 2026, 10, 29, 15, 40 }, now) == "chiar acum");
    CHECK(describeTime({ 2026, 10, 29, 15, 35 }, now) == "acum 5 min");
    CHECK(describeTime({ 2026, 10, 29, 14, 41 }, now) == "acum 59 min");
    CHECK(describeTime({ 2026, 10, 29, 9, 5 }, now) == "azi, 09:05");
    CHECK(describeTime({ 2026, 10, 28, 23, 59 }, now) == "ieri, 23:59");
    CHECK(describeTime({ 2026, 10, 12, 8, 0 }, now) == "12 oct.");
    CHECK(describeTime({ 2026, 1, 3, 8, 0 }, now) == "3 ian.");
    CHECK(describeTime({ 2025, 9, 1, 8, 0 }, now) == "1 sept. 2025");
    // Peste miezul nopții: 23:58 de ieri e acum 3 minute, nu „ieri”.
    CHECK(describeTime({ 2026, 10, 28, 23, 58 }, { 2026, 10, 29, 0, 1 }) == "acum 3 min");
    // Peste schimbarea anului.
    CHECK(describeTime({ 2025, 12, 31, 20, 0 }, { 2026, 1, 1, 9, 0 }) == "ieri, 20:00");
    // Din viitor: data, fără „acum”.
    CHECK(describeTime({ 2026, 10, 29, 16, 0 }, now) == "azi, 16:00");
    CHECK(describeTime({ 2026, 11, 2, 16, 0 }, now) == "2 nov.");
}
