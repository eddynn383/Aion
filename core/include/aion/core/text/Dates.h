#pragma once

#include <string>

// Momente scrise pentru oameni, în română (ecranul de start: documentele recente, copiile de recuperare).
namespace aion::text {

// Un moment în ora locală (luna 1–12).
struct LocalTime {
    int year = 1970;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
};

// „chiar acum”, „acum 5 min”, „azi, 14:32”, „ieri, 09:05”, „12 oct.”, „12 oct. 2025” (anul doar dacă nu e cel
// curent). Un moment din viitor (ceasuri nesincronizate) se scrie ca dată, fără „acum”.
[[nodiscard]] std::string describeTime(const LocalTime& when, const LocalTime& now);

} // namespace aion::text
