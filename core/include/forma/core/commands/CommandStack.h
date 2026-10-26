#pragma once

#include "forma/core/geom/ChangeSet.h"
#include "forma/core/geom/Mesh.h"

#include <functional>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace forma::commands {

// O operație care poate fi anulată și refăcută.
class Command {
public:
    virtual ~Command() = default;
    virtual void apply(geom::Mesh& mesh) = 0;
    virtual void revert(geom::Mesh& mesh) = 0;
    // Numele din meniul Edit („Undo Line”, „Undo Push/Pull”).
    [[nodiscard]] virtual std::string name() const = 0;
};

// Comanda obișnuită: schimbările înregistrate de Mesh în timpul unei tranzacții.
class RecordedCommand final : public Command {
public:
    RecordedCommand(std::string name, geom::ChangeSet changes)
        : m_name(std::move(name))
        , m_changes(std::move(changes))
    {
    }

    void apply(geom::Mesh& mesh) override { mesh.apply(m_changes, geom::ChangeDirection::Forward); }
    void revert(geom::Mesh& mesh) override { mesh.apply(m_changes, geom::ChangeDirection::Backward); }
    [[nodiscard]] std::string name() const override { return m_name; }
    [[nodiscard]] const geom::ChangeSet& changes() const { return m_changes; }

private:
    std::string m_name;
    geom::ChangeSet m_changes;
};

// Istoricul undo/redo al unui Mesh.
//
// Uneltele interactive deschid o tranzacție (beginTransaction), modifică geometria pe măsură ce
// utilizatorul trage, apoi fac commit() la click-ul final sau abort() la Esc.
class CommandStack {
public:
    explicit CommandStack(geom::Mesh& mesh);

    void beginTransaction(std::string name);
    [[nodiscard]] bool inTransaction() const { return m_transactionName.has_value(); }
    // Încheie tranzacția; întoarce false dacă nu s-a schimbat nimic (nu se adaugă în istoric).
    bool commit();
    // Anulează tot ce s-a făcut în tranzacție.
    void abort();

    // Rulează `fn` într-o tranzacție și o încheie.
    template <class Fn>
    bool run(std::string name, Fn&& fn)
    {
        beginTransaction(std::move(name));
        fn(m_mesh);
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

    // Verificarea modelului după fiecare commit / undo / redo (implicit activă în debug).
    // La eșec se apelează `onInvalid`; implicit se oprește programul (assert).
    void setValidation(bool enabled, geom::ValidationLevel level = geom::ValidationLevel::Topology);
    void setOnInvalid(std::function<void(const geom::ValidationReport&)> handler);

    // Apelat după orice schimbare a istoricului (pentru meniul Edit din UI).
    void setOnChanged(std::function<void()> handler) { m_onChanged = std::move(handler); }

private:
    void afterChange();

    geom::Mesh& m_mesh;
    std::vector<std::unique_ptr<Command>> m_undo;
    std::vector<std::unique_ptr<Command>> m_redo;
    std::optional<std::string> m_transactionName;

    bool m_validate;
    geom::ValidationLevel m_validationLevel = geom::ValidationLevel::Topology;
    std::function<void(const geom::ValidationReport&)> m_onInvalid;
    std::function<void()> m_onChanged;
};

} // namespace forma::commands
