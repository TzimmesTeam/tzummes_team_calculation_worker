#pragma once
#include "rates.hpp"
#include <optional>
#include <string>
#include <vector>

struct Period {
  int months = 0;
  int days = 0;
  bool end = false;
  bool valid() const { return months != 0 || days != 0; }
};
Period period_from_code(long long code);

struct Rule {
  std::optional<Day> start;
  Period period;
  int day = 0;
  bool usable() const { return start && period.valid(); }
};

struct SubsidyIn {
  bool present = false;
  int rate_type = 0; // 1 FIXED, 2 FLOAT
  double rate = 0;
  std::string indicator;
  double coeff = 1;
  Rule pay, rep;
  std::optional<double> cap, floor;
  Period lag;
};

struct DealIn {
  long long id = 0;
  Day report, maturity;
  std::optional<Day> deal_date;
  int disc = 0;      // 1 ANN, 2 RGM, 3 SIMPLE
  int rate_type = 0; // 1 FIXED, 2 FLOAT
  double rate = 0;
  std::optional<double> base_rate;
  std::string base_indicator;
  Rule int_rule, reg_rule, rep_rule;
  int day_conv = 0; // 1 ACT/ACT, 2 ACT/ACT ISDA, 3 ACT/365
  std::optional<double> accrued;
  double balance = 0;
  std::vector<Day> int_dates;
  std::vector<Day> prin_dates;
  std::vector<std::optional<double>> prin_amounts;
  std::vector<Day> rep_dates;
  std::vector<Day> rs_dates;
  std::vector<double> rs_rates;
  std::vector<Day> rf_dates;
  std::vector<double> rf_rates;
  std::optional<double> cap, floor;
  Period lag;
  SubsidyIn sub;
  std::string behavior; // behavior_model_cd
};

struct Flow {
  Day date;
  std::string type; // interest, principal, close, repricing, subsidy
  double amount = 0;
  double balance_after = 0;
  long long days = 0; // дней от начала периода
};

std::vector<Flow> calculate(const DealIn &deal, const Rates &rates);
