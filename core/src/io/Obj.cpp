#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/io/Exchange.h"
#include "aion/core/model/Texturing.h"
#include "aion/core/model/Operations.h"

#include <glm/geometric.hpp>

#include <charconv>
#include <cstdio>
#include <functional>
#include <sstream>

namespace aion::io {

namespace {

math::Vec3d exportPoint(const math::Vec3d& p, const ExportOptions& o)
{
    const math::Vec3d q = o.yUp ? math::Vec3d { p.x, p.z, -p.y } : p;
    return q * o.scale;
}

math::Vec3d exportNormal(const math::Vec3d& n, const ExportOptions& o)
{
    return o.yUp ? math::Vec3d { n.x, n.z, -n.y } : n;
}

math::Vec3d importPoint(const math::Vec3d& p, const ImportOptions& o)
{
    const math::Vec3d q = o.yUp ? math::Vec3d { p.x, -p.z, p.y } : p;
    return q * o.scale;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.remove_suffix(1);
    }
    return s;
}

std::vector<std::string_view> split(std::string_view s)
{
    std::vector<std::string_view> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        const std::size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t') {
            ++i;
        }
        if (i > start) {
            out.push_back(s.substr(start, i - start));
        }
    }
    return out;
}

// Numele unui material în OBJ / MTL: fără spații (le-ar tăia cititoarele), unic prin id.
std::string mtlName(const model::Material& m, model::MaterialId id)
{
    std::string name;
    for (char c : m.name) {
        name += (c == ' ' || c == '\t') ? '_' : c;
    }
    return name + "_" + std::to_string(id.value);
}

std::string textureFileName(model::TextureId id, const model::Texture& t)
{
    const bool jpeg = t.data.size() > 2 && t.data[0] == 0xFF && t.data[1] == 0xD8;
    return "aion_texture_" + std::to_string(id.value) + (jpeg ? ".jpg" : ".png");
}

bool parseDouble(std::string_view s, double& out)
{
    // std::from_chars pentru double nu e disponibil peste tot; strtod pe o copie terminată cu zero.
    const std::string copy(s);
    char* end = nullptr;
    out = std::strtod(copy.c_str(), &end);
    return end != copy.c_str();
}

} // namespace

std::string exportObj(const model::Model& model, const ExportOptions& options, std::string_view materialLibrary)
{
    std::ostringstream out;
    out << "# Aion — export OBJ (metri, Y în sus)\n";
    if (!materialLibrary.empty()) {
        out << "mtllib " << materialLibrary << "\n";
    }
    std::size_t vertexBase = 1;
    std::size_t normalBase = 1;
    char buffer[128];

    const auto writeDefinition = [&](const std::string& name, model::DefinitionId def, const math::Transform& toWorld,
                                     model::MaterialId inherited) {
        const geom::Mesh& mesh = model.mesh(def);
        if (mesh.faces().empty()) {
            return;
        }
        out << "o " << name << "\n";
        std::string currentMaterial;
        for (geom::FaceId f : mesh.faces().keys()) {
            const geom::Face& face = mesh.face(f);
            const model::MaterialId mid = face.material ? face.material : inherited;
            const model::Material* m = mid ? model.materials().find(mid) : nullptr;
            const std::string materialName = m != nullptr ? mtlName(*m, mid) : "aion_default";
            if (!materialLibrary.empty() && materialName != currentMaterial) {
                out << "usemtl " << materialName << "\n";
                currentMaterial = materialName;
            }
            const geom::FaceTriangles tri = geom::triangulateFace(mesh, f);
            for (const math::Vec3d& p : tri.points) {
                const math::Vec3d q = exportPoint(toWorld.applyToPoint(p), options);
                std::snprintf(buffer, sizeof buffer, "v %.6f %.6f %.6f\n", q.x, q.y, q.z);
                out << buffer;
                // UV-ul, calculat în coordonatele definiției (textura stă pe obiect, nu în lume).
                const math::Vec2d uv = m != nullptr
                    ? model::textureCoordinates(face.plane.normal, p, face.texture, m->textureSize) : math::Vec2d { 0.0 };
                std::snprintf(buffer, sizeof buffer, "vt %.6f %.6f\n", uv.x, uv.y);
                out << buffer;
            }
            const math::Vec3d n = exportNormal(glm::normalize(toWorld.applyToNormal(face.plane.normal)), options);
            std::snprintf(buffer, sizeof buffer, "vn %.6f %.6f %.6f\n", n.x, n.y, n.z);
            out << buffer;
            for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
                out << "f";
                for (int k = 0; k < 3; ++k) {
                    const std::size_t v = vertexBase + tri.indices[i + static_cast<std::size_t>(k)];
                    out << ' ' << v << '/' << v << '/' << normalBase;
                }
                out << '\n';
            }
            vertexBase += tri.points.size();
            normalBase += 1;
        }
    };

    std::function<void(model::DefinitionId, const math::Transform&, const std::string&, model::MaterialId)> visit;
    visit = [&](model::DefinitionId def, const math::Transform& toWorld, const std::string& name,
                model::MaterialId inherited) {
        writeDefinition(name, def, toWorld, inherited);
        for (model::InstanceId child : model.childrenOf(def)) {
            if (!options.includeHidden && !model.isVisible(child)) {
                continue;
            }
            const model::Instance& inst = model.instance(child);
            const std::string childName = inst.name.empty() ? model.definition(inst.definition).name : inst.name;
            visit(inst.definition, toWorld * inst.transform, childName, inst.material ? inst.material : inherited);
        }
    };
    visit(model.root(), {}, "Model", {});
    return out.str();
}

std::string exportMtl(const model::Model& model)
{
    std::ostringstream out;
    out << "# Aion — materiale\n"
           "newmtl aion_default\n"
           "Kd 0.85 0.84 0.82\n"
           "Ka 0 0 0\n"
           "Ks 0 0 0\n"
           "d 1\n"
           "illum 1\n";
    char buffer[128];
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        const model::Material& m = model.materials().values()[i];
        out << "\nnewmtl " << mtlName(m, model.materials().keys()[i]) << "\n";
        std::snprintf(buffer, sizeof buffer, "Kd %.4f %.4f %.4f\nKa 0 0 0\nKs %.3f %.3f %.3f\nd %.4f\nillum 2\n",
            m.color.r / 255.0, m.color.g / 255.0, m.color.b / 255.0, (1.0 - m.roughness) * 0.3,
            (1.0 - m.roughness) * 0.3, (1.0 - m.roughness) * 0.3, m.color.a / 255.0);
        out << buffer;
        if (const model::Texture* t = m.texture ? model.textures().find(m.texture) : nullptr; t && !t->data.empty()) {
            out << "map_Kd " << textureFileName(m.texture, *t) << "\n";
        }
    }
    return out.str();
}

std::vector<std::pair<std::string, std::vector<std::uint8_t>>> exportTextureFiles(const model::Model& model)
{
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> files;
    for (std::size_t i = 0; i < model.textures().size(); ++i) {
        const model::Texture& t = model.textures().values()[i];
        if (!t.data.empty()) {
            files.emplace_back(textureFileName(model.textures().keys()[i], t), t.data);
        }
    }
    return files;
}

ImportResult importObj(
    model::Model& model, model::DefinitionId parent, std::string_view text, std::string name, const ImportOptions& options)
{
    ImportResult result;
    std::vector<math::Vec3d> positions;
    std::vector<std::vector<math::Vec3d>> faces;
    std::size_t lineNumber = 0;
    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = trim(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        ++lineNumber;
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::vector<std::string_view> parts = split(line);
        if (parts.empty()) {
            continue;
        }
        if (parts[0] == "v" && parts.size() >= 4) {
            math::Vec3d p;
            if (!parseDouble(parts[1], p.x) || !parseDouble(parts[2], p.y) || !parseDouble(parts[3], p.z)) {
                result.error = "Linia " + std::to_string(lineNumber) + ": coordonată invalidă.";
                return result;
            }
            positions.push_back(importPoint(p, options));
        } else if (parts[0] == "f" && parts.size() >= 4) {
            std::vector<math::Vec3d> face;
            for (std::size_t i = 1; i < parts.size(); ++i) {
                const std::string_view token = parts[i].substr(0, parts[i].find('/'));
                long long index = 0;
                const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), index);
                if (ec != std::errc {} || index == 0) {
                    result.error = "Linia " + std::to_string(lineNumber) + ": față invalidă.";
                    return result;
                }
                const long long resolved = index < 0 ? static_cast<long long>(positions.size()) + index : index - 1;
                if (resolved < 0 || resolved >= static_cast<long long>(positions.size())) {
                    result.error = "Linia " + std::to_string(lineNumber) + ": vârf inexistent.";
                    return result;
                }
                face.push_back(positions[static_cast<std::size_t>(resolved)]);
            }
            faces.push_back(std::move(face));
        }
    }
    if (faces.empty()) {
        result.error = "Fișierul nu conține fețe.";
        return result;
    }
    const model::DefinitionId def = model.addDefinition({ name.empty() ? "Import OBJ" : std::move(name), true });
    geom::MeshBuilder builder(model.mesh(def));
    for (const auto& face : faces) {
        builder.face(face);
    }
    builder.finish();
    result.faces = model.mesh(def).faces().size();
    result.instance = model.addInstance({ parent, def, {}, {}, {}, false, false, {}, {}, {} });
    return result;
}

} // namespace aion::io
