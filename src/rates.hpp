#pragma once
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using Day = std::chrono::sys_days;

class Rates {
public:
    void add(const std::string& code, Day from, double value);
    std::optional<double> value(const std::string& code, Day date) const;

private:
    std::map<std::string, std::vector<std::pair<Day, double>>> series_;
};