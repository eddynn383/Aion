#include "aion/core/geom/Operations.h"

#include "ToolHelpers.h"

// Paint Bucket (B): vopsește o față sau un grup cu materialul curent.
// Option: pipetă (ia materialul de sub cursor). Cmd: toate fețele conectate. Shift: toate fețele din context
// care au același material ca fața atinsă.
namespace aion::tools {

namespace {

class PaintTool final : public Tool {
public:
    std::string_view id() const override { return "paint"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        const view::PickResult hit = pickWithInstances(ctx, e.screen, { -1.0, -1.0 });
        if (!hit.hit() || ctx.model == nullptr) {
            return;
        }
        if (e.modifiers.alt) {
            sample(ctx, hit);
            return;
        }
        if (!ctx.paintMaterial && ctx.libraryMaterial == nullptr) {
            return;
        }
        ctx.stack.run("Vopsește", [&](model::Model& model) {
            model::MaterialId material = ctx.paintMaterial;
            if (!material) {
                material = model.addMaterial(*ctx.libraryMaterial);
                if (ctx.requests != nullptr) {
                    ctx.requests->addedMaterial = material;
                }
            }
            if (hit.kind == view::PickResult::Kind::Instance) {
                model::Instance inst = model.instance(hit.instance);
                inst.material = material;
                model.setInstance(hit.instance, inst);
                return;
            }
            geom::Mesh& mesh = model.mesh(ctx.context);
            if (!mesh.faces().contains(hit.face)) {
                return;
            }
            std::set<geom::FaceId> faces { hit.face };
            if (e.modifiers.command) {
                faces = geom::connectedTo(mesh, { {}, { hit.face }, {} }).faces;
            } else if (e.modifiers.shift) {
                const model::MaterialId old = mesh.face(hit.face).material;
                for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
                    if (mesh.faces().values()[i].material == old) {
                        faces.insert(mesh.faces().keys()[i]);
                    }
                }
            }
            for (geom::FaceId f : faces) {
                mesh.setFaceMaterial(f, material);
            }
        });
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_hover = pickWithInstances(ctx, e.screen, { -1.0, -1.0 });
        m_sampling = e.modifiers.alt;
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    view::PickResult hover() const override { return m_hover; }

    std::string status() const override
    {
        return m_sampling ? "Click pe o față pentru a-i lua materialul"
                          : "Click pe o față sau un grup pentru a-l vopsi · Option: pipetă · Cmd: tot ce e conectat · "
                            "Shift: toate fețele cu același material";
    }

private:
    static void sample(ToolContext& ctx, const view::PickResult& hit)
    {
        if (ctx.requests == nullptr) {
            return;
        }
        if (hit.kind == view::PickResult::Kind::Instance) {
            ctx.requests->sampledMaterial = ctx.model->instance(hit.instance).material;
        } else if (ctx.mesh.faces().contains(hit.face)) {
            ctx.requests->sampledMaterial = ctx.mesh.face(hit.face).material;
        }
    }

    view::PickResult m_hover;
    bool m_sampling = false;
};

} // namespace

std::unique_ptr<Tool> makePaintTool()
{
    return std::make_unique<PaintTool>();
}

} // namespace aion::tools
