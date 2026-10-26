// tinygltf fără stb_image: Aion nu încarcă încă texturi (Faza 8), iar imaginile din fișiere se ignoră.
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/io/Exchange.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <functional>
#include <map>

namespace aion::io {

namespace {

// Rotația care duce Z-sus în Y-sus: (x, y, z) → (x, z, −y).
math::Mat4d zUpToYUp()
{
    math::Mat4d m(0.0);
    m[0][0] = 1.0; // x → x
    m[1][2] = -1.0; // y → −z
    m[2][1] = 1.0; // z → y
    m[3][3] = 1.0;
    return m;
}

// Transformarea unei instanțe cu translația scalată (geometria e scalată separat, în buffer).
std::vector<double> nodeMatrix(const math::Mat4d& m, double scale)
{
    std::vector<double> out(16);
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[static_cast<std::size_t>(c * 4 + r)] = (c == 3 && r < 3) ? m[c][r] * scale : m[c][r];
        }
    }
    return out;
}

void appendBytes(std::vector<unsigned char>& buffer, const void* data, std::size_t size)
{
    const auto* p = static_cast<const unsigned char*>(data);
    buffer.insert(buffer.end(), p, p + size);
    while (buffer.size() % 4 != 0) {
        buffer.push_back(0);
    }
}

// Datele unui accessor ca float/uint (cu stride).
template <class T>
std::vector<T> readAccessor(const tinygltf::Model& gltf, int index, int components)
{
    std::vector<T> out;
    if (index < 0 || index >= static_cast<int>(gltf.accessors.size())) {
        return out;
    }
    const tinygltf::Accessor& acc = gltf.accessors[static_cast<std::size_t>(index)];
    if (acc.bufferView < 0) {
        return out;
    }
    const tinygltf::BufferView& view = gltf.bufferViews[static_cast<std::size_t>(acc.bufferView)];
    const tinygltf::Buffer& buffer = gltf.buffers[static_cast<std::size_t>(view.buffer)];
    const std::size_t componentSize = static_cast<std::size_t>(tinygltf::GetComponentSizeInBytes(static_cast<std::uint32_t>(acc.componentType)));
    const std::size_t elementSize = componentSize * static_cast<std::size_t>(components);
    const std::size_t stride = view.byteStride > 0 ? view.byteStride : elementSize;
    const std::size_t base = view.byteOffset + acc.byteOffset;
    out.reserve(acc.count * static_cast<std::size_t>(components));
    for (std::size_t i = 0; i < acc.count; ++i) {
        const unsigned char* element = buffer.data.data() + base + i * stride;
        if (base + i * stride + elementSize > buffer.data.size()) {
            break;
        }
        for (int c = 0; c < components; ++c) {
            const unsigned char* p = element + static_cast<std::size_t>(c) * componentSize;
            switch (acc.componentType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT: {
                float v;
                std::memcpy(&v, p, sizeof v);
                out.push_back(static_cast<T>(v));
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
                std::uint32_t v;
                std::memcpy(&v, p, sizeof v);
                out.push_back(static_cast<T>(v));
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                std::uint16_t v;
                std::memcpy(&v, p, sizeof v);
                out.push_back(static_cast<T>(v));
                break;
            }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                out.push_back(static_cast<T>(*p));
                break;
            default:
                return {};
            }
        }
    }
    return out;
}

math::Mat4d localMatrix(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16) {
        math::Mat4d m;
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                m[c][r] = node.matrix[static_cast<std::size_t>(c * 4 + r)];
            }
        }
        return m;
    }
    math::Mat4d t(1.0);
    if (node.translation.size() == 3) {
        t[3] = { node.translation[0], node.translation[1], node.translation[2], 1.0 };
    }
    math::Mat4d r(1.0);
    if (node.rotation.size() == 4) {
        const glm::dquat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
        r = glm::mat4_cast(q);
    }
    math::Mat4d s(1.0);
    if (node.scale.size() == 3) {
        s[0][0] = node.scale[0];
        s[1][1] = node.scale[1];
        s[2][2] = node.scale[2];
    }
    return t * r * s;
}

} // namespace

bool exportGltf(const model::Model& model, const std::filesystem::path& path, bool binary, const ExportOptions& options,
    std::string* error)
{
    tinygltf::Model gltf;
    gltf.asset.version = "2.0";
    gltf.asset.generator = "Aion";
    gltf.buffers.emplace_back();
    std::vector<unsigned char>& data = gltf.buffers[0].data;

    tinygltf::Material material;
    material.name = "aion_default";
    material.pbrMetallicRoughness.baseColorFactor = { 0.85, 0.84, 0.82, 1.0 };
    material.pbrMetallicRoughness.metallicFactor = 0.0;
    material.pbrMetallicRoughness.roughnessFactor = 0.8;
    material.doubleSided = true;
    gltf.materials.push_back(material);

    const auto addView = [&](const void* bytes, std::size_t size, int target) {
        tinygltf::BufferView view;
        view.buffer = 0;
        view.byteOffset = data.size();
        view.byteLength = size;
        view.target = target;
        appendBytes(data, bytes, size);
        gltf.bufferViews.push_back(view);
        return static_cast<int>(gltf.bufferViews.size() - 1);
    };

    // Un mesh glTF pentru fiecare definiție cu fețe (geometria în metri, în coordonatele definiției).
    std::map<model::DefinitionId, int> meshes;
    for (model::DefinitionId def : model.definitions().keys()) {
        const geom::Mesh& mesh = model.mesh(def);
        if (mesh.faces().empty()) {
            continue;
        }
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<std::uint32_t> indices;
        math::Vec3d lo { 1e300 };
        math::Vec3d hi { -1e300 };
        for (geom::FaceId f : mesh.faces().keys()) {
            const geom::FaceTriangles tri = geom::triangulateFace(mesh, f);
            const auto base = static_cast<std::uint32_t>(positions.size() / 3);
            const math::Vec3d n = mesh.face(f).plane.normal;
            for (const math::Vec3d& p : tri.points) {
                const math::Vec3d q = p * options.scale;
                lo = glm::min(lo, q);
                hi = glm::max(hi, q);
                positions.insert(positions.end(), { float(q.x), float(q.y), float(q.z) });
                normals.insert(normals.end(), { float(n.x), float(n.y), float(n.z) });
            }
            for (std::uint32_t i : tri.indices) {
                indices.push_back(base + i);
            }
        }
        const int pv = addView(positions.data(), positions.size() * sizeof(float), TINYGLTF_TARGET_ARRAY_BUFFER);
        const int nv = addView(normals.data(), normals.size() * sizeof(float), TINYGLTF_TARGET_ARRAY_BUFFER);
        const int iv = addView(indices.data(), indices.size() * sizeof(std::uint32_t), TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER);

        tinygltf::Accessor pa;
        pa.bufferView = pv;
        pa.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
        pa.count = positions.size() / 3;
        pa.type = TINYGLTF_TYPE_VEC3;
        pa.minValues = { lo.x, lo.y, lo.z };
        pa.maxValues = { hi.x, hi.y, hi.z };
        gltf.accessors.push_back(pa);
        tinygltf::Accessor na = pa;
        na.bufferView = nv;
        na.minValues.clear();
        na.maxValues.clear();
        gltf.accessors.push_back(na);
        tinygltf::Accessor ia;
        ia.bufferView = iv;
        ia.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
        ia.count = indices.size();
        ia.type = TINYGLTF_TYPE_SCALAR;
        gltf.accessors.push_back(ia);

        tinygltf::Primitive prim;
        prim.attributes["POSITION"] = static_cast<int>(gltf.accessors.size() - 3);
        prim.attributes["NORMAL"] = static_cast<int>(gltf.accessors.size() - 2);
        prim.indices = static_cast<int>(gltf.accessors.size() - 1);
        prim.material = 0;
        prim.mode = TINYGLTF_MODE_TRIANGLES;
        tinygltf::Mesh m;
        m.name = model.definition(def).name;
        m.primitives.push_back(prim);
        gltf.meshes.push_back(m);
        meshes[def] = static_cast<int>(gltf.meshes.size() - 1);
    }

    // Nodurile: rădăcina (cu rotația spre Y-sus), apoi instanțele, recursiv.
    std::function<int(model::DefinitionId, const std::string&, const std::vector<double>&)> addNode;
    addNode = [&](model::DefinitionId def, const std::string& name, const std::vector<double>& matrix) {
        tinygltf::Node node;
        node.name = name;
        node.matrix = matrix;
        if (const auto it = meshes.find(def); it != meshes.end()) {
            node.mesh = it->second;
        }
        const int index = static_cast<int>(gltf.nodes.size());
        gltf.nodes.push_back(node);
        std::vector<int> children;
        for (model::InstanceId child : model.childrenOf(def)) {
            if (!options.includeHidden && !model.isVisible(child)) {
                continue;
            }
            const model::Instance& inst = model.instance(child);
            children.push_back(addNode(inst.definition,
                inst.name.empty() ? model.definition(inst.definition).name : inst.name,
                nodeMatrix(inst.transform.matrix(), options.scale)));
        }
        gltf.nodes[static_cast<std::size_t>(index)].children = children;
        return index;
    };
    const math::Mat4d rootMatrix = options.yUp ? zUpToYUp() : math::Mat4d(1.0);
    const int root = addNode(model.root(), "Aion", nodeMatrix(rootMatrix, 1.0));
    tinygltf::Scene scene;
    scene.name = "Aion";
    scene.nodes.push_back(root);
    gltf.scenes.push_back(scene);
    gltf.defaultScene = 0;

    tinygltf::TinyGLTF writer;
    if (!writer.WriteGltfSceneToFile(&gltf, path.string(), true, true, !binary, binary)) {
        if (error != nullptr) {
            *error = "Fișierul glTF nu a putut fi scris.";
        }
        return false;
    }
    return true;
}

ImportResult importGltf(
    model::Model& model, model::DefinitionId parent, const std::filesystem::path& path, const ImportOptions& options)
{
    ImportResult result;
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    // Imaginile (texturile) nu se încarcă încă.
    loader.SetImageLoader(
        [](tinygltf::Image*, const int, std::string*, std::string*, int, int, const unsigned char*, int, void*) {
            return true;
        },
        nullptr);
    std::string err;
    std::string warn;
    const std::string ext = path.extension().string();
    const bool ok = (ext == ".glb" || ext == ".GLB") ? loader.LoadBinaryFromFile(&gltf, &err, &warn, path.string())
                                                     : loader.LoadASCIIFromFile(&gltf, &err, &warn, path.string());
    if (!ok) {
        result.error = "Fișierul glTF nu poate fi citit" + (err.empty() ? std::string(".") : ": " + err);
        return result;
    }

    // Câte noduri folosesc fiecare mesh: cele folosite de mai multe devin obiecte (copii legate).
    std::map<int, int> uses;
    for (const tinygltf::Node& n : gltf.nodes) {
        if (n.mesh >= 0) {
            ++uses[n.mesh];
        }
    }
    const math::Mat4d scaleTranslation = glm::scale(math::Mat4d(1.0), math::Vec3d(options.scale));
    std::map<int, model::DefinitionId> meshDefinitions;
    const auto meshDefinition = [&](int index) {
        if (const auto it = meshDefinitions.find(index); it != meshDefinitions.end()) {
            return it->second;
        }
        const tinygltf::Mesh& mesh = gltf.meshes[static_cast<std::size_t>(index)];
        const model::DefinitionId def = model.addDefinition(
            { mesh.name.empty() ? "Mesh " + std::to_string(index + 1) : mesh.name, uses[index] <= 1 });
        geom::MeshBuilder builder(model.mesh(def));
        for (const tinygltf::Primitive& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1) {
                continue;
            }
            const auto pos = prim.attributes.find("POSITION");
            if (pos == prim.attributes.end()) {
                continue;
            }
            const std::vector<double> p = readAccessor<double>(gltf, pos->second, 3);
            std::vector<std::uint32_t> idx = readAccessor<std::uint32_t>(gltf, prim.indices, 1);
            if (prim.indices < 0) {
                idx.resize(p.size() / 3);
                for (std::size_t i = 0; i < idx.size(); ++i) {
                    idx[i] = static_cast<std::uint32_t>(i);
                }
            }
            const auto at = [&](std::uint32_t i) {
                return math::Vec3d { p[i * 3], p[i * 3 + 1], p[i * 3 + 2] } * options.scale;
            };
            for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
                if (std::max({ idx[i], idx[i + 1], idx[i + 2] }) * 3 + 2 >= p.size()) {
                    continue;
                }
                builder.face({ at(idx[i]), at(idx[i + 1]), at(idx[i + 2]) });
            }
        }
        builder.finish();
        result.faces += model.mesh(def).faces().size();
        meshDefinitions[index] = def;
        return def;
    };

    // Grupul importului: transformarea din Y-sus în Z-sus.
    const model::DefinitionId top = model.addDefinition({ path.stem().string(), true });
    std::function<void(int, model::DefinitionId)> addNode;
    addNode = [&](int index, model::DefinitionId into) {
        const tinygltf::Node& node = gltf.nodes[static_cast<std::size_t>(index)];
        // Translațiile sunt în metri: le scalăm ca geometria (care e deja în mm).
        const math::Mat4d local = scaleTranslation * localMatrix(node) * glm::inverse(scaleTranslation);
        const math::Transform transform(local);
        if (node.children.empty() && node.mesh >= 0) {
            model.addInstance({ into, meshDefinition(node.mesh), transform, node.name, {}, false, false, {} });
            return;
        }
        const model::DefinitionId group
            = model.addDefinition({ node.name.empty() ? "Nod " + std::to_string(index + 1) : node.name, true });
        model.addInstance({ into, group, transform, {}, {}, false, false, {} });
        if (node.mesh >= 0) {
            model.addInstance({ group, meshDefinition(node.mesh), {}, {}, {}, false, false, {} });
        }
        for (int child : node.children) {
            addNode(child, group);
        }
    };
    const int sceneIndex = gltf.defaultScene >= 0 ? gltf.defaultScene : 0;
    if (sceneIndex < static_cast<int>(gltf.scenes.size())) {
        for (int n : gltf.scenes[static_cast<std::size_t>(sceneIndex)].nodes) {
            addNode(n, top);
        }
    } else {
        for (std::size_t n = 0; n < gltf.nodes.size(); ++n) {
            addNode(static_cast<int>(n), top);
        }
    }
    math::Mat4d axes(1.0);
    if (options.yUp) {
        axes = glm::inverse(zUpToYUp());
    }
    result.instance = model.addInstance({ parent, top, math::Transform(axes), {}, {}, false, false, {} });
    if (result.faces == 0) {
        result.error = "Fișierul nu conține geometrie (triunghiuri).";
    }
    return result;
}

} // namespace aion::io
