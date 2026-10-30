#pragma once

#include "aion/core/geom/ChangeSet.h"
#include "aion/core/geom/Mesh.h"
#include "aion/core/model/Model.h"

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace aion::commands {

// O operație care poate fi anulată și refăcută (își ține ținta: geometria sau modelul).
class Command {
public:
    virtual ~Command() = default;
    virtual void apply() = 0;
    virtual void revert() = 0;
    // Numele din meniul Edit („Undo Line”, „Undo Push/Pull”).
    [[nodiscard]] virtual std::string name() const = 0;
};

// Schimbările înregistrate de un Mesh în timpul unei tranzacții.
class RecordedCommand final : public Command {
public:
    RecordedCommand(geom::Mesh& mesh, std::string name, geom::ChangeSet changes)
        : m_mesh(mesh)
        , m_name(std::move(name))
        , m_changes(std::move(changes))
    {
    }

    void apply() override { m_mesh.apply(m_changes, geom::ChangeDirection::Forward); }
    void revert() override { m_mesh.apply(m_changes, geom::ChangeDirection::Backward); }
    [[nodiscard]] std::string name() const override { return m_name; }

private:
    geom::Mesh& m_mesh;
    std::string m_name;
    geom::ChangeSet m_changes;
};

// Schimbările înregistrate de un Model (structură și geometria fiecărei definiții).
class ModelCommand final : public Command {
public:
    ModelCommand(model::Model& model, std::string name, model::ChangeSet changes)
        : m_model(model)
        , m_name(std::move(name))
        , m_changes(std::move(changes))
    {
    }

    void apply() override { m_model.apply(m_changes, geom::ChangeDirection::Forward); }
    void revert() override { m_model.apply(m_changes, geom::ChangeDirection::Backward); }
    [[nodiscard]] std::string name() const override { return m_name; }

private:
    model::Model& m_model;
    std::string m_name;
    model::ChangeSet m_changes;
};

// Istoricul undo/redo al unui model (sau, mai simplu, al unei singure geometrii).
//
// Uneltele interactive deschid o tranzacție (beginTransaction), modifică geometria pe măsură ce
// utilizatorul trage, apoi fac commit() la click-ul final sau abort() la Esc.
// `run` primește o funcție de `geom::Mesh&` (geometria contextului activ) sau de `model::Model&`.
class CommandStack {
public:
    explicit CommandStack(geom::Mesh& mesh);
    explicit CommandStack(model::Model& model);

    // Contextul de editare (pentru un model): definiția a cărei geometrie o primesc uneltele.
    void setActiveDefinition(model::DefinitionId definition);
    [[nodiscard]] geom::Mesh& activeMesh();
    [[nodiscard]] model::Model* model() { return m_model; }

    void beginTransaction(std::string name);
    [[nodiscard]] bool inTransaction() const { return m_transactionName.has_value(); }
    // Încheie tranzacția; întoarce false dacă nu s-a schimbat nimic (nu se adaugă în istoric).
    bool commit();
    // Anulează tot ce s-a făcut în tranzacție.
    void abort();

    // Rulează `fn` într-o tranzacție și o încheie.
    template <class Fn> bool run(std::string name, Fn&& fn)
    {
        beginTransaction(std::move(name));
        if constexpr (std::is_invocable_v<Fn, model::Model&>) {
            assert(m_model != nullptr);
            fn(*m_model);
        } else {
            fn(activeMesh());
        }
        return commit();
    }

    // Adaugă o comandă construită separat; o aplică întâi.
    void push(std::unique_ptr<Command> command);

    [[nodiscard]] bool canUndo() const { return !m_undo.empty(); }
    [[nodiscard]] bool canRedo() const { return !m_redo.empty(); }
    [[nodiscard]] std::string undoName() const;
    [[nodiscard]] std::string redoName() const;
    bool undo();
    bool redo();
    void clear();
    [[nodiscard]] std::size_t undoCount() const { return m_undo.size(); }

    // Starea „salvată”: isClean() e adevărat cât timp istoricul e în punctul marcat (după undo/redo înapoi la
    // el, tot).
    void markClean();
    // Documentul are modificări nesalvate chiar fără istoric (de exemplu după o recuperare).
    void markDirty();
    [[nodiscard]] bool isClean() const;

    // Verificarea modelului după fiecare commit / undo / redo (implicit activă în debug).
    // La eșec se apelează `onInvalid`; implicit se oprește programul (assert).
    void setValidation(bool enabled, geom::ValidationLevel level = geom::ValidationLevel::Topology);
    void setOnInvalid(std::function<void(const geom::ValidationReport&)> handler);
    // Apelat după orice schimbare a istoricului (pentru meniul Edit din UI).
    void setOnChanged(std::function<void()> handler) { m_onChanged = std::move(handler); }
    // Finalizatorul (ADR 0044): rulat primul în commit(), în aceeași tranzacție, cu definițiile al căror mesh
    // s-a schimbat (de exemplu ca plăcile să-și preia dimensiunile după Push/Pull). Doar pentru stivele pe
    // model; nu la push(), undo sau redo. O excepție anulează tranzacția: commit() întoarce false și mesajul
    // e în lastError().
    void setFinalizer(std::function<void(model::Model&, std::span<const model::DefinitionId>)> finalizer)
    {
        m_finalizer = std::move(finalizer);
    }
    [[nodiscard]] const std::string& lastError() const { return m_lastError; }

private:
    void afterChange();

    geom::Mesh* m_mesh = nullptr;
    model::Model* m_model = nullptr;
    model::DefinitionId m_active;
    std::vector<std::unique_ptr<Command>> m_undo;
    std::vector<std::unique_ptr<Command>> m_redo;
    // Un număr unic pentru fiecare comandă, ca starea salvată să fie recunoscută sigur.
    std::vector<std::uint64_t> m_undoSerials;
    std::vector<std::uint64_t> m_redoSerials;
    std::uint64_t m_nextSerial = 1;
    std::function<void(model::Model&, std::span<const model::DefinitionId>)> m_finalizer;
    std::string m_lastError;
    std::uint64_t m_cleanSerial = 0; // 0 = istoric gol
    std::optional<std::string> m_transactionName;
    bool m_validate;
    geom::ValidationLevel m_validationLevel = geom::ValidationLevel::Topology;
    std::function<void(const geom::ValidationReport&)> m_onInvalid;
    std::function<void()> m_onChanged;
};

} // namespace aion::commands
