#include "aion/core/io/Recovery.h"

#include "aion/core/io/AionFile.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

namespace aion::io {

namespace {

using nlohmann::json;

std::filesystem::path sessionPath(const std::filesystem::path& dir, std::string_view session, std::string_view ext)
{
    std::u8string name(session.begin(), session.end());
    name.append(ext.begin(), ext.end());
    return dir / std::filesystem::path(name);
}

std::int64_t modifiedTime(const std::filesystem::path& path)
{
    std::error_code ec;
    const auto time = std::filesystem::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    // file_clock nu are conversie portabilă la system_clock în C++20 (clock_cast lipsește în libc++):
    // diferența față de „acum” e aceeași pe ambele ceasuri.
    const auto age = std::filesystem::file_time_type::clock::now() - time;
    const auto then = std::chrono::system_clock::now() - std::chrono::duration_cast<std::chrono::system_clock::duration>(age);
    return std::chrono::duration_cast<std::chrono::seconds>(then.time_since_epoch()).count();
}

std::optional<std::string> readText(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

std::filesystem::path recoveryModelPath(const std::filesystem::path& dir, std::string_view session)
{
    return sessionPath(dir, session, ".aion");
}

std::filesystem::path recoveryInfoPath(const std::filesystem::path& dir, std::string_view session)
{
    return sessionPath(dir, session, ".json");
}

std::filesystem::path recoveryLockPath(const std::filesystem::path& dir, std::string_view session)
{
    return sessionPath(dir, session, ".lock");
}

std::string recoveryInfoToJson(const RecoveryInfo& info)
{
    return json { { "source", info.source }, { "savedAt", info.savedAt } }.dump(2);
}

std::optional<RecoveryInfo> recoveryInfoFromJson(std::string_view text)
{
    const json j = json::parse(text, nullptr, false);
    if (!j.is_object() || !j.contains("savedAt") || !j["savedAt"].is_number_integer()) {
        return std::nullopt;
    }
    RecoveryInfo info;
    info.savedAt = j["savedAt"].get<std::int64_t>();
    if (j.contains("source") && j["source"].is_string()) {
        info.source = j["source"].get<std::string>();
    }
    return info;
}

bool writeRecovery(const std::filesystem::path& dir, std::string_view session, const model::Model& model,
    const RecoveryInfo& info, std::span<const std::uint8_t> thumbnailPng, std::string* error)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (!saveFile(recoveryModelPath(dir, session), model, thumbnailPng, error)) {
        return false;
    }
    const std::filesystem::path path = recoveryInfoPath(dir, session);
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        const std::string text = recoveryInfoToJson(info);
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!out) {
            if (error != nullptr) {
                *error = "Informațiile copiei nu au putut fi scrise.";
            }
            return false;
        }
    }
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        if (error != nullptr) {
            *error = "Informațiile copiei nu au putut fi înlocuite.";
        }
        return false;
    }
    return true;
}

std::vector<RecoveryEntry> listRecoveries(
    const std::filesystem::path& dir, const std::function<bool(const std::string& session)>& include)
{
    std::vector<RecoveryEntry> out;
    std::error_code ec;
    // Ordinea din dosar nu e garantată: sesiunile, sortate după nume, ca `include` să fie chemat mereu la fel.
    std::map<std::string, std::filesystem::path> models;
    for (const auto& item : std::filesystem::directory_iterator(dir, ec)) {
        if (item.is_regular_file(ec) && item.path().extension() == ".aion") {
            const std::u8string stem = item.path().stem().u8string();
            models.emplace(std::string(stem.begin(), stem.end()), item.path());
        }
    }
    for (const auto& [session, model] : models) {
        if (include && !include(session)) {
            continue;
        }
        RecoveryEntry entry { session, model, {} };
        const std::optional<std::string> text = readText(recoveryInfoPath(dir, session));
        if (const std::optional<RecoveryInfo> info = text ? recoveryInfoFromJson(*text) : std::nullopt) {
            entry.info = *info;
        } else {
            entry.info.savedAt = modifiedTime(model);
        }
        out.push_back(std::move(entry));
    }
    std::stable_sort(out.begin(), out.end(),
        [](const RecoveryEntry& a, const RecoveryEntry& b) { return a.info.savedAt > b.info.savedAt; });
    return out;
}

void removeRecovery(const std::filesystem::path& dir, std::string_view session)
{
    std::error_code ec;
    std::filesystem::remove(recoveryModelPath(dir, session), ec);
    std::filesystem::remove(recoveryInfoPath(dir, session), ec);
}

std::vector<std::string> recoveriesToPrune(
    std::span<const RecoveryEntry> entries, std::size_t maxCount, std::int64_t maxAgeSeconds, std::int64_t now)
{
    std::vector<std::string> out;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (i >= maxCount || now - entries[i].info.savedAt > maxAgeSeconds) {
            out.push_back(entries[i].session);
        }
    }
    return out;
}

} // namespace aion::io
