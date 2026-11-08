#include "text_util.h"

#include <charconv>
#include <cmath>
#include <system_error>

namespace mykam {

namespace {
bool isBlank(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
}  // namespace

std::string trim(const std::string& s) {
    std::size_t begin = 0;
    std::size_t end = s.size();
    while (begin < end && isBlank(s[begin])) {
        ++begin;
    }
    while (end > begin && isBlank(s[end - 1])) {
        --end;
    }
    return s.substr(begin, end - begin);
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '\n' || c == '\r') {
            lines.push_back(current);
            current.clear();
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

bool parseDouble(const std::string& s, double& out) {
    const std::string t = trim(s);
    if (t.empty()) {
        return false;
    }
    // std::from_chars does not accept a leading '+', so allow one explicitly.
    const char* first = t.data();
    const char* last = t.data() + t.size();
    if (*first == '+') {
        ++first;
        if (first == last || *first == '-' || *first == '+') {
            return false;
        }
    }
    double value = 0.0;
    const std::from_chars_result r = std::from_chars(first, last, value);
    if (r.ec != std::errc() || r.ptr != last) {
        return false;
    }
    if (!std::isfinite(value)) {
        return false;
    }
    out = value;
    return true;
}

std::string formatDouble(double value) {
    char buffer[64];
    const std::to_chars_result r = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (r.ec != std::errc()) {
        return "0";
    }
    return std::string(buffer, r.ptr);
}

std::string formatCompact(double value, int decimals) {
    char buffer[64];
    const std::to_chars_result r =
        std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::fixed, decimals);
    if (r.ec != std::errc()) {
        return "0";
    }
    std::string s(buffer, r.ptr);
    if (s.find('.') != std::string::npos) {
        while (!s.empty() && s.back() == '0') {
            s.pop_back();
        }
        if (!s.empty() && s.back() == '.') {
            s.pop_back();
        }
    }
    if (s == "-0" || s.empty()) {
        s = "0";
    }
    return s;
}

}  // namespace mykam
