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

// Un material: culoare (cu opacitate), rugozitate și caracter metalic (PBR simplificat).
struct Material {
    std::string name;
    Color color { 200, 200, 200, 255 };
    float roughness = 0.8f;
    float metallic = 0.0f;

    bool operator==(const Material&) const = default;
};

// O linie sau un punct de construcție (Tape Measure, Protractor), în coordonatele definiției `context`.
// `direction` nul = punct; altfel linie infinită prin `point`.
struct Guide {
    DefinitionId context;
    math::Vec3d point { 0.0 };
    math::Vec3d direction { 0.0 };

    [[nodiscard]] bool isLine() const { return direction != math::Vec3d { 0.0 }; }
    bool operator==(const Guide&) const = default;
};

// O adnotare: o cotă (distanța dintre `a` și `b`, cu linia de cotă deplasată cu `offset`) sau un text
// (ancorat în `a`, cu eticheta în `a + offset`). În coordonatele definiției `context`.
struct Annotation {
    enum class Kind { Dimension, Text };
    Kind kind = Kind::Dimension;
    DefinitionId context;
    math::Vec3d a { 0.0 };
    math::Vec3d b { 0.0 };
    math::Vec3d offset { 0.0 };
    std::string text; // pentru Text; pentru o cotă, gol = lungimea calculată

    bool operator==(const Annotation&) const = default;
};

// Un plan de secțiune (în coordonatele modelului): ce e de partea normalei se ascunde când e activ.
struct SectionPlane {
    std::string name;
    math::Vec3d point { 0.0 };
    math::Vec3d normal { 0.0, 0.0, 1.0 };
    bool active = true;

    bool operator==(const SectionPlane&) const = default;
};

// O scenă: o poziție de cameră salvată (și stilul de afișare).
struct Scene {
    std::string name;
    math::Vec3d target { 0.0 };
    double yaw = 0.0;
    double pitch = 0.0;
    double distance = 10000.0;
    bool orthographic = false;
    std::string style;

    bool operator==(const Scene&) const = default;
};

// Proprietățile unei definiții; geometria stă separat (Model::mesh).
struct Definition {
    std::string name;
    bool group = false; // fără copii legate (o singură instanță); altfel, un obiect cu copii
    // Un grup în sensul Outliner-ului: colecție de obiecte și alte grupuri (folder), fără geometrie proprie.
    bool collection = false;

    bool operator==(const Definition&) const = default;
};

// Comportamentul la click cu unealta Interact (ca la componentele dinamice din SketchUp): o ușă pe balama sau
// un sertar care glisează. Axele și punctele se calculează din cutia definiției, în coordonatele ei.
struct Behavior {
    enum class Kind { None, Rotate, Slide };
    // Rotate: latura balamalei (Left, Right = verticală; Top, Bottom = orizontală), deschidere spre față (−Y).
    // Slide: direcția (Front = −Y, Back = +Y, Left = −X, Right = +X, Top = +Z, Bottom = −Z).
    enum class Anchor { Left, Right, Front, Back, Top, Bottom };

    Kind kind = Kind::None;
    Anchor anchor = Anchor::Left;
    double amount = 90.0; // grade (Rotate) sau mm (Slide); negativ = sens invers
    double duration = 0.6; // secunde
    bool open = false; // starea curentă (transformarea instanței e cea „deschisă” când e true)

    bool operator==(const Behavior&) const = default;
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
    // Materialul cu care se văd fețele fără material din interior (ca „vopsirea” unui grup în SketchUp).
    MaterialId material;
    Behavior behavior;

    bool operator==(const Instance&) const = default;
};

// Schimbările unei operații pe tot modelul: structura (înainte/după) și geometria fiecărei definiții.
struct ChangeSet {
    std::map<DefinitionId, geom::EntityChange<Definition>> definitions;
    std::map<InstanceId, geom::EntityChange<Instance>> instances;
    std::map<TagId, geom::EntityChange<Tag>> tags;
    std::map<MaterialId, geom::EntityChange<Material>> materials;
    std::map<GuideId, geom::EntityChange<Guide>> guides;
    std::map<AnnotationId, geom::EntityChange<Annotation>> annotations;
    std::map<SectionId, geom::EntityChange<SectionPlane>> sections;
    std::map<SceneId, geom::EntityChange<Scene>> scenes;
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
    [[nodiscard]] const geom::SlotMap<MaterialId, Material>& materials() const { return m_materials; }
    [[nodiscard]] const Material& material(MaterialId id) const { return m_materials.at(id); }
    [[nodiscard]] const geom::SlotMap<GuideId, Guide>& guides() const { return m_guides; }
    [[nodiscard]] const geom::SlotMap<AnnotationId, Annotation>& annotations() const { return m_annotations; }
    [[nodiscard]] const geom::SlotMap<SectionId, SectionPlane>& sections() const { return m_sections; }
    [[nodiscard]] const geom::SlotMap<SceneId, Scene>& scenes() const { return m_scenes; }
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
    MaterialId addMaterial(Material material);
    // Fețele și instanțele care îl folosesc rămân fără material.
    void removeMaterial(MaterialId id);
    void setMaterial(MaterialId id, Material material);
    GuideId addGuide(Guide guide);
    void removeGuide(GuideId id);
    AnnotationId addAnnotation(Annotation annotation);
    void removeAnnotation(AnnotationId id);
    void setAnnotation(AnnotationId id, Annotation annotation);
    SectionId addSection(SectionPlane section);
    void removeSection(SectionId id);
    void setSection(SectionId id, SectionPlane section);
    SceneId addScene(Scene scene);
    void removeScene(SceneId id);
    void setScene(SceneId id, Scene scene);

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
    void touch(MaterialId id);
    template <class IdT, class T>
    IdT addEntity(geom::SlotMap<IdT, T>& map, std::map<IdT, geom::EntityChange<T>> ChangeSet::*changes, T value);
    template <class IdT, class T>
    void removeEntity(geom::SlotMap<IdT, T>& map, std::map<IdT, geom::EntityChange<T>> ChangeSet::*changes, IdT id);
    template <class IdT, class T>
    void setEntity(geom::SlotMap<IdT, T>& map, std::map<IdT, geom::EntityChange<T>> ChangeSet::*changes, IdT id, T value);
    void notifyStructure();

    DefinitionId m_root;
    geom::SlotMap<DefinitionId, Definition> m_definitions;
    geom::SlotMap<InstanceId, Instance> m_instances;
    geom::SlotMap<TagId, Tag> m_tags;
    geom::SlotMap<MaterialId, Material> m_materials;
    geom::SlotMap<GuideId, Guide> m_guides;
    geom::SlotMap<AnnotationId, Annotation> m_annotations;
    geom::SlotMap<SectionId, SectionPlane> m_sections;
    geom::SlotMap<SceneId, Scene> m_scenes;
    std::map<DefinitionId, std::unique_ptr<geom::Mesh>> m_meshes;
    // Geometria (goală) a definițiilor șterse, păstrată ca undo să le refacă fără să refolosească ID-uri.
    std::map<DefinitionId, std::unique_ptr<geom::Mesh>> m_graveyard;
    std::optional<ChangeSet> m_recording;
    std::set<DefinitionId> m_createdWhileRecording;
    bool m_structureDirty = false;
    std::vector<ModelObserver*> m_observers;
};

} // namespace aion::model
