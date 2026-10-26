#pragma once

class QWindow;

// Setări și ajustări care țin de API-urile native ale platformei.
namespace platform {

// „Reduce Motion” din setările de accesibilitate (macOS). Pe celelalte platforme: false.
bool prefersReducedMotion();

// Bara de titlu integrată: ascunde titlul desenat de sistem și centrează vertical butoanele native
// ale ferestrei (macOS) într-o bară de înălțimea dată. Pe celelalte platforme nu face nimic.
void integrateTitleBar(QWindow* window, int titleBarHeight);

} // namespace platform
