#include "aion/core/units/Length.h"

#include <cmath>
#include <cstdio>

namespace aion::units {

namespace {

constexpr double kMmPerInch = 25.4;
constexpr double kMmPerFoot = 304.8;

class Parser {
public:
    explicit Parser(std::string_view text)
        : m_text(text)
    {
    }

    void skipSpaces()
    {
        while (m_pos < m_text.size() && (m_text[m_pos] == ' ' || m_text[m_pos] == '\t')) {
            ++m_pos;
        }
    }

    [[nodiscard]] bool atEnd() const { return m_pos >= m_text.size(); }

    bool accept(char c)
    {
        if (!atEnd() && m_text[m_pos] == c) {
            ++m_pos;
            return true;
        }
        return false;
    }

    bool acceptWord(std::string_view word)
    {
        if (m_text.substr(m_pos, word.size()) == word) {
            m_pos += word.size();
            return true;
        }
        return false;
    }

    // Un număr fără semn: „12”, „12.5”, „.5”. Fără exponent (nu are sens pentru lungimi tastate).
    std::optional<double> number()
    {
        double value = 0.0;
        bool digits = false;
        while (!atEnd() && m_text[m_pos] >= '0' && m_text[m_pos] <= '9') {
            value = value * 10.0 + (m_text[m_pos] - '0');
            digits = true;
            ++m_pos;
        }
        if (accept('.')) {
            double scale = 0.1;
            while (!atEnd() && m_text[m_pos] >= '0' && m_text[m_pos] <= '9') {
                value += (m_text[m_pos] - '0') * scale;
                scale *= 0.1;
                digits = true;
                ++m_pos;
            }
        }
        return digits ? std::optional(value) : std::nullopt;
    }

private:
    std::string_view m_text;
    std::size_t m_pos = 0;
};

std::string_view trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

std::string trimZeros(std::string s)
{
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    return s;
}

} // namespace

std::optional<double> parseLength(std::string_view text)
{
    Parser p(trim(text));
    p.skipSpaces();
    const bool negative = p.accept('-');
    if (!negative) {
        p.accept('+');
    }
    p.skipSpaces();
    const auto first = p.number();
    if (!first) {
        return std::nullopt;
    }
    p.skipSpaces();

    double mm = 0.0;
    if (p.accept('\'')) {
        // Picioare, opțional urmate de inci: 3'6", 3' 6", 3'6
        mm = *first * kMmPerFoot;
        p.skipSpaces();
        if (!p.atEnd()) {
            const auto inches = p.number();
            if (!inches) {
                return std::nullopt;
            }
            p.skipSpaces();
            p.accept('"');
            mm += *inches * kMmPerInch;
        }
    } else if (p.accept('"')) {
        mm = *first * kMmPerInch;
    } else if (p.acceptWord("mm")) {
        mm = *first;
    } else if (p.acceptWord("cm")) {
        mm = *first * 10.0;
    } else if (p.acceptWord("km")) {
        mm = *first * 1'000'000.0;
    } else if (p.acceptWord("m")) {
        mm = *first * 1000.0;
    } else if (p.acceptWord("in")) {
        mm = *first * kMmPerInch;
    } else if (p.acceptWord("ft")) {
        mm = *first * kMmPerFoot;
    } else {
        mm = *first;
    }
    p.skipSpaces();
    if (!p.atEnd()) {
        return std::nullopt;
    }
    return negative ? -mm : mm;
}

std::optional<std::vector<std::optional<double>>> parseLengths(std::string_view text, std::size_t maxCount)
{
    std::vector<std::optional<double>> out;
    bool any = false;
    while (true) {
        const std::size_t sep = text.find_first_of(",;");
        const std::string_view field = trim(text.substr(0, sep));
        if (field.empty()) {
            out.emplace_back(std::nullopt);
        } else {
            const auto value = parseLength(field);
            if (!value) {
                return std::nullopt;
            }
            out.emplace_back(*value);
            any = true;
        }
        if (sep == std::string_view::npos) {
            break;
        }
        text.remove_prefix(sep + 1);
    }
    if (!any || out.size() > maxCount) {
        return std::nullopt;
    }
    return out;
}

std::string formatLength(double mm)
{
    char buffer[64];
    const double a = std::abs(mm);
    if (a >= 1000.0) {
        std::snprintf(buffer, sizeof buffer, "%.3f", mm / 1000.0);
        return trimZeros(buffer) + " m";
    }
    std::snprintf(buffer, sizeof buffer, "%.1f", mm);
    std::string s = trimZeros(buffer);
    if (s == "-0") {
        s = "0";
    }
    return s + " mm";
}

} // namespace aion::units
