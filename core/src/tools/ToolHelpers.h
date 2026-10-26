#pragma once

#include "aion/core/tools/Tool.h"

#include <memory>

// Funcții comune uneltelor (folosite numai în core/src/tools).
namespace aion::tools {

// Distanța minimă (în pixeli) de la care o apăsare devine tragere.
inline constexpr double kDragThresholdPx = 4.0;

// Ghidajele și marcajul inferenței, adăugate în overlay.
void appendInference(Overlay& o, const inference::Inference& inf);
// Ce se evidențiază sub cursor: muchia sau fața pe care stă punctul inferat (capetele au marcajul lor).
[[nodiscard]] view::PickResult inferenceHover(const inference::Inference& inf);
// Săgețile: → axa roșie, ← verde, ↑ albastră, ↓ paralel cu muchia de referință.
bool handleArrow(inference::Engine& engine, Key key);
// Shift ținut apăsat blochează inferența curentă; eliberat, o deblochează (doar lock-ul pus de el).
void updateShiftLock(inference::Engine& engine, bool shift, bool& shiftLock);

[[nodiscard]] std::unique_ptr<Tool> makeMoveTool();
[[nodiscard]] std::unique_ptr<Tool> makeRotateTool();
[[nodiscard]] std::unique_ptr<Tool> makeScaleTool();
[[nodiscard]] std::unique_ptr<Tool> makeCircleTool();
[[nodiscard]] std::unique_ptr<Tool> makePolygonTool();
[[nodiscard]] std::unique_ptr<Tool> makeArcTool();

} // namespace aion::tools
