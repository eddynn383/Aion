#pragma once

#include <QJSValue>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace aion::render {

class Document;

// API-ul scripturilor, global `aion` în JavaScript (vezi docs/scripting.md). Coordonatele sunt în mm, în contextul
// editat; id-urile sunt ale obiectelor (instanțelor). Fiecare funcție modifică documentul prin Document, deci totul
// intră în pasul de undo al rulării.
class ScriptApi : public QObject {
    Q_OBJECT
public:
    explicit ScriptApi(Document* document, QObject* parent = nullptr);

    Q_INVOKABLE void log(const QJSValue& value);
    Q_INVOKABLE void message(const QString& text);
    Q_INVOKABLE double box(double x, double y, double z, double width, double depth, double height, const QString& name = {});
    Q_INVOKABLE double cylinder(double x, double y, double z, double diameter, double height, const QString& name = {});
    Q_INVOKABLE double cone(double x, double y, double z, double diameter, double height, const QString& name = {});
    Q_INVOKABLE double sphere(double x, double y, double z, double diameter, const QString& name = {});
    Q_INVOKABLE double object(const QString& generator, const QVariantMap& parameters, double x, double y, double z);
    Q_INVOKABLE bool place(double id, const QVariantMap& placement);
    Q_INVOKABLE bool move(double id, double dx, double dy, double dz);
    // În jurul centrului obiectului sau al punctului `pivot` ([x, y, z]).
    Q_INVOKABLE bool rotate(double id, double degrees, const QString& axis = QStringLiteral("z"),
        const QVariantList& pivot = {});
    Q_INVOKABLE bool remove(double id);
    Q_INVOKABLE bool material(double id, const QString& nameOrColor);
    Q_INVOKABLE bool line(const QVariantList& from, const QVariantList& to);
    Q_INVOKABLE bool face(const QVariantList& points);
    Q_INVOKABLE QVariantList objects() const;
    Q_INVOKABLE QVariantList selection() const;
    Q_INVOKABLE void select(const QVariantList& ids);
    Q_INVOKABLE void command(const QString& id);

signals:
    void output(const QString& text);
    void messageRequested(const QString& text);
    void commandRequested(const QString& id);

private:
    QPointer<Document> m_document;
};

// Rulează scripturi JavaScript (QJSEngine) peste document și încarcă plugin-urile (dosare cu manifest.json și
// main.js). O rulare = un singur pas de undo; o eroare anulează tot ce a făcut scriptul. ADR 0027.
class ScriptEngine : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(aion::render::Document* document READ document WRITE setDocument NOTIFY documentChanged)
    // Plugin-urile: { id, name, version, description, commands: [{ id, title }] }.
    Q_PROPERTY(QVariantList plugins READ plugins NOTIFY pluginsChanged)
    // Exemplele incluse: { name, code }.
    Q_PROPERTY(QVariantList examples READ examples CONSTANT)

public:
    explicit ScriptEngine(QObject* parent = nullptr);

    [[nodiscard]] Document* document() const { return m_document; }
    void setDocument(Document* document);
    [[nodiscard]] QVariantList plugins() const { return m_plugins; }
    [[nodiscard]] QVariantList examples() const;

    // { ok, output, error, line }.
    Q_INVOKABLE QVariantMap run(const QString& code, const QString& name = QStringLiteral("Script"));
    // „plugin/comandă”: încarcă main.js al plugin-ului și apelează funcția comenzii.
    Q_INVOKABLE QVariantMap runPluginCommand(const QString& commandId);
    Q_INVOKABLE QString pluginFolder() const;
    Q_INVOKABLE void reloadPlugins();

signals:
    void documentChanged();
    void pluginsChanged();
    void messageRequested(const QString& text);
    void commandRequested(const QString& id);

private:
    QPointer<Document> m_document;
    QVariantList m_plugins;
};

} // namespace aion::render
