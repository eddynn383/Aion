#pragma once

// Setări de sistem citite din API-urile native.
namespace platform {

// „Reduce Motion” din setările de accesibilitate (macOS). Pe celelalte platforme: false.
bool prefersReducedMotion();

} // namespace platform
