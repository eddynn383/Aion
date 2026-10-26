#include "forma/core/text/FuzzyMatch.h"

#include <algorithm>
#include <limits>

namespace forma::text {

namespace {

// Decodează UTF-8 în code point-uri. Octeții invalizi devin U+FFFD.
std::u32string decodeUtf8(std::string_view s)
{
    std::u32string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        int extra = 0;
        char32_t cp = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            extra = 2;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            extra = 3;
        } else {
            out.push_back(U'�');
            ++i;
            continue;
        }
        if (i + static_cast<std::size_t>(extra) >= s.size()) {
            out.push_back(U'�'); // secvență trunchiată la final
            break;
        }
        bool valid = true;
        for (int k = 1; k <= extra; ++k) {
            const auto cc = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
            if ((cc & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            cp = (cp << 6) | (cc & 0x3F);
        }
        out.push_back(valid ? cp : U'�');
        i += valid ? static_cast<std::size_t>(extra) + 1 : 1;
    }
    return out;
}

// Litere mici, fără diacritice românești.
char32_t fold(char32_t c)
{
    switch (c) {
    case U'ă': case U'â': case U'Ă': case U'Â':
        return U'a';
    case U'î': case U'Î':
        return U'i';
    case U'ș': case U'ş': case U'Ș': case U'Ş':
        return U's';
    case U'ț': case U'ţ': case U'Ț': case U'Ţ':
        return U't';
    default:
        break;
    }
    if (c >= U'A' && c <= U'Z') {
        return c - U'A' + U'a';
    }
    return c;
}

bool isSeparator(char32_t c)
{
    return c == U' ' || c == U'-' || c == U'_' || c == U'/' || c == U'.' || c == U'(' || c == U'·';
}

bool isUpper(char32_t c)
{
    return (c >= U'A' && c <= U'Z') || c == U'Ă' || c == U'Â' || c == U'Î' || c == U'Ș' || c == U'Ț';
}

// Punctaje (alese empiric; contează doar ordinea relativă a rezultatelor).
constexpr int kMatch = 16;
constexpr int kTextStart = 32;
constexpr int kWordStart = 24;
constexpr int kConsecutive = 20;
constexpr int kGapPenalty = 1;
constexpr int kMaxGapPenalty = 12;
constexpr int kLeadingGapPenalty = 1;
constexpr int kMaxLeadingGapPenalty = 10;

constexpr int kNone = std::numeric_limits<int>::min() / 2;

} // namespace

std::optional<FuzzyMatch> fuzzyMatch(std::string_view query, std::string_view text)
{
    std::u32string q;
    for (char32_t c : decodeUtf8(query)) {
        if (c != U' ') {
            q.push_back(fold(c));
        }
    }
    if (q.empty()) {
        return FuzzyMatch {};
    }

    const std::u32string original = decodeUtf8(text);
    std::u32string t(original.size(), U'\0');
    std::transform(original.begin(), original.end(), t.begin(), fold);

    const std::size_t n = q.size();
    const std::size_t m = t.size();
    if (n > m) {
        return std::nullopt;
    }

    // Bonusul pentru potrivirea la poziția j, independent de restul.
    std::vector<int> positionBonus(m);
    for (std::size_t j = 0; j < m; ++j) {
        if (j == 0) {
            positionBonus[j] = kTextStart;
        } else if (isSeparator(original[j - 1])) {
            positionBonus[j] = kWordStart;
        } else if (isUpper(original[j]) && !isUpper(original[j - 1])) {
            positionBonus[j] = kWordStart;
        } else {
            positionBonus[j] = 0;
        }
    }

    // best[i][j]: scorul maxim când caracterul i din interogare e potrivit la poziția j.
    std::vector<std::vector<int>> best(n, std::vector<int>(m, kNone));
    std::vector<std::vector<std::size_t>> parent(n, std::vector<std::size_t>(m, 0));

    for (std::size_t j = 0; j < m; ++j) {
        if (t[j] == q[0]) {
            const int leading = std::min(kMaxLeadingGapPenalty, static_cast<int>(j) * kLeadingGapPenalty);
            best[0][j] = kMatch + positionBonus[j] - leading;
        }
    }
    for (std::size_t i = 1; i < n; ++i) {
        for (std::size_t j = i; j < m; ++j) {
            if (t[j] != q[i]) {
                continue;
            }
            int bestPrev = kNone;
            std::size_t bestK = 0;
            for (std::size_t k = i - 1; k < j; ++k) {
                if (best[i - 1][k] == kNone) {
                    continue;
                }
                const std::size_t gap = j - k - 1;
                const int transition = gap == 0
                    ? kConsecutive
                    : -std::min(kMaxGapPenalty, static_cast<int>(gap) * kGapPenalty);
                const int candidate = best[i - 1][k] + transition;
                if (candidate > bestPrev) {
                    bestPrev = candidate;
                    bestK = k;
                }
            }
            if (bestPrev != kNone) {
                best[i][j] = bestPrev + kMatch + positionBonus[j];
                parent[i][j] = bestK;
            }
        }
    }

    int bestScore = kNone;
    std::size_t bestEnd = 0;
    for (std::size_t j = n - 1; j < m; ++j) {
        if (best[n - 1][j] > bestScore) {
            bestScore = best[n - 1][j];
            bestEnd = j;
        }
    }
    if (bestScore == kNone) {
        return std::nullopt;
    }

    FuzzyMatch result;
    result.score = bestScore;
    result.positions.resize(n);
    std::size_t j = bestEnd;
    for (std::size_t i = n; i-- > 0;) {
        result.positions[i] = j;
        j = parent[i][j];
    }
    return result;
}

} // namespace forma::text
