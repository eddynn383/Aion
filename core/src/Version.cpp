#include "aion/core/Version.h"

#include "VersionConfig.h"

namespace aion {

Version version() noexcept
{
    return { AION_VERSION_MAJOR, AION_VERSION_MINOR, AION_VERSION_PATCH };
}

std::string_view versionString() noexcept
{
    return AION_VERSION_STRING;
}

} // namespace aion
