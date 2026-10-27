#include "aion/core/math/Intersect.h"
#include "aion/core/model/Texturing.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <cmath>

// Poziționează textura (Position Texture): tras pe o față, textura se mută cu cursorul; Shift + tras o rotește, iar
// Cmd + tras o scalează, ambele în jurul punctului apăsat (care rămâne pe loc). O tragere = un pas de undo.
// Doar fețele din contextul editat (pentru un grup: intră în el cu dublu-click). Vezi ADR 0024.
namespace aion::tools {

namespace {

class TextureTool final : public Tool {
public:
    std::string_view id() const override { return "texture"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        const view::PickResult hit = view::pick(ctx.mesh, ctx.camera, e.screen, {});
        if (hit.kind != view::PickResult::Kind::Face || !ctx.mesh.faces().contains(hit.face)) {
            return;
        }
        const geom::Face& face = ctx.mesh.face(hit.face);
        const auto point = onPlane(ctx, face.plane, e.screen);
        if (!point) {
            return;
        }
        m_face = hit.face;
        m_plane = face.plane;
        m_start = face.texture;
        m_press = *point;
        m_pressScreen = e.screen;
        ctx.stack.beginTransaction("Poziționează textura");
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_mode = e.modifiers.shift ? Mode::Rotate : e.modifiers.command ? Mode::Scale : Mode::Move;
        if (!m_face) {
            m_hover = view::pick(ctx.mesh, ctx.camera, e.screen, {});
            if (m_hover.kind != view::PickResult::Kind::Face) {
                m_hover = {};
            }
            return;
        }
        if (!ctx.mesh.faces().contains(m_face)) {
            return;
        }
        const auto point = onPlane(ctx, m_plane, e.screen);
        if (!point) {
            return;
        }
        // Coordonatele (mm) pe planul texturii ale punctului apăsat și ale celui curent.
        const auto [u, v] = model::textureAxes(m_plane.normal);
        const math::Vec2d pressed { glm::dot(m_press, u), glm::dot(m_press, v) };
        const math::Vec2d current { glm::dot(*point, u), glm::dot(*point, v) };
        geom::TextureTransform t = m_start;
        const double dx = e.screen.x - m_pressScreen.x;
        switch (m_mode) {
        case Mode::Move:
            // Punctul de sub cursor ține de textură: deplasarea crește cu cât s-a mutat cursorul.
            t.offset = m_start.offset + (current - pressed);
            break;
        case Mode::Rotate: {
            // 0,5° pe pixel; punctul apăsat rămâne pe loc: off' = P − R(Δθ)(P − off).
            const double delta = dx * 0.5;
            t.rotation = m_start.rotation + delta;
            const double a = glm::radians(delta);
            const math::Vec2d r = pressed - m_start.offset;
            t.offset = pressed - math::Vec2d { r.x * std::cos(a) - r.y * std::sin(a), r.x * std::sin(a) + r.y * std::cos(a) };
            break;
        }
        case Mode::Scale: {
            // Exponențial (×2 la ~70 px); punctul apăsat rămâne pe loc: off' = P − k (P − off).
            const double k = std::exp(dx * 0.01);
            t.scale = m_start.scale * k;
            t.offset = pressed - (pressed - m_start.offset) * k;
            break;
        }
        }
        ctx.mesh.setFaceTexture(m_face, t);
    }

    void pointerUp(ToolContext& ctx, const PointerEvent& /*e*/) override
    {
        if (m_face) {
            ctx.stack.commit();
            m_face = {};
        }
    }

    void cancel(ToolContext& ctx) override
    {
        if (m_face) {
            ctx.stack.abort();
            m_face = {};
        }
    }

    view::PickResult hover() const override { return m_hover; }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_face) {
            o.points.push_back(m_press);
        }
        return o;
    }

    std::string status() const override
    {
        switch (m_mode) {
        case Mode::Rotate:
            return "Shift: trage stânga / dreapta ca să rotești textura în jurul punctului apăsat";
        case Mode::Scale:
            return "Cmd: trage stânga / dreapta ca să scalezi textura în jurul punctului apăsat";
        case Mode::Move:
            break;
        }
        return "Trage pe o față ca să muți textura · Shift: rotește · Cmd: scalează · pentru un grup, intră în el (dublu-click)";
    }

private:
    enum class Mode { Move, Rotate, Scale };

    static std::optional<Vec3d> onPlane(const ToolContext& ctx, const math::Plane& plane, Vec2d screen)
    {
        const math::Ray ray = ctx.camera.rayAt(screen);
        if (const auto t = math::intersectRayPlane(ray, plane)) {
            return ray.at(*t);
        }
        return std::nullopt;
    }

    geom::FaceId m_face;
    math::Plane m_plane;
    geom::TextureTransform m_start;
    Vec3d m_press { 0.0 };
    Vec2d m_pressScreen { 0.0 };
    Mode m_mode = Mode::Move;
    view::PickResult m_hover;
};

} // namespace

std::unique_ptr<Tool> makeTextureTool()
{
    return std::make_unique<TextureTool>();
}

} // namespace aion::tools
