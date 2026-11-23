#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "calc.hpp"
#include "file_rate_source.hpp"
#include "rates.hpp"

using namespace std::chrono;
namespace fs = std::filesystem;
using namespace std::chrono;

namespace {

std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t p = s.find(sep, start);
    out.push_back(s.substr(start, p == std::string::npos ? std::string::npos
                                                         : p - start));
    if (p == std::string::npos)
      break;
    start = p + 1;
  }
  return out;
}

std::string trim(const std::string &s) {
  size_t b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos)
    return "";
  return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

long long to_int(const std::string &s, const char *name) {
  try {
    size_t pos = 0;
    long long v = std::stoll(s, &pos);
    if (pos == s.size())
      return v;
  } catch (...) {
  }
  throw std::runtime_error(std::string("") + name + ": '" + s + "'");
}

double to_num(const std::string &s, const char *name) {
  try {
    size_t pos = 0;
    double v = std::stod(s, &pos);
    if (pos == s.size())
      return v;
  } catch (...) {
  }
  throw std::runtime_error(std::string("") + name + ": '" + s + "'");
}

std::optional<double> opt_num(const std::string &s, const char *name) {
  if (s.empty())
    return std::nullopt;
  return to_num(s, name);
}

Day day_from_seconds(long long secs) { return Day{days{secs / 86400}}; }

std::optional<Day> opt_day(const std::string &s, const char *name) {
  if (s.empty())
    return std::nullopt;
  return day_from_seconds(to_int(s, name));
}

Rule make_rule(const std::string &start, const std::string &period,
               const std::string &day, const char *name) {
  Rule r;
  r.start = opt_day(start, name);
  if (!period.empty())
    r.period = period_from_code(to_int(period, name));
  if (!day.empty())
    r.day = int(to_int(day, name));
  return r;
}

std::vector<std::string> items(const std::string &s) {
  return s.empty() ? std::vector<std::string>{} : split(s, ',');
}

std::vector<Day> day_list(const std::string &s, const char *name) {
  std::vector<Day> out;
  for (auto &t : items(s))
    out.push_back(day_from_seconds(to_int(t, name)));
  return out;
}

void rate_pairs(const std::string &dates, const std::string &values,
                std::vector<Day> &outDates, std::vector<double> &outValues) {
  auto ds = items(dates), vs = items(values);
  if (ds.size() != vs.size())
    throw std::runtime_error("");
  std::vector<std::pair<Day, double>> pairs;
  for (size_t i = 0; i < ds.size(); ++i)
    pairs.push_back(
        {day_from_seconds(to_int(ds[i], "date")), to_num(vs[i], "rate")});
  std::sort(pairs.begin(), pairs.end(),
            [](auto &a, auto &b) { return a.first < b.first; });
  for (auto &p : pairs) {
    outDates.push_back(p.first);
    outValues.push_back(p.second);
  }
}

DealIn parse_deal(const std::vector<std::string> &f) {
  if (f.size() != 46)
    throw std::runtime_error("");
  DealIn d;
  d.id = to_int(f[0], "deal_id");
  d.report = day_from_seconds(to_int(f[1], "report_dttm"));
  d.maturity = day_from_seconds(to_int(f[2], "maturity_dttm"));
  d.deal_date = opt_day(f[3], "deal_dttm");
  d.disc = int(to_int(f[4], "discriminator"));
  d.rate_type = int(to_int(f[5], "rate_type_cd"));
  d.rate = to_num(f[6], "interest_rate");
  d.base_rate = opt_num(f[7], "base_indicator_rate");
  d.base_indicator = f[8];
  d.int_rule = make_rule(f[9], f[10], f[11], "interest_payment_rule");
  d.day_conv = int(to_int(f[12], "day_convention_cd"));
  d.reg_rule = make_rule(f[13], f[14], f[15], "regular_payment_rule");
  d.rep_rule = make_rule(f[16], f[17], f[18], "repricing_rule");
  d.accrued = opt_num(f[19], "accrued_interest");
  d.balance = to_num(f[20], "current_principal_amt");
  d.int_dates = day_list(f[21], "interest_payment");
  d.prin_dates = day_list(f[22], "principal_payment_dates");
  for (auto &t : items(f[23]))
    d.prin_amounts.push_back(t.empty() ? std::nullopt
                                       : std::optional<double>(to_num(
                                             t, "principal_payment_amounts")));
  d.rep_dates = day_list(f[24], "repricing_dttm_schedule");
  rate_pairs(f[25], f[26], d.rs_dates, d.rs_rates);
  rate_pairs(f[27], f[28], d.rf_dates, d.rf_rates);
  d.cap = opt_num(f[29], "cap");
  d.floor = opt_num(f[30], "floor");
  if (!f[31].empty())
    d.lag = period_from_code(to_int(f[31], "indicator_lag_cd"));
  d.behavior = f[45];

  if (!f[32].empty()) {
    SubsidyIn s;
    s.present = true;
    s.rate_type = int(to_int(f[32], "sub_rate_type_cd"));
    s.rate = to_num(f[33], "sub_interest_rate");
    s.indicator = f[34];
    s.coeff = opt_num(f[35], "sub_coeff_index").value_or(1.0);
    s.pay = make_rule(f[36], f[37], f[38], "sub_payment_rule");
    s.rep = make_rule(f[39], f[40], f[41], "sub_repricing_rule");
    s.cap = opt_num(f[42], "sub_cap");
    s.floor = opt_num(f[43], "sub_floor");
    if (!f[44].empty())
      s.lag = period_from_code(to_int(f[44], "sub_indicator_lag_cd"));
    d.sub = s;
  }
  return d;
}

std::string json_str(const std::string &s) {
  std::string out = "\"";
  for (char c : s) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
    }
  }
  return out + "\"";
}

std::string iso_date(Day d) {
  year_month_day ymd{d};
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04d-%02u-%02u", int(ymd.year()),
                unsigned(ymd.month()), unsigned(ymd.day()));
  return buf;
}

void write_ok(const std::filesystem::path &file, long long id,
              const std::vector<Flow> &flows) {
  std::ofstream out(file);
  out << std::fixed << std::setprecision(6);
  out << "{\n  \"deal_id\": " << id
      << ",\n  \"status\": \"ok\",\n  \"flows\": [\n";
  for (size_t i = 0; i < flows.size(); ++i) {
    const Flow &f = flows[i];
    out << "    {\"date\": " << json_str(iso_date(f.date))
        << ", \"type\": " << json_str(f.type) << ", \"amount\": " << f.amount
        << ", \"balance_after\": " << f.balance_after
        << ", \"days\": " << f.days << "}" << (i + 1 < flows.size() ? "," : "")
        << "\n";
  }
  out << "  ]\n}\n";
}
} // namespace
int main(int argc, char **argv) {
  if (argc > 2) {
    return 2;
  }

  const fs::path exeDir = fs::absolute(argv[0]).parent_path();
  const fs::path outDir = argc == 2 ? fs::path(argv[1]) : exeDir / "result";

  Rates rates;
  try {
    std::unique_ptr<RateSource> source =
        std::make_unique<JsonFileRateSource>(exeDir / "rates.json");
    rates = source->load();
  } catch (const std::exception &) {
    return 1;
  }

  fs::create_directories(outDir);

  std::string line;
  while (std::getline(std::cin, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#')
      continue;

    try {
      DealIn d = parse_deal(split(line, ';'));
      auto flows = calculate(d, rates);
      if (flows.empty())
        continue;
      write_ok(outDir / ("deal_" + std::to_string(d.id) + ".json"), d.id,
               flows);
    } catch (const std::exception &) {
    }
  }

  return 0;
}