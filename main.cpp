#include "algorithms.hpp"

#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace calculation;

static const size_t FIELDS_BASE = 13;
static const size_t FIELDS_FULL = 18;

static std::string type_name(PaymentType t) {
    switch (t) {
    case PaymentType::INTEREST: return "interest";
    case PaymentType::BASE:     return "principal";
    case PaymentType::CLOSE:    return "close";
    case PaymentType::REPRICING: return "repricing";
    default:                    return "none";
    }
}

static std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    while (true) {
        size_t p = s.find(sep, start);
        if (p == std::string::npos) { out.push_back(trim(s.substr(start))); break; }
        out.push_back(trim(s.substr(start, p - start)));
        start = p + 1;
    }
    return out;
}

static Date req_date(const std::string& s, const char* name) {
    auto d = parse_date(s);
    if (!d) throw std::runtime_error(std::string("bad date in ") + name + ": '" + s + "'");
    return *d;
}

static Date opt_date(const std::string& s, const char* name) {
    return s.empty() ? Date() : req_date(s, name);
}

static Discriminators parse_disc(const std::string& s) {
    if (s == "ANN")    return Discriminators::ANN;
    if (s == "RGM")    return Discriminators::RGM;
    if (s == "SIMPLE") return Discriminators::SIMPLE;
    throw std::runtime_error("bad discriminator: '" + s + "'");
}

static RateType parse_rate_type(const std::string& s) {
    if (s == "FIXED") return RateType::FIXED;
    if (s == "FLOAT") return RateType::FLOAT;
    throw std::runtime_error("bad rate_type: '" + s + "'");
}

static PaymentRulePeriod parse_period(const std::string& s) {
    if (s == "1M" || s == "ONEMonth") return PaymentRulePeriod::ONEMonth;
    if (s == "ENDDATE")               return PaymentRulePeriod::ENDDATE;
    return PaymentRulePeriod::None;
}

static DayConvention parse_day_conv(const std::string& s) {
    if (s == "ACT/ACT")      return DayConvention::ACTACT;
    if (s == "ACT/ACT ISDA") return DayConvention::ACTACTISDA;
    if (s == "ACT/365")      return DayConvention::ACT365;
    throw std::runtime_error("bad day_convention: '" + s + "'");
}

static long double parse_num(const std::string& s, const char* name) {
    try {
        size_t pos = 0;
        long double v = std::stold(s, &pos);
        if (pos != s.size()) throw std::invalid_argument("tail");
        return v;
    }
    catch (...) {
        throw std::runtime_error(std::string("bad number in ") + name + ": '" + s + "'");
    }
}

static std::optional<long double> opt_num(const std::string& s, const char* name) {
    if (s.empty()) return std::nullopt;
    return parse_num(s, name);
}

static Deal make_deal(std::vector<std::string> f) {
    if (f.size() != FIELDS_BASE && f.size() != FIELDS_FULL)
        throw std::runtime_error("expected " + std::to_string(FIELDS_BASE) + " or " + std::to_string(FIELDS_FULL) +
            " fields, got " + std::to_string(f.size()));
    f.resize(FIELDS_FULL);

    RepricingRule rep;
    rep.start = opt_date(f[13], "repricing_rule_start_dttm");
    auto period = parse_period_spec(f[14]);
    if (!period) throw std::runtime_error("bad repricing_rule_period_cd: '" + f[14] + "'");
    rep.period = *period;
    rep.day = f[15].empty() ? 0 : static_cast<int>(parse_num(f[15], "repricing_rule_day_num"));

    return Deal(
        static_cast<int64_t>(parse_num(f[0], "deal_id")),
        req_date(f[1], "load_dtm"),
        req_date(f[2], "maturity_dttm"),
        req_date(f[3], "deal_dttm"),
        parse_disc(f[4]),
        parse_rate_type(f[5]),
        parse_num(f[6], "interest_rate"),
        parse_period(f[7]),
        parse_period(f[8]),
        parse_day_conv(f[9]),
        opt_date(f[10], "interest_payment_rule_start_dttm"),
        opt_date(f[11], "base_payment_rule_start_dttm"),
        parse_num(f[12], "current_principal_amt"),
        rep,
        opt_num(f[16], "cap_limit_interest_rate"),
        opt_num(f[17], "floor_limit_interest_rate")
    );
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cout << std::fixed << std::setprecision(2);

    std::string line;
    size_t line_no = 0;
    while (std::getline(std::cin, line)) {
        ++line_no;
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        try {
            Deal deal = make_deal(split(line, ';'));
            deal.calculate();

            for (const auto& cp : deal.cash_flow) {
                int y, m, d;
                cp.date.ymd(y, m, d);
                std::cout << deal.deal_id << " "
                    << d << "." << m << "." << y << " "
                    << type_name(cp.payment_type) << " "
                    << cp.amount << " " << cp.balance_after << " "
                    << cp.days_from_start << "\n";
            }
            std::cout << std::flush;
        }
        catch (const std::exception& e) {
            std::cerr << "line " << line_no << ": " << e.what() << std::endl;
        }
    }
    return 0;
}