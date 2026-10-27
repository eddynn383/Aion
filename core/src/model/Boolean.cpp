#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/model/Operations.h"

#include <manifold/manifold.h>

#include <map>

namespace aion::model {

namespace {

// Triunghiurile unei definiții (cu instanțele din ea), transformate cu `toContext`, cu vârfuri comune.
void collect(const Model& model, DefinitionId definition, const math::Transform& toContext, manifold::MeshGL64& out)
{
    const geom::Mesh& mesh = model.mesh(definition);
    std::map<geom::VertexId, std::uint64_t> index;
    const auto vertex = [&](geom::VertexId v) {
        if (const auto it = index.find(v); it != index.end()) {
            return it->second;
        }
        const math::Vec3d p = toContext.applyToPoint(mesh.position(v));
        const auto i = static_cast<std::uint64_t>(out.vertProperties.size() / 3);
        out.vertProperties.insert(out.vertProperties.end(), { p.x, p.y, p.z });
        index[v] = i;
        return i;
    };
    for (geom::FaceId f : mesh.faces().keys()) {
        const geom::Face& face = mesh.face(f);
        std::vector<geom::VertexId> ids = mesh.loopVertices(face.outer);
        for (const geom::Loop& h : face.inner) {
            const auto hv = mesh.loopVertices(h);
            ids.insert(ids.end(), hv.begin(), hv.end());
        }
        const geom::FaceTriangles tri = geom::triangulateFace(mesh, f);
        for (std::uint32_t i : tri.indices) {
            out.triVerts.push_back(vertex(ids[i]));
        }
    }
    for (InstanceId child : model.childrenOf(definition)) {
        const Instance& inst = model.instance(child);
        collect(model, inst.definition, toContext * inst.transform, out);
    }
}

} // namespace

bool isSolid(const Model& model, DefinitionId definition)
{
    const geom::Mesh& mesh = model.mesh(definition);
    if (mesh.faces().empty() && model.childrenOf(definition).empty()) {
        return false;
    }
    for (const geom::Edge& e : mesh.edges().values()) {
        if (e.faces.size() != 2) {
            return false;
        }
    }
    for (InstanceId child : model.childrenOf(definition)) {
        if (!isSolid(model, model.instance(child).definition)) {
            return false;
        }
    }
    return true;
}

BooleanResult booleanOperation(Model& model, InstanceId a, InstanceId b, BooleanKind kind)
{
    BooleanResult result;
    const Instance ia = model.instance(a);
    const Instance ib = model.instance(b);
    if (ia.parent != ib.parent) {
        result.error = "Cele două obiecte trebuie să fie în același grup.";
        return result;
    }
    if (!isSolid(model, ia.definition) || !isSolid(model, ib.definition)) {
        result.error = "Operațiile booleene cer solide (volume închise, fiecare muchie între exact două fețe).";
        return result;
    }
    manifold::MeshGL64 ma;
    manifold::MeshGL64 mb;
    collect(model, ia.definition, ia.transform, ma);
    collect(model, ib.definition, ib.transform, mb);
    ma.Merge();
    mb.Merge();
    const manifold::Manifold sa(ma);
    const manifold::Manifold sb(mb);
    if (sa.Status() != manifold::Manifold::Error::NoError || sb.Status() != manifold::Manifold::Error::NoError) {
        result.error = "Geometria nu formează un volum valid (fețe orientate greșit sau autointersecții).";
        return result;
    }
    const manifold::Manifold out = kind == BooleanKind::Union ? sa + sb : kind == BooleanKind::Subtract ? sa - sb : sa ^ sb;
    if (out.Status() != manifold::Manifold::Error::NoError) {
        result.error = "Operația booleană a eșuat.";
        return result;
    }
    if (out.IsEmpty()) {
        result.error = "Rezultatul e gol (obiectele nu se suprapun).";
        return result;
    }
    const manifold::MeshGL64 mesh = out.GetMeshGL64();

    const DefinitionId definition = model.addDefinition({ "Grup", true });
    geom::MeshBuilder builder(model.mesh(definition));
    for (std::size_t t = 0; t + 2 < mesh.triVerts.size(); t += 3) {
        std::vector<math::Vec3d> pts;
        for (int k = 0; k < 3; ++k) {
            const std::size_t v = mesh.triVerts[t + static_cast<std::size_t>(k)];
            pts.push_back({ mesh.vertProperties[v * mesh.numProp], mesh.vertProperties[v * mesh.numProp + 1],
                mesh.vertProperties[v * mesh.numProp + 2] });
        }
        builder.face(pts);
    }
    builder.finish();
    // Materialul primului obiect trece pe grupul nou.
    result.instance = model.addInstance({ ia.parent, definition, {}, {}, ia.tag, false, false, ia.material, {}, {} });
    eraseInstance(model, a);
    eraseInstance(model, b);
    return result;
}

} // namespace aion::model
