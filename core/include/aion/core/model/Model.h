#pragma once

#include "aion/core/geom/ChangeSet.h"
#include "aion/core/geom/Id.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/geom/SlotMap.h"
#include "aion/core/math/Transform.h"
#include "aion/core/model/Ids.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

// Modelul: definiții (fiecare cu geometria ei), instanțe plasate cu o transformare și tag-uri (vezi ADR 0008).
// Rădăcina e și ea o definiție; un grup e o definiție cu o singură instanță, o componentă poate avea oricâte.
namespace aion::model {

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;

    bool operator==(const Color&) const = default;
};

// Un tag (layer): vizibilitate și culoare pentru entitățile care îl poartă.
struct Tag {
    std::string name;
    Color color;
    bool visible = true;

    bool operator==(const Tag&) const = default;
};

// Proprietățile unei definiții; geometria stă separat (Model::mesh).
struct Definition {
    std::string name;
    bool group = false; // grup (o singură instanță, fără nume în biblioteca de componente)

    bool operator==(const Definition&) const = default;
};

// O plasare a unei definiții în altă definiție (`parent`), cu transformarea local → părinte.
struct Instance {
    DefinitionId parent;
    DefinitionId definition;
    math::Transform transform;
    std::string name;
    TagId tag; // nul = fără tag
    bool hidden = false;
    bool locked = false;

    bool operator==(const Instance&) const = default;
};

// Schimbările unei operații pe tot modelul: structura (înainte/după) și geometria fiecărei definiții.
struct ChangeSet {
    std::map<DefinitionId, geom::EntityChange<Definition>> definitions;
    std::map<InstanceId, geom::EntityChange<Instance>> instances;
    std::map<TagId, geom::EntityChange<Tag>> tags;
    std::map<DefinitionId, geom::ChangeSet> meshes;

    [[nodiscard]] bool empty() const;
};

class Model;

// Notificări pentru randare și interfață.
class ModelObserver {
public:
    virtual ~ModelObserver() = default;
    virtual void meshChanged(DefinitionId definition, const geom::Mesh& mesh, const geom::ChangeSummary& summary) = 0;
    // Definiții, instanțe sau tag-uri adăugate, schimbate sau șterse.
    virtual void structureChanged(const Model& model) = 0;
};

class Model final : private geom::MeshObserver {
public:
    Model();
    ~Model() override;
    Model(const Model& other);
    Model& operator=(const Model& other);

    // --- Citire ---
    [[nodiscard]] DefinitionId root() const { return m_root; }
    [[nodiscard]] const geom::SlotMap<DefinitionId, Definition>& definitions() const { return m_definitions; }
    [[nodiscard]] const geom::SlotMap<InstanceId, Instance>& instances() const { return m_instances; }
    [[nodiscard]] const geom::SlotMap<TagId, Tag>& tags() const { return m_tags; }
    [[nodiscard]] const Definition& definition(DefinitionId id) const { return m_definitions.at(id); }
    [[nodiscard]] const Instance& instance(InstanceId id) const { return m_instances.at(id); }
    [[nodiscard]] const Tag& tag(TagId id) const { return m_tags.at(id); }
    [[nodiscard]] geom::Mesh& mesh(DefinitionId id) { return *m_meshes.at(id); }
    [[nodiscard]] const geom::Mesh& mesh(DefinitionId id) const { return *m_meshes.at(id); }
    // Instanțele plasate direct în definiția dată (copiii ei în Outliner), în ordinea creării.
    [[nodiscard]] std::vector<InstanceId> childrenOf(DefinitionId parent) const;
    // Toate instanțele unei definiții (copiile unei componente).
    [[nodiscard]] std::vector<InstanceId> instancesOf(DefinitionId definition) const;
    // O instanță e vizibilă dacă nu e ascunsă și tag-ul ei e vizibil.
    [[nodiscard]] bool isVisible(InstanceId id) const;

    // --- Modificări (înregistrate pentru undo când înregistrarea e activă) ---
    DefinitionId addDefinition(Definition definition);
    // Șterge geometria și definiția. Definiția nu trebuie să mai aibă instanțe și nici copii.
    void removeDefinition(DefinitionId id);
    void setDefinition(DefinitionId id, Definition definition);
    InstanceId addInstance(Instance instance);
    void removeInstance(InstanceId id);
    void setInstance(InstanceId id, Instance instance);
    TagId addTag(Tag tag);
    // Instanțele cu acest tag rămân fără tag.
    void removeTag(TagId id);
    void setTag(TagId id, Tag tag);

    // --- Înregistrarea schimbărilor ---
    void beginRecording();
    [[nodiscard]] bool isRecording() const { return m_recording.has_value(); }
    ChangeSet endRecording();
    void apply(const ChangeSet& changes, geom::ChangeDirection direction);

    void addObserver(ModelObserver* observer);
    void removeObserver(ModelObserver* observer);

    // --- Verificări ---
    // Geometria fiecărei definiții și structura: referințe valide, fără cicluri (o definiție nu se conține pe sine).
    [[nodiscard]] geom::ValidationReport validate(geom::ValidationLevel level = geom::ValidationLevel::Full) const;
    [[nodiscard]] bool sameAs(const Model& other) const;

private:
    void meshChanged(const geom::Mesh& mesh, const geom::ChangeSummary& summary) override;
    geom::Mesh& createMesh(DefinitionId id);
    void destroyMesh(DefinitionId id);
    void touch(DefinitionId id);
    void touch(InstanceId id);
    void touch(TagId id);
    void notifyStructure();

    DefinitionId m_root;
    geom::SlotMap<DefinitionId, Definition> m_definitions;
    geom::SlotMap<InstanceId, Instance> m_instances;
    geom::SlotMap<TagId, Tag> m_tags;
    std::map<DefinitionId, std::unique_ptr<geom::Mesh>> m_meshes;
    // Geometria (goală) a definițiilor șterse, păstrată ca undo să le refacă fără să refolosească ID-uri.
    std::map<DefinitionId, std::unique_ptr<geom::Mesh>> m_graveyard;
    std::optional<ChangeSet> m_recording;
    std::set<DefinitionId> m_createdWhileRecording;
    bool m_structureDirty = false;
    std::vector<ModelObserver*> m_observers;
};

} // namespace aion::model
