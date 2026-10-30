#pragma once

#include "aion/core/tools/Tool.h"

#include <memory>

// Funcții comune uneltelor (folosite numai în core/src/tools).
namespace aion::tools {

// Distanța minimă (în pixeli) de la care o apăsare devine tragere.
inline constexpr double kDragThresholdPx = 4.0;

// Inferența cu ghidajele, grid-ul și scena contextului.
[[nodiscard]] inference::Inference inferAt(inference::Engine& engine, const ToolContext& ctx, Vec2d screen,
    std::optional<Vec3d> from, std::optional<math::Plane> plane = std::nullopt);
// Normala feței de sub inferență: `inf.normal`, altfel fața din ctx.mesh (`inf.face` / `inf.pick.face`),
// altfel `fallback`.
[[nodiscard]] Vec3d inferenceNormal(const ToolContext& ctx, const inference::Inference& inf, Vec3d fallback);
// Ce se evidențiază sub cursor: muchia sau fața pe care stă punctul inferat (capetele au marcajul lor).
[[nodiscard]] view::PickResult inferenceHover(const inference::Inference& inf);
// Săgețile: → axa roșie, ← verde, ↑ albastră, ↓ paralel cu muchia de referință.
bool handleArrow(inference::Engine& engine, Key key);
// Shift ținut apăsat blochează inferența curentă; eliberat, o deblochează (doar lock-ul pus de el).
void updateShiftLock(inference::Engine& engine, bool shift, bool& shiftLock);

[[nodiscard]] std::unique_ptr<Tool> makeMoveTool();
[[nodiscard]] std::unique_ptr<Tool> makeRotateTool();
[[nodiscard]] std::unique_ptr<Tool> makeScaleTool();
[[nodiscard]] std::unique_ptr<Tool> makePivotTool();
[[nodiscard]] std::unique_ptr<Tool> makePlaceTool();
[[nodiscard]] std::unique_ptr<Tool> makeDividerTool();
[[nodiscard]] std::unique_ptr<Tool> makeStretchTool();
[[nodiscard]] std::unique_ptr<Tool> makePenTool();
[[nodiscard]] std::unique_ptr<Tool> makePenEditTool();
[[nodiscard]] std::unique_ptr<Tool> makeCircleTool();
[[nodiscard]] std::unique_ptr<Tool> makePolygonTool();
[[nodiscard]] std::unique_ptr<Tool> makeArcTool();
// Cub, cilindru, con, sferă (id-urile „cube”, „cylinder”, „cone”, „sphere”); nullptr pentru alt id.
[[nodiscard]] std::unique_ptr<Tool> makePrimitiveTool(std::string_view id);
[[nodiscard]] std::unique_ptr<Tool> makeOffsetTool();
[[nodiscard]] std::unique_ptr<Tool> makeFollowMeTool();
[[nodiscard]] std::unique_ptr<Tool> makePaintTool();
[[nodiscard]] std::unique_ptr<Tool> makeTextureTool();
[[nodiscard]] std::unique_ptr<Tool> makeTapeMeasureTool();
[[nodiscard]] std::unique_ptr<Tool> makeProtractorTool();
[[nodiscard]] std::unique_ptr<Tool> makeDimensionTool();
[[nodiscard]] std::unique_ptr<Tool> makeTextTool();
[[nodiscard]] std::unique_ptr<Tool> makeSectionTool();

} // namespace aion::tools
