#include "forma/core/Version.h"

#include "VersionConfig.h"

namespace forma {

Version version() noexcept
{
    return { FORMA_VERSION_MAJOR, FORMA_VERSION_MINOR, FORMA_VERSION_PATCH };
}

std::string_view versionString() noexcept
{
    return FORMA_VERSION_STRING;
}

} // namespace forma
