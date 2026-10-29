#include "aion/core/text/Dates.h"

#include <array>
#include <chrono>
#include <cstdio>

namespace aion::text {

namespace {

long long dayNumber(const LocalTime& t)
{
    using namespace std::chrono;
    const year_month_day date { year { t.year }, month { static_cast<unsigned>(t.month) },
        day { static_cast<unsigned>(t.day) } };
    return sys_days(date).time_since_epoch().count();
}

std::string clock(const LocalTime& t)
{
    std::array<char, 8> buf {};
    std::snprintf(buf.data(), buf.size(), "%02d:%02d", t.hour, t.minute);
    return buf.data();
}

} // namespace

std::string describeTime(const LocalTime& when, const LocalTime& now)
{
    static constexpr std::array<const char*, 12> kMonths { "ian.", "feb.", "mar.", "apr.", "mai", "iun.", "iul.",
        "aug.", "sept.", "oct.", "nov.", "dec." };
    const long long days = dayNumber(now) - dayNumber(when);
    const long long minutes = days * 24 * 60 + (now.hour * 60 + now.minute) - (when.hour * 60 + when.minute);
    if (minutes >= 0 && minutes < 1) {
        return "chiar acum";
    }
    if (minutes >= 1 && minutes < 60) {
        return "acum " + std::to_string(minutes) + " min";
    }
    if (days == 0) {
        return "azi, " + clock(when);
    }
    if (days == 1) {
        return "ieri, " + clock(when);
    }
    std::string text = std::to_string(when.day) + " " + kMonths.at(static_cast<std::size_t>((when.month + 11) % 12));
    if (when.year != now.year) {
        text += " " + std::to_string(when.year);
    }
    return text;
}

} // namespace aion::text
