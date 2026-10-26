#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/io/Exchange.h"
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

    const auto writeDefinition = [&](const std::string& name, model::DefinitionId def, const math::Transform& toWorld) {
        const geom::Mesh& mesh = model.mesh(def);
        if (mesh.faces().empty()) {
            return;
        }
        out << "o " << name << "\n";
        if (!materialLibrary.empty()) {
            out << "usemtl aion_default\n";
        }
        for (geom::FaceId f : mesh.faces().keys()) {
            const geom::FaceTriangles tri = geom::triangulateFace(mesh, f);
            for (const math::Vec3d& p : tri.points) {
                const math::Vec3d q = exportPoint(toWorld.applyToPoint(p), options);
                std::snprintf(buffer, sizeof buffer, "v %.6f %.6f %.6f\n", q.x, q.y, q.z);
                out << buffer;
            }
            const math::Vec3d n
                = exportNormal(glm::normalize(toWorld.applyToNormal(mesh.face(f).plane.normal)), options);
            std::snprintf(buffer, sizeof buffer, "vn %.6f %.6f %.6f\n", n.x, n.y, n.z);
            out << buffer;
            for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
                out << "f";
                for (int k = 0; k < 3; ++k) {
                    out << ' ' << vertexBase + tri.indices[i + static_cast<std::size_t>(k)] << "//" << normalBase;
                }
                out << '\n';
            }
            vertexBase += tri.points.size();
            normalBase += 1;
        }
    };

    std::function<void(model::DefinitionId, const math::Transform&, const std::string&)> visit;
    visit = [&](model::DefinitionId def, const math::Transform& toWorld, const std::string& name) {
        writeDefinition(name, def, toWorld);
        for (model::InstanceId child : model.childrenOf(def)) {
            if (!options.includeHidden && !model.isVisible(child)) {
                continue;
            }
            const model::Instance& inst = model.instance(child);
            const std::string childName = inst.name.empty() ? model.definition(inst.definition).name : inst.name;
            visit(inst.definition, toWorld * inst.transform, childName);
        }
    };
    visit(model.root(), {}, "Model");
    return out.str();
}

std::string exportMtl()
{
    return "# Aion — materiale\n"
           "newmtl aion_default\n"
           "Kd 0.85 0.84 0.82\n"
           "Ka 0 0 0\n"
           "Ks 0 0 0\n"
           "d 1\n"
           "illum 1\n";
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
    result.instance = model.addInstance({ parent, def, {}, {}, {}, false, false });
    return result;
}

} // namespace aion::io
