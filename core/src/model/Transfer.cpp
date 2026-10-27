#include "aion/core/model/Transfer.h"

#include "aion/core/model/Operations.h"

#include <glm/geometric.hpp>

#include <map>

namespace aion::model {

namespace {

class Copier {
public:
    Copier(Model& target, const Model& source)
        : m_target(target)
        , m_source(source)
    {
    }

    TextureId texture(TextureId id)
    {
        const Texture* t = id ? m_source.textures().find(id) : nullptr;
        if (t == nullptr) {
            return {};
        }
        if (const auto it = m_textures.find(id); it != m_textures.end()) {
            return it->second;
        }
        for (std::size_t i = 0; i < m_target.textures().size(); ++i) {
            const Texture& mine = m_target.textures().values()[i];
            if (mine.source == t->source && mine.data == t->data) {
                return m_textures[id] = m_target.textures().keys()[i];
            }
        }
        return m_textures[id] = m_target.addTexture(*t);
    }

    MaterialId material(MaterialId id)
    {
        const Material* m = id ? m_source.materials().find(id) : nullptr;
        if (m == nullptr) {
            return {};
        }
        if (const auto it = m_materials.find(id); it != m_materials.end()) {
            return it->second;
        }
        // Același nume în modelul țintă = același material (ca la bibliotecă).
        for (std::size_t i = 0; i < m_target.materials().size(); ++i) {
            if (m_target.materials().values()[i].name == m->name) {
                return m_materials[id] = m_target.materials().keys()[i];
            }
        }
        Material copy = *m;
        copy.texture = texture(m->texture);
        return m_materials[id] = m_target.addMaterial(copy);
    }

    // Geometria definiției sursă, copiată în definiția țintă.
    void mesh(DefinitionId from, DefinitionId to)
    {
        const geom::Mesh& src = m_source.mesh(from);
        geom::Mesh& dst = m_target.mesh(to);
        std::map<geom::VertexId, geom::VertexId> vertices;
        for (std::size_t i = 0; i < src.vertices().size(); ++i) {
            vertices[src.vertices().keys()[i]] = dst.addVertex(src.vertices().values()[i].position);
        }
        std::map<geom::EdgeId, geom::EdgeId> edges;
        for (std::size_t i = 0; i < src.edges().size(); ++i) {
            const geom::Edge& e = src.edges().values()[i];
            const geom::EdgeId id = dst.addEdge(vertices.at(e.v0), vertices.at(e.v1));
            dst.setEdgeStyle(id, e.soft, e.curve, e.smoothCurve);
            edges[src.edges().keys()[i]] = id;
        }
        const auto loop = [&](const geom::Loop& l) {
            geom::Loop out;
            for (const geom::EdgeUse& u : l.uses) {
                out.uses.push_back({ edges.at(u.edge), u.reversed });
            }
            return out;
        };
        for (const geom::Face& f : src.faces().values()) {
            std::vector<geom::Loop> inner;
            for (const geom::Loop& h : f.inner) {
                inner.push_back(loop(h));
            }
            const geom::FaceId id = dst.addFace(f.plane, loop(f.outer), std::move(inner), material(f.material),
                material(f.backMaterial));
            dst.setFaceTexture(id, f.texture);
        }
    }

    // Definiția sursă, copiată o singură dată (copiile unei componente rămân legate și în țintă).
    DefinitionId definition(DefinitionId from)
    {
        if (const auto it = m_definitions.find(from); it != m_definitions.end()) {
            return it->second;
        }
        const DefinitionId to = m_target.addDefinition(m_source.definition(from));
        m_definitions[from] = to;
        contents(from, to);
        return to;
    }

    // Geometria și obiectele dinăuntru.
    void contents(DefinitionId from, DefinitionId to)
    {
        mesh(from, to);
        for (InstanceId child : m_source.childrenOf(from)) {
            Instance inst = m_source.instance(child);
            inst.parent = to;
            inst.definition = definition(inst.definition);
            inst.material = material(inst.material);
            inst.tag = {};
            inst.rules.clear(); // regulile țintesc obiecte din modelul sursă
            m_target.addInstance(inst);
        }
    }

private:
    Model& m_target;
    const Model& m_source;
    std::map<MaterialId, MaterialId> m_materials;
    std::map<TextureId, TextureId> m_textures;
    std::map<DefinitionId, DefinitionId> m_definitions;
};

} // namespace

InstanceId insertModel(Model& target, DefinitionId context, const Model& source, const std::string& name,
    const math::Transform& placement)
{
    Copier copier(target, source);
    Definition def = source.definition(source.root());
    def.name = name;
    def.group = false; // componentă: copiile rămân legate
    const DefinitionId d = target.addDefinition(def);
    copier.contents(source.root(), d);
    Instance inst;
    inst.parent = context;
    inst.definition = d;
    inst.transform = placement;
    inst.name = name;
    return target.addInstance(inst);
}

Model extractInstance(const Model& source, InstanceId instance)
{
    Model out;
    const Instance& inst = source.instance(instance);
    // Un model intermediar cu obiectul în origine, apoi conținutul lui devine rădăcina modelului nou.
    Copier copier(out, source);
    Definition root = out.definition(out.root());
    const Definition& def = source.definition(inst.definition);
    root.generator = def.generator;
    root.parameters = def.parameters;
    root.shape = def.shape;
    out.setDefinition(out.root(), root);
    copier.contents(inst.definition, out.root());
    // Colțul minim în origine.
    const auto [lo, hi] = bounds(out, out.root());
    if (lo.x <= hi.x && (lo.x != 0.0 || lo.y != 0.0 || lo.z != 0.0)) {
        const math::Transform shift = math::Transform::translation(-lo);
        geom::Mesh& mesh = out.mesh(out.root());
        for (std::size_t i = 0; i < mesh.vertices().size(); ++i) {
            mesh.setPosition(mesh.vertices().keys()[i], shift.applyToPoint(mesh.vertices().values()[i].position));
        }
        for (std::size_t i = 0; i < mesh.faces().size(); ++i) {
            const geom::FaceId f = mesh.faces().keys()[i];
            math::Plane plane = mesh.face(f).plane;
            plane.d = glm::dot(plane.normal, shift.applyToPoint(plane.normal * plane.d));
            mesh.setFacePlane(f, plane);
        }
        for (InstanceId child : out.childrenOf(out.root())) {
            Instance c = out.instance(child);
            c.transform = shift * c.transform;
            out.setInstance(child, c);
        }
    }
    return out;
}

} // namespace aion::model
