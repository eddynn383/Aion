#pragma once

#include <compare>
#include <cstdint>
#include <functional>

namespace aion::geom {

// ID stabil al unei entități. Valoarea 0 înseamnă „niciun ID”.
// Tag-ul împiedică amestecarea tipurilor (un VertexId nu poate fi folosit ca EdgeId).
template <class Tag>
struct Id {
    std::uint64_t value = 0;

    [[nodiscard]] constexpr bool valid() const { return value != 0; }
    constexpr explicit operator bool() const { return valid(); }
    constexpr auto operator<=>(const Id&) const = default;
};

struct VertexTag;
struct EdgeTag;
struct FaceTag;

using VertexId = Id<VertexTag>;
using EdgeId = Id<EdgeTag>;
using FaceId = Id<FaceTag>;

} // namespace aion::geom

template <class Tag>
struct std::hash<aion::geom::Id<Tag>> {
    std::size_t operator()(const aion::geom::Id<Tag>& id) const noexcept
    {
        return std::hash<std::uint64_t> {}(id.value);
    }
};
