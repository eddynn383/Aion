#include "aion/core/units/Length.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using aion::units::formatLength;
using aion::units::parseLength;
using aion::units::parseLengths;
using Catch::Approx;

TEST_CASE("Lungimi: parsare în mm, m, cm, picioare și inci", "[core][units]")
{
    CHECK(*parseLength("2500") == Approx(2500));
    CHECK(*parseLength(" 2500 ") == Approx(2500));
    CHECK(*parseLength("12.5") == Approx(12.5));
    CHECK(*parseLength(".5") == Approx(0.5));
    CHECK(*parseLength("2.5m") == Approx(2500));
    CHECK(*parseLength("2.5 m") == Approx(2500));
    CHECK(*parseLength("250cm") == Approx(2500));
    CHECK(*parseLength("25mm") == Approx(25));
    CHECK(*parseLength("-200") == Approx(-200));
    CHECK(*parseLength("3'") == Approx(914.4));
    CHECK(*parseLength("3'6\"") == Approx(1066.8));
    CHECK(*parseLength("3' 6\"") == Approx(1066.8));
    CHECK(*parseLength("3'6") == Approx(1066.8));
    CHECK(*parseLength("6\"") == Approx(152.4));
    CHECK(*parseLength("2ft") == Approx(609.6));

    CHECK_FALSE(parseLength(""));
    CHECK_FALSE(parseLength("m"));
    CHECK_FALSE(parseLength("abc"));
    CHECK_FALSE(parseLength("12x"));
    CHECK_FALSE(parseLength("1.2.3"));
    CHECK_FALSE(parseLength("3'x"));
}

TEST_CASE("Lungimi: mai multe valori pentru dreptunghi", "[core][units]")
{
    const auto two = parseLengths("3000,2000", 2);
    REQUIRE(two);
    REQUIRE(two->size() == 2);
    CHECK(*(*two)[0] == Approx(3000));
    CHECK(*(*two)[1] == Approx(2000));

    const auto mixed = parseLengths("3m; 2'", 2);
    REQUIRE(mixed);
    CHECK(*(*mixed)[0] == Approx(3000));
    CHECK(*(*mixed)[1] == Approx(609.6));

    const auto partial = parseLengths(",2000", 2);
    REQUIRE(partial);
    CHECK_FALSE((*partial)[0]);
    CHECK(*(*partial)[1] == Approx(2000));

    CHECK_FALSE(parseLengths("1,2,3", 2));
    CHECK_FALSE(parseLengths(",", 2));
    CHECK_FALSE(parseLengths("1,x", 2));
}

TEST_CASE("Lungimi: formatare", "[core][units]")
{
    CHECK(formatLength(850) == "850 mm");
    CHECK(formatLength(12.5) == "12.5 mm");
    CHECK(formatLength(0.0001) == "0 mm");
    CHECK(formatLength(4000) == "4 m");
    CHECK(formatLength(3250) == "3.25 m");
    CHECK(formatLength(1234.5678) == "1.235 m");
    CHECK(formatLength(-2500) == "-2.5 m");
}
