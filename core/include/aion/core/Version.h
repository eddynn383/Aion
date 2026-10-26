#pragma once

#include <string_view>

namespace aion {

struct Version {
    int major;
    int minor;
    int patch;
};

Version version() noexcept;
std::string_view versionString() noexcept;

} // namespace aion
