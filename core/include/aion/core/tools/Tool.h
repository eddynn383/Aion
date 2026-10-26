#pragma once

#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/Selection.h"
#include "aion/core/math/Vec.h"
#include "aion/core/view/Camera.h"
#include "aion/core/view/Picking.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Uneltele de modelare. Primesc evenimente abstracte (nu evenimente Qt), ca aceeași logică
// să meargă pe orice platformă (PLAN.md, regula 2 de arhitectură).
namespace aion::tools {

using math::Vec2d;
using math::Vec3d;

struct Modifiers {
    bool shift = false;
    bool command = false; // Cmd pe macOS, Ctrl în rest
    bool alt = false; // Option pe macOS
};

struct PointerEvent {
    Vec2d screen { 0.0 }; // pixeli logici, originea stânga-sus
    Modifiers modifiers;
    int clickCount = 1; // 2 = dublu-click, 3 = triplu-click
};

enum class Key { Escape, Enter, Other };

struct KeyEvent {
    Key key = Key::Other;
    Modifiers modifiers;
};

// Ce desenează unealta peste model (previzualizări), în coordonate 3D.
struct OverlayLine {
    Vec3d a;
    Vec3d b;
    bool dashed = false;
};

struct Overlay {
    std::vector<OverlayLine> lines;
    std::vector<Vec3d> points;
    // Dreptunghiul de selecție, în coordonate de ecran; `crossing` = tras de la dreapta la stânga.
    std::optional<std::pair<Vec2d, Vec2d>> selectionRect;
    bool crossing = false;
    // Muchiile marcate de unealtă (de exemplu cele pe care Eraser le va șterge).
    std::vector<geom::EdgeId> markedEdges;
};

// Tot ce poate folosi o unealtă; aplicația îl construiește și îl ține în viață.
struct ToolContext {
    geom::Mesh& mesh;
    commands::CommandStack& stack;
    geom::Selection& selection;
    const view::Camera& camera;
};

class Tool {
public:
    virtual ~Tool() = default;

    [[nodiscard]] virtual std::string_view id() const = 0;

    virtual void pointerDown(ToolContext& ctx, const PointerEvent& e) = 0;
    // Mișcarea cursorului, cu sau fără butonul apăsat.
    virtual void pointerMove(ToolContext& ctx, const PointerEvent& e) = 0;
    virtual void pointerUp(ToolContext& ctx, const PointerEvent& e) = 0;
    // Întoarce true dacă unealta a folosit tasta.
    virtual bool key(ToolContext& ctx, const KeyEvent& e);
    // Renunță la operația în curs (Esc sau schimbarea uneltei).
    virtual void cancel(ToolContext& ctx);

    [[nodiscard]] virtual Overlay overlay() const { return {}; }
    // Ce e sub cursor (pentru evidențierea la hover).
    [[nodiscard]] virtual view::PickResult hover() const { return {}; }
    // Indicația din status bar („Alege punctul de început”).
    [[nodiscard]] virtual std::string status() const = 0;
};

// "select", "line", "rectangle", "eraser"; nullptr pentru o unealtă necunoscută.
[[nodiscard]] std::unique_ptr<Tool> makeTool(std::string_view id);

// Punctul 3D de sub cursor: pe geometrie dacă există, altfel pe planul solului (Z = 0).
struct SurfacePoint {
    Vec3d point { 0.0 };
    view::PickResult pick;
};
[[nodiscard]] std::optional<SurfacePoint> surfacePoint(const ToolContext& ctx, Vec2d screen);

// Șterge selecția: fețele selectate, apoi muchiile selectate (cu fețele lor). O singură comandă.
bool deleteSelection(ToolContext& ctx);

} // namespace aion::tools
