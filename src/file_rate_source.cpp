#include "file_rate_source.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "json.hpp"

namespace {

Day parse_day(const std::string &s) {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-')
    throw std::runtime_error("");

  int y = std::stoi(s.substr(0, 4));
  int m = std::stoi(s.substr(5, 2));
  int d = std::stoi(s.substr(8, 2));

  std::chrono::year_month_day ymd{std::chrono::year{y},
                                  std::chrono::month{static_cast<unsigned>(m)},
                                  std::chrono::day{static_cast<unsigned>(d)}};

  if (!ymd.ok())
    throw std::runtime_error("");

  return Day{ymd};
}

} // namespace

Rates JsonFileRateSource::load() {
  std::ifstream in(file_);
  if (!in)
    throw std::runtime_error("");
  std::stringstream buf;
  buf << in.rdbuf();

  Json root = parse_json(buf.str());
  if (root.kind != Json::Object)
    throw std::runtime_error("");

  Rates rates;
  for (const auto &[code, arr] : root.fields) {
    if (arr.kind != Json::Array)
      throw std::runtime_error("");
    for (const Json &item : arr.items) {
      auto from = item.fields.find("from");
      auto value = item.fields.find("value");
      if (from == item.fields.end() || value == item.fields.end())
        throw std::runtime_error("");
      rates.add(code, parse_day(from->second.text), value->second.number);
    }
  }
  return rates;
}