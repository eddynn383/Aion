#include "RecoveryStore.h"

#include "aion/core/io/Recovery.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QLockFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace aion::render {

namespace {

// Câte copii păstrăm și cât timp (cele ignorate se oferă la fiecare pornire până la Recuperează / Renunță).
#ifdef Q_OS_WASM
constexpr std::size_t kMaxRecoveries = 2; // în localStorage, unde locul e puțin
#else
constexpr std::size_t kMaxRecoveries = 5;
#endif
constexpr std::int64_t kMaxAgeSeconds = 30LL * 24 * 3600;

// Copia unică din versiunile de dinainte de ADR 0040.
const QString kLegacySourceKey = QStringLiteral("recovery/source");
const QString kLegacyTimeKey = QStringLiteral("recovery/time");
const QString kLegacyDataKey = QStringLiteral("recovery/data");
// Web: copiile, ca `recovery/sessions/<sesiune>/data` (arhiva) și `.../info` (JSON-ul).
const QString kSessionsGroup = QStringLiteral("recovery/sessions");

std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

QString fromPath(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

#ifndef Q_OS_WASM
QString lockFile(const QString& dir, const QString& session)
{
    return fromPath(io::recoveryLockPath(toPath(dir), session.toStdString()));
}
#endif

std::int64_t nowSeconds()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

#ifdef Q_OS_WASM
QByteArray readAll(const std::filesystem::path& path)
{
    QFile file(fromPath(path));
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray {};
}
#endif

bool writeAll(const std::filesystem::path& path, const QByteArray& bytes)
{
    QFile file(fromPath(path));
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate) && file.write(bytes) == bytes.size();
}

} // namespace

RecoveryStore::RecoveryStore()
    : m_dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/Recuperare"))
    , m_session(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-"))
          + QString::number(QCoreApplication::applicationPid()))
{
    QDir().mkpath(m_dir);
#ifndef Q_OS_WASM
    m_lock = std::make_unique<QLockFile>(lockFile(m_dir, m_session));
    m_lock->tryLock(0);
#endif
    migrateLegacy();
    restoreFromSettings();
    collectOrphans();
}

RecoveryStore::~RecoveryStore()
{
    // O închidere normală: utilizatorul a ales deja ce face cu modificările.
    clear();
}

void RecoveryStore::migrateLegacy()
{
    QSettings settings;
    const QString legacy
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/autosave.aion");
    const QString session = QStringLiteral("anterioara");
    const std::filesystem::path model = io::recoveryModelPath(toPath(m_dir), session.toStdString());
    bool moved = QFile::exists(legacy) && QFile::rename(legacy, fromPath(model));
#ifdef Q_OS_WASM
    const QByteArray data = settings.value(kLegacyDataKey).toByteArray();
    moved = moved || (!data.isEmpty() && writeAll(model, data));
#endif
    if (moved) {
        const QDateTime time = settings.value(kLegacyTimeKey).toDateTime();
        const io::RecoveryInfo info { settings.value(kLegacySourceKey).toString().toStdString(),
            time.isValid() ? time.toSecsSinceEpoch() : nowSeconds() };
        const std::string json = io::recoveryInfoToJson(info);
        writeAll(io::recoveryInfoPath(toPath(m_dir), session.toStdString()), QByteArray::fromStdString(json));
#ifdef Q_OS_WASM
        const QString key = kSessionsGroup + QLatin1Char('/') + session;
        settings.setValue(key + QStringLiteral("/data"), readAll(model));
        settings.setValue(key + QStringLiteral("/info"), QString::fromStdString(json));
#endif
    }
    settings.remove(kLegacySourceKey);
    settings.remove(kLegacyTimeKey);
    settings.remove(kLegacyDataKey);
}

void RecoveryStore::restoreFromSettings()
{
#ifdef Q_OS_WASM
    QSettings settings;
    settings.beginGroup(kSessionsGroup);
    for (const QString& session : settings.childGroups()) {
        const std::string id = session.toStdString();
        const std::filesystem::path model = io::recoveryModelPath(toPath(m_dir), id);
        if (!std::filesystem::exists(model)) {
            writeAll(model, settings.value(session + QStringLiteral("/data")).toByteArray());
            const QByteArray info = settings.value(session + QStringLiteral("/info")).toString().toUtf8();
            writeAll(io::recoveryInfoPath(toPath(m_dir), id), info);
        }
    }
    settings.endGroup();
#endif
}

bool RecoveryStore::claim(const QString& session)
{
    if (session == m_session) {
        return false;
    }
#ifdef Q_OS_WASM
    return true;
#else
    // Lock-ul unei sesiuni care rulează nu e niciodată „vechi” după vârstă (staleLockTime 0): doar un proces mort
    // îl lasă liber. Fără lock deloc (copia din versiunile vechi) îl luăm direct.
    auto lock = std::make_unique<QLockFile>(lockFile(m_dir, session));
    lock->setStaleLockTime(0);
    if (!lock->tryLock(0)) {
        return false;
    }
    m_claimed[session] = std::move(lock);
    return true;
#endif
}

void RecoveryStore::collectOrphans()
{
    const std::vector<io::RecoveryEntry> entries = io::listRecoveries(
        toPath(m_dir), [this](const std::string& session) { return claim(QString::fromStdString(session)); });
    const std::vector<std::string> pruned = io::recoveriesToPrune(entries, kMaxRecoveries, kMaxAgeSeconds, nowSeconds());
    for (const std::string& session : pruned) {
        discard(QString::fromStdString(session));
    }
    m_orphans.clear();
    for (const io::RecoveryEntry& e : entries) {
        if (std::find(pruned.begin(), pruned.end(), e.session) == pruned.end()) {
            m_orphans.push_back({ QString::fromStdString(e.session), QString::fromStdString(e.info.source),
                QDateTime::fromSecsSinceEpoch(e.info.savedAt) });
        }
    }
#ifndef Q_OS_WASM
    // Lock-urile sesiunilor moarte care n-au apucat să scrie o copie: nimic de oferit, doar le ștergem.
    for (const QFileInfo& file : QDir(m_dir).entryInfoList({ QStringLiteral("*.lock") }, QDir::Files)) {
        const QString session = file.completeBaseName();
        if (session != m_session && !m_claimed.contains(session)
            && !std::filesystem::exists(io::recoveryModelPath(toPath(m_dir), session.toStdString()))) {
            QLockFile lock(file.absoluteFilePath());
            lock.setStaleLockTime(0);
            if (lock.tryLock(0)) {
                lock.unlock(); // șterge fișierul
            }
        }
    }
#endif
}

bool RecoveryStore::save(const model::Model& model, const QString& source, QString* error)
{
    std::string message;
    const io::RecoveryInfo info { source.toStdString(), nowSeconds() };
    if (!io::writeRecovery(toPath(m_dir), m_session.toStdString(), model, info, {}, &message)) {
        if (error != nullptr) {
            *error = QString::fromStdString(message);
        }
        return false;
    }
#ifdef Q_OS_WASM
    QSettings settings;
    settings.setValue(kSessionsGroup + QLatin1Char('/') + m_session + QStringLiteral("/data"),
        readAll(io::recoveryModelPath(toPath(m_dir), m_session.toStdString())));
    settings.setValue(kSessionsGroup + QLatin1Char('/') + m_session + QStringLiteral("/info"),
        QString::fromStdString(io::recoveryInfoToJson(info)));
#endif
    return true;
}

void RecoveryStore::clear()
{
    io::removeRecovery(toPath(m_dir), m_session.toStdString());
#ifdef Q_OS_WASM
    QSettings().remove(kSessionsGroup + QLatin1Char('/') + m_session);
#endif
}

io::LoadResult RecoveryStore::load(const QString& session) const
{
    return io::loadFile(io::recoveryModelPath(toPath(m_dir), session.toStdString()));
}

void RecoveryStore::discard(const QString& session)
{
    io::removeRecovery(toPath(m_dir), session.toStdString());
#ifdef Q_OS_WASM
    QSettings().remove(kSessionsGroup + QLatin1Char('/') + session);
#endif
    m_claimed.erase(session); // QLockFile își șterge fișierul la distrugere
    std::erase_if(m_orphans, [&](const Entry& e) { return e.session == session; });
}

} // namespace aion::render
