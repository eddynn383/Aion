#pragma once

#include <string_view>

namespace forma {

struct Version {
    int major;
    int minor;
    int patch;
};

Version version() noexcept;
std::string_view versionString() noexcept;

} // namespace forma
