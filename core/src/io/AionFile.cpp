#include "aion/core/io/AionFile.h"

#include "aion/core/Version.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace aion::io {

namespace {

using nlohmann::json;

constexpr const char* kModelEntry = "model.json";
constexpr const char* kThumbnailEntry = "thumbnail.png";

template <class IdT, class T>
std::vector<std::size_t> sortedIndices(const geom::SlotMap<IdT, T>& map)
{
    std::vector<std::size_t> order(map.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return map.keys()[a] < map.keys()[b]; });
    return order;
}

json loopToJson(const geom::Loop& loop)
{
    json uses = json::array();
    for (const geom::EdgeUse& u : loop.uses) {
        const auto id = static_cast<std::int64_t>(u.edge.value);
        uses.push_back(u.reversed ? -id : id);
    }
    return uses;
}

json meshToJson(const geom::Mesh& mesh)
{
    json vertices = json::array();
    for (std::size_t i : sortedIndices(mesh.vertices())) {
        const geom::Vertex& v = mesh.vertices().values()[i];
        vertices.push_back({ mesh.vertices().keys()[i].value, v.position.x, v.position.y, v.position.z });
    }
    json edges = json::array();
    for (std::size_t i : sortedIndices(mesh.edges())) {
        const geom::Edge& e = mesh.edges().values()[i];
        edges.push_back({ mesh.edges().keys()[i].value, e.v0.value, e.v1.value, e.soft ? 1 : 0, e.curve,
            e.smoothCurve ? 1 : 0 });
    }
    json faces = json::array();
    for (std::size_t i : sortedIndices(mesh.faces())) {
        const geom::Face& f = mesh.faces().values()[i];
        json inner = json::array();
        for (const geom::Loop& h : f.inner) {
            inner.push_back(loopToJson(h));
        }
        faces.push_back({
            { "id", mesh.faces().keys()[i].value },
            { "material", f.material.value },
            { "plane", { f.plane.normal.x, f.plane.normal.y, f.plane.normal.z, f.plane.d } },
            { "outer", loopToJson(f.outer) },
            { "inner", inner },
        });
    }
    return { { "vertices", vertices }, { "edges", edges }, { "faces", faces } };
}

geom::Loop loopFromJson(const json& j)
{
    geom::Loop loop;
    for (const json& u : j) {
        const std::int64_t id = u.get<std::int64_t>();
        loop.uses.push_back({ geom::EdgeId { static_cast<std::uint64_t>(id < 0 ? -id : id) }, id < 0 });
    }
    return loop;
}

// Geometria unei definiții, ca ChangeSet de creare (ID-urile păstrate); vecinătățile se refac din muchii și fețe.
geom::ChangeSet meshFromJson(const json& j)
{
    std::map<geom::VertexId, geom::Vertex> vertices;
    std::map<geom::EdgeId, geom::Edge> edges;
    std::map<geom::FaceId, geom::Face> faces;
    for (const json& v : j.at("vertices")) {
        vertices[geom::VertexId { v.at(0).get<std::uint64_t>() }]
            = geom::Vertex { { v.at(1).get<double>(), v.at(2).get<double>(), v.at(3).get<double>() }, {} };
    }
    for (const json& e : j.at("edges")) {
        geom::Edge edge { geom::VertexId { e.at(1).get<std::uint64_t>() }, geom::VertexId { e.at(2).get<std::uint64_t>() },
            {} };
        edge.soft = e.at(3).get<int>() != 0;
        edge.curve = e.at(4).get<std::uint32_t>();
        edge.smoothCurve = e.at(5).get<int>() != 0;
        edges[geom::EdgeId { e.at(0).get<std::uint64_t>() }] = edge;
    }
    for (const json& f : j.at("faces")) {
        const json& p = f.at("plane");
        geom::Face face;
        face.plane = { { p.at(0).get<double>(), p.at(1).get<double>(), p.at(2).get<double>() }, p.at(3).get<double>() };
        face.outer = loopFromJson(f.at("outer"));
        face.material = model::MaterialId { f.value("material", std::uint64_t { 0 }) };
        for (const json& h : f.at("inner")) {
            face.inner.push_back(loopFromJson(h));
        }
        faces[geom::FaceId { f.at("id").get<std::uint64_t>() }] = face;
    }
    for (auto& [id, e] : edges) {
        vertices.at(e.v0).edges.push_back(id);
        vertices.at(e.v1).edges.push_back(id);
    }
    for (auto& [id, f] : faces) {
        const auto attach = [&](const geom::Loop& loop) {
            for (const geom::EdgeUse& u : loop.uses) {
                edges.at(u.edge).faces.push_back(id);
            }
        };
        attach(f.outer);
        for (const geom::Loop& h : f.inner) {
            attach(h);
        }
    }
    geom::ChangeSet changes;
    for (auto& [id, v] : vertices) {
        changes.vertices[id].after = std::move(v);
    }
    for (auto& [id, e] : edges) {
        changes.edges[id].after = std::move(e);
    }
    for (auto& [id, f] : faces) {
        changes.faces[id].after = std::move(f);
    }
    return changes;
}

json colorToJson(const model::Color& c)
{
    return { c.r, c.g, c.b, c.a };
}

} // namespace

std::string toJson(const model::Model& model)
{
    json definitions = json::array();
    for (std::size_t i : sortedIndices(model.definitions())) {
        const model::DefinitionId id = model.definitions().keys()[i];
        const model::Definition& d = model.definitions().values()[i];
        definitions.push_back({
            { "id", id.value },
            { "name", d.name },
            { "group", d.group },
            { "mesh", meshToJson(model.mesh(id)) },
        });
    }
    json instances = json::array();
    for (std::size_t i : sortedIndices(model.instances())) {
        const model::Instance& inst = model.instances().values()[i];
        json matrix = json::array();
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                matrix.push_back(inst.transform.matrix()[c][r]);
            }
        }
        instances.push_back({
            { "id", model.instances().keys()[i].value },
            { "parent", inst.parent.value },
            { "definition", inst.definition.value },
            { "transform", matrix },
            { "name", inst.name },
            { "tag", inst.tag.value },
            { "hidden", inst.hidden },
            { "locked", inst.locked },
            { "material", inst.material.value },
        });
    }
    json tags = json::array();
    for (std::size_t i : sortedIndices(model.tags())) {
        const model::Tag& t = model.tags().values()[i];
        tags.push_back({
            { "id", model.tags().keys()[i].value },
            { "name", t.name },
            { "color", colorToJson(t.color) },
            { "visible", t.visible },
        });
    }
    json materials = json::array();
    for (std::size_t i : sortedIndices(model.materials())) {
        const model::Material& m = model.materials().values()[i];
        materials.push_back({
            { "id", model.materials().keys()[i].value },
            { "name", m.name },
            { "color", colorToJson(m.color) },
            { "roughness", m.roughness },
            { "metallic", m.metallic },
        });
    }
    const json doc {
        { "format", "aion" },
        { "formatVersion", kFormatVersion },
        { "generator", std::string("Aion ") + std::string(versionString()) },
        { "units", "mm" },
        { "root", model.root().value },
        { "definitions", definitions },
        { "instances", instances },
        { "tags", tags },
        { "materials", materials },
    };
    return doc.dump(1);
}

LoadResult fromJson(std::string_view text)
{
    LoadResult result;
    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        result.error = "Fișierul nu conține un model Aion valid.";
        return result;
    }
    if (doc.value("format", std::string()) != "aion") {
        result.error = "Fișierul nu e un model Aion.";
        return result;
    }
    if (doc.value("formatVersion", 0) > kFormatVersion) {
        result.error = "Fișierul a fost salvat de o versiune mai nouă a Aion.";
        return result;
    }
    try {
        model::Model model;
        if (model::DefinitionId { doc.at("root").get<std::uint64_t>() } != model.root()) {
            result.error = "Fișierul are o structură necunoscută (rădăcina modelului).";
            return result;
        }
        model::ChangeSet changes;
        for (const json& d : doc.at("definitions")) {
            const model::DefinitionId id { d.at("id").get<std::uint64_t>() };
            model::Definition def { d.at("name").get<std::string>(), d.at("group").get<bool>() };
            auto& change = changes.definitions[id];
            if (id == model.root()) {
                change.before = model.definition(id);
            }
            change.after = std::move(def);
            changes.meshes[id] = meshFromJson(d.at("mesh"));
        }
        for (const json& i : doc.at("instances")) {
            model::Instance inst;
            inst.parent = model::DefinitionId { i.at("parent").get<std::uint64_t>() };
            inst.definition = model::DefinitionId { i.at("definition").get<std::uint64_t>() };
            const json& m = i.at("transform");
            math::Mat4d matrix(1.0);
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    matrix[c][r] = m.at(static_cast<std::size_t>(c * 4 + r)).get<double>();
                }
            }
            inst.transform = math::Transform(matrix);
            inst.name = i.value("name", std::string());
            inst.tag = model::TagId { i.value("tag", std::uint64_t { 0 }) };
            inst.hidden = i.value("hidden", false);
            inst.locked = i.value("locked", false);
            inst.material = model::MaterialId { i.value("material", std::uint64_t { 0 }) };
            changes.instances[model::InstanceId { i.at("id").get<std::uint64_t>() }].after = std::move(inst);
        }
        for (const json& t : doc.at("tags")) {
            const json& c = t.at("color");
            model::Tag tag { t.at("name").get<std::string>(),
                { c.at(0).get<std::uint8_t>(), c.at(1).get<std::uint8_t>(), c.at(2).get<std::uint8_t>(),
                    c.at(3).get<std::uint8_t>() },
                t.value("visible", true) };
            changes.tags[model::TagId { t.at("id").get<std::uint64_t>() }].after = std::move(tag);
        }
        for (const json& mj : doc.value("materials", json::array())) {
            const json& c = mj.at("color");
            model::Material material { mj.at("name").get<std::string>(),
                { c.at(0).get<std::uint8_t>(), c.at(1).get<std::uint8_t>(), c.at(2).get<std::uint8_t>(),
                    c.at(3).get<std::uint8_t>() },
                mj.value("roughness", 0.8f), mj.value("metallic", 0.0f) };
            changes.materials[model::MaterialId { mj.at("id").get<std::uint64_t>() }].after = std::move(material);
        }
        model.apply(changes, geom::ChangeDirection::Forward);
        const geom::ValidationReport report = model.validate(geom::ValidationLevel::Topology);
        if (!report.ok()) {
            result.error = "Fișierul e deteriorat: " + report.errors.front();
            return result;
        }
        result.model = std::move(model);
    } catch (const std::exception& e) {
        result.error = std::string("Fișierul e deteriorat (") + e.what() + ").";
    }
    return result;
}

std::vector<std::uint8_t> writeArchive(const model::Model& model, std::span<const std::uint8_t> thumbnailPng)
{
    const std::string text = toJson(model);
    mz_zip_archive zip {};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        return {};
    }
    bool ok = mz_zip_writer_add_mem(&zip, kModelEntry, text.data(), text.size(), MZ_BEST_COMPRESSION);
    if (ok && !thumbnailPng.empty()) {
        ok = mz_zip_writer_add_mem(&zip, kThumbnailEntry, thumbnailPng.data(), thumbnailPng.size(), MZ_NO_COMPRESSION);
    }
    void* buffer = nullptr;
    std::size_t size = 0;
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    std::vector<std::uint8_t> out;
    if (ok) {
        const auto* bytes = static_cast<const std::uint8_t*>(buffer);
        out.assign(bytes, bytes + size);
    }
    mz_zip_writer_end(&zip);
    return out;
}

namespace {

std::optional<std::vector<std::uint8_t>> extract(std::span<const std::uint8_t> bytes, const char* name)
{
    mz_zip_archive zip {};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        return std::nullopt;
    }
    std::size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    std::optional<std::vector<std::uint8_t>> out;
    if (data != nullptr) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        out.emplace(p, p + size);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    return out;
}

} // namespace

LoadResult readArchive(std::span<const std::uint8_t> bytes)
{
    const auto text = extract(bytes, kModelEntry);
    if (!text) {
        LoadResult r;
        r.error = "Fișierul nu e o arhivă Aion (lipsește model.json).";
        return r;
    }
    return fromJson(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
}

std::vector<std::uint8_t> readThumbnail(std::span<const std::uint8_t> bytes)
{
    return extract(bytes, kThumbnailEntry).value_or(std::vector<std::uint8_t> {});
}

bool saveFile(const std::filesystem::path& path, const model::Model& model, std::span<const std::uint8_t> thumbnailPng,
    std::string* error)
{
    const std::vector<std::uint8_t> bytes = writeArchive(model, thumbnailPng);
    if (bytes.empty()) {
        if (error != nullptr) {
            *error = "Arhiva nu a putut fi creată.";
        }
        return false;
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            if (error != nullptr) {
                *error = "Fișierul nu a putut fi scris.";
            }
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        if (error != nullptr) {
            *error = "Fișierul nu a putut fi înlocuit.";
        }
        return false;
    }
    return true;
}

LoadResult loadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LoadResult r;
        r.error = "Fișierul nu poate fi deschis.";
        return r;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return readArchive(bytes);
}

} // namespace aion::io
