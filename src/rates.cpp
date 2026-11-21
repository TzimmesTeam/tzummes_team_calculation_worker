#include "rates.hpp"

#include <algorithm>

void Rates::add(const std::string& code, Day from, double value) {
    auto& v = series_[code];
    auto pos = std::upper_bound(v.begin(), v.end(), from,[](Day d, const auto& e) { return d < e.first; });
    v.insert(pos, { from, value });
}

std::optional<double> Rates::value(const std::string& code, Day date) const {
    auto it = series_.find(code);
    if (it == series_.end())
        return std::nullopt;
    const auto& v = it->second;
    auto p = std::upper_bound(v.begin(), v.end(), date,[](Day d, const auto& e) { return d < e.first; });
    if (p == v.begin()) 
        return std::nullopt;
    return std::prev(p)->second;
}