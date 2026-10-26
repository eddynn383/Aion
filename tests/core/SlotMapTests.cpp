#include "aion/core/geom/Id.h"
#include "aion/core/geom/SlotMap.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using aion::geom::SlotMap;
using aion::geom::VertexId;

TEST_CASE("SlotMap: ID-uri stabile la inserare și ștergere", "[core][geom][slotmap]")
{
    SlotMap<VertexId, std::string> map;
    const VertexId a = map.insert("a");
    const VertexId b = map.insert("b");
    const VertexId c = map.insert("c");
    CHECK(a != b);
    CHECK(map.size() == 3);

    REQUIRE(map.erase(a));
    CHECK_FALSE(map.erase(a));
    CHECK_FALSE(map.contains(a));
    // Celelalte rămân accesibile după mutarea internă din ștergere.
    CHECK(map.at(b) == "b");
    CHECK(map.at(c) == "c");

    // ID-urile nu se refolosesc.
    const VertexId d = map.insert("d");
    CHECK(d != a);
    CHECK(d.value > c.value);
}

TEST_CASE("SlotMap: reinserare cu ID-ul vechi (pentru undo)", "[core][geom][slotmap]")
{
    SlotMap<VertexId, int> map;
    const VertexId a = map.insert(1);
    const VertexId b = map.insert(2);
    map.erase(a);
    map.insertWithId(a, 10);
    CHECK(map.at(a) == 10);
    CHECK(map.at(b) == 2);

    // Un ID mai mare decât toate cele date împinge contorul înainte.
    map.insertWithId(VertexId { 100 }, 5);
    CHECK(map.insert(6).value == 101);
}

TEST_CASE("SlotMap: cheile și valorile rămân aliniate", "[core][geom][slotmap]")
{
    SlotMap<VertexId, int> map;
    std::vector<VertexId> ids;
    for (int i = 0; i < 50; ++i) {
        ids.push_back(map.insert(i));
    }
    for (int i = 0; i < 50; i += 3) {
        map.erase(ids[static_cast<std::size_t>(i)]);
    }
    for (std::size_t i = 0; i < map.size(); ++i) {
        const VertexId key = map.keys()[i];
        CHECK(map.at(key) == map.values()[i]);
        CHECK(std::find(ids.begin(), ids.end(), key) != ids.end());
    }
    CHECK(map.find(VertexId { 9999 }) == nullptr);
}
