#include "aion/core/text/FuzzyMatch.h"

#include <catch2/catch_test_macros.hpp>

using aion::text::fuzzyMatch;

TEST_CASE("Interogarea goală se potrivește cu orice", "[core][text][fuzzy]")
{
    const auto m = fuzzyMatch("", "Vedere de sus");
    REQUIRE(m.has_value());
    CHECK(m->score == 0);
    CHECK(m->positions.empty());
}

TEST_CASE("Literele trebuie să apară în ordine", "[core][text][fuzzy]")
{
    CHECK(fuzzyMatch("vds", "Vedere de sus").has_value());
    CHECK(fuzzyMatch("VDS", "Vedere de sus").has_value());
    CHECK_FALSE(fuzzyMatch("sdv", "Vedere de sus").has_value());
    CHECK_FALSE(fuzzyMatch("xyz", "Vedere de sus").has_value());
    CHECK_FALSE(fuzzyMatch("vedere de sus și mai mult", "Vedere").has_value());
}

TEST_CASE("Diacriticele și spațiile din interogare sunt ignorate", "[core][text][fuzzy]")
{
    CHECK(fuzzyMatch("comuta", "Comută tema").has_value());
    CHECK(fuzzyMatch("comută", "Comuta tema").has_value());
    CHECK(fuzzyMatch("ascunde panouri", "Ascunde panourile").has_value());
    CHECK(fuzzyMatch("st", "Șterge tot").has_value());
}

TEST_CASE("Pozițiile potrivite sunt în code point-uri", "[core][text][fuzzy]")
{
    // „ă” ocupă doi octeți în UTF-8, dar un singur caracter în UI.
    const auto m = fuzzyMatch("at", "Comută tema");
    REQUIRE(m.has_value());
    REQUIRE(m->positions.size() == 2);
    CHECK(m->positions[0] == 5); // ă
    CHECK(m->positions[1] == 7); // t din „tema”, început de cuvânt
}

TEST_CASE("Începutul de cuvânt și potrivirile consecutive au prioritate", "[core][text][fuzzy]")
{
    const auto prefix = fuzzyMatch("zoom", "Zoom Extents");
    const auto middle = fuzzyMatch("zoom", "Ultimul zoom");
    const auto scattered = fuzzyMatch("zoom", "Azot comun");
    REQUIRE(prefix.has_value());
    REQUIRE(middle.has_value());
    REQUIRE(scattered.has_value());
    CHECK(prefix->score > middle->score);
    CHECK(middle->score > scattered->score);

    // Inițialele cuvintelor bat literele din mijlocul unui cuvânt.
    const auto initials = fuzzyMatch("ze", "Zoom Extents");
    const auto inside = fuzzyMatch("ze", "Bazele");
    REQUIRE(initials.has_value());
    REQUIRE(inside.has_value());
    CHECK(initials->score > inside->score);
}

TEST_CASE("UTF-8 invalid nu strică potrivirea", "[core][text][fuzzy]")
{
    const std::string broken = std::string("ab") + static_cast<char>(0xC3);
    CHECK(fuzzyMatch("ab", broken).has_value());
    CHECK_FALSE(fuzzyMatch("abc", broken).has_value());
}
