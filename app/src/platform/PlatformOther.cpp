#include "Platform.h"

namespace platform {

bool prefersReducedMotion()
{
    return false;
}

void integrateTitleBar(QWindow* /*window*/, int /*titleBarHeight*/)
{
}

} // namespace platform
