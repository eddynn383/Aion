#pragma once

#include "aion/core/io/AionFile.h"
#include "aion/core/model/Model.h"

#include <QDateTime>
#include <QString>

#include <map>
#include <memory>
#include <vector>

class QLockFile;

namespace aion::render {

// Copiile de recuperare ale aplicației (ADR 0040), peste `io::Recovery`. Sesiunea curentă își scrie copia în
// dosarul „Recuperare” și ține un lock cât rulează; la o închidere normală își șterge copia. Copiile sesiunilor care
// nu mai rulează (închideri bruște) se oferă la pornire; cât sunt oferite, lock-ul lor e al nostru, ca altă instanță
// pornită între timp să nu le ofere și ea. Pe web (fără procese și fără fișiere care persistă) copiile stau și în
// QSettings, iar lock-urile lipsesc.
class RecoveryStore {
public:
    struct Entry {
        QString session;
        QString source; // fișierul documentului; gol = nesalvat niciodată
        QDateTime savedAt;
    };

    RecoveryStore();
    ~RecoveryStore();
    RecoveryStore(const RecoveryStore&) = delete;
    RecoveryStore& operator=(const RecoveryStore&) = delete;

    // Copia sesiunii curente (`source` = fișierul documentului, dacă are).
    bool save(const model::Model& model, const QString& source, QString* error = nullptr);
    void clear();

    // Copiile rămase de la alte sesiuni, cele mai noi întâi.
    const std::vector<Entry>& orphans() const { return m_orphans; }
    [[nodiscard]] io::LoadResult load(const QString& session) const;
    void discard(const QString& session);

private:
    void migrateLegacy();
    void restoreFromSettings();
    void collectOrphans();
    bool claim(const QString& session);

    QString m_dir;
    QString m_session;
    std::unique_ptr<QLockFile> m_lock;
    std::map<QString, std::unique_ptr<QLockFile>> m_claimed;
    std::vector<Entry> m_orphans;
};

} // namespace aion::render
