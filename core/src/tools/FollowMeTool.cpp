#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Sweep.h"

#include "ToolHelpers.h"

// Follow Me: întâi se selectează traseul (muchii legate sau o față, al cărei contur devine traseul), apoi un click
// pe profil (fața de extrudat) îl duce de-a lungul traseului. Un pas de undo; o eroare nu schimbă nimic.
namespace aion::tools {

namespace {

class FollowMeTool final : public Tool {
public:
    std::string_view id() const override { return "followme"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        const view::PickResult hit = view::pick(ctx.mesh, ctx.camera, e.screen);
        if (hit.kind != view::PickResult::Kind::Face) {
            return;
        }
        const std::set<geom::EdgeId> path = pathOf(ctx, hit.face);
        if (path.empty()) {
            m_message = "Selectează întâi traseul (muchii sau o față), apoi click pe profil.";
            return;
        }
        // Încercat întâi pe o copie: o eroare nu lasă un pas gol în istoric.
        geom::Mesh trial = ctx.mesh;
        const geom::SweepResult check = geom::followMe(trial, hit.face, path);
        if (!check.ok()) {
            m_message = check.error;
            return;
        }
        geom::SweepResult done;
        ctx.stack.run("Follow Me", [&](geom::Mesh& mesh) { done = geom::followMe(mesh, hit.face, path); });
        ctx.selection = done.selection;
        m_message.clear();
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        const view::PickResult hit = view::pick(ctx.mesh, ctx.camera, e.screen);
        m_hover = hit.kind == view::PickResult::Kind::Face && !ctx.selection.faces.contains(hit.face) ? hit : view::PickResult {};
        m_hasPath = !ctx.selection.edges.empty() || !ctx.selection.faces.empty();
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    view::PickResult hover() const override { return m_hover; }

    std::string status() const override
    {
        if (!m_message.empty()) {
            return m_message;
        }
        return m_hasPath ? "Click pe profilul (fața) de extrudat de-a lungul traseului selectat"
                         : "Selectează întâi traseul: muchii legate sau o față (conturul ei), apoi click pe profil";
    }

private:
    // Traseul: muchiile selectate și conturul fețelor selectate (fără profil și conturul lui).
    static std::set<geom::EdgeId> pathOf(const ToolContext& ctx, geom::FaceId profile)
    {
        std::set<geom::EdgeId> path;
        for (geom::EdgeId e : ctx.selection.edges) {
            if (ctx.mesh.edges().contains(e)) {
                path.insert(e);
            }
        }
        for (geom::FaceId f : ctx.selection.faces) {
            if (f == profile || !ctx.mesh.faces().contains(f)) {
                continue;
            }
            for (const geom::EdgeUse& u : ctx.mesh.face(f).outer.uses) {
                path.insert(u.edge);
            }
        }
        for (const geom::EdgeUse& u : ctx.mesh.face(profile).outer.uses) {
            path.erase(u.edge);
        }
        return path;
    }

    view::PickResult m_hover;
    std::string m_message;
    bool m_hasPath = false;
};

} // namespace

std::unique_ptr<Tool> makeFollowMeTool()
{
    return std::make_unique<FollowMeTool>();
}

} // namespace aion::tools
