#pragma once

#include <cstring>
#include <cstddef>
#include <algorithm>
#include <iostream>
#include <optional>
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>

//date
bool is_leap(int y) { 
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

int days_in_month(int y, int m) {
    int dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    return (m == 2 && is_leap(y)) ? 29 : dm[m - 1];
}

//days since 1970.01.01.
constexpr int32_t days_from_date(int y, int m, int d) {
    y -= (m <= 2);
    int era = ((y >= 0 ? y : y - 399) / 400);
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153u * static_cast<unsigned>(m > 2 ? m - 3 : m + 9) + 2) / 5 + static_cast<unsigned>(d) - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

void date_from_days(int32_t z, int& y, int& m, int& d) {
    z += 719468;
    const int era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int>(yoe) + era * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y += (m <= 2);
}

enum class DayCount : uint8_t { ACT_365F, ACT_ACT_ISDA };

struct Date {
    int32_t n = INT32_MIN;  //days since 1970-01-01; INT32_MIN == "no date"

    Date() = default;

    explicit Date(int32_t days) : n(days) {}

    static Date from_ymd(int y, int m, int d) { return Date(days_from_date(y, m, d)); }
    static Date max() { return Date(INT32_MAX); }

    bool valid() const { return n != INT32_MIN; }
    void ymd(int& y, int& m, int& d) const { date_from_days(n, y, m, d); }
    int year() const { int y, m, d; ymd(y, m, d); return y; }

    friend bool operator==(Date a, Date b) { return a.n == b.n; }
    friend bool operator!=(Date a, Date b) { return a.n != b.n; }
    friend bool operator<(Date a, Date b) { return a.n < b.n; }
    friend bool operator<=(Date a, Date b) { return a.n <= b.n; }
    friend bool operator>(Date a, Date b) { return a.n > b.n; }
    friend bool operator>=(Date a, Date b) { return a.n >= b.n; }
    friend int32_t operator-(Date a, Date b) { return a.n - b.n; }
    Date operator+(int32_t days) const { return Date(n + days); }
};

constexpr Date add_months(Date base, int months, int day = 0) {
    int y, m, d;
    base.ymd(y, m, d);
    const int total = y * 12 + (m - 1) + months;
    const int ny = total / 12;  // years are positive in practice
    const int nm = total - ny * 12 + 1;
    const int dim = days_in_month(ny, nm);
    int nd = day == 0 ? d : (day < 0 ? dim : day);
    if (nd > dim) nd = dim;
    return Date::from_ymd(ny, nm, nd);
}

// Accepts "DD.MM.YYYY", "YYYY-MM-DD"
inline std::optional<Date> parse_date(std::string_view s) {
    auto num = [&](size_t pos, size_t len, int& out) {
        if (pos + len > s.size()) return false;
        int v = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = s[pos + i];
            if (c < '0' || c > '9') return false;
            v = v * 10 + (c - '0');
        }
        out = v;
        return true;
        };
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    int y = 0, m = 0, d = 0;
    if (s.size() >= 10 && s[4] == '-' && s[7] == '-' && num(0, 4, y) && num(5, 2, m) && num(8, 2, d)) {
    }
    else if (s.size() >= 10 && s[2] == '.' && s[5] == '.' && num(0, 2, d) && num(3, 2, m) && num(6, 4, y)) {
    }
    else {
        return std::nullopt;
    }
    if (m < 1 || m > 12 || d < 1 || d > days_in_month(y, m)) return std::nullopt;
    return Date::from_ymd(y, m, d);
}


enum class Discriminators {
    ANN,
    RGM,
    SIMPLE,
    None
};

enum class RateType {
    FIXED,
    FLOAT,
    None
};

enum class DayConvention {
    ACTACT,
    ACTACTISDA,
    ACT365,
    None
};

enum class PaymentRulePeriod {
    ONEMonth,
    ENDDATE,
    None
};


struct Schedules {
    std::vector<Date> regular_future;
    std::vector<Date> interest_future;
};

class Deal {
public:
    int64_t deal_id = 0;
    Date load_dtm;
    Date maturity_dttm;
    Date deal_dttm;
    Discriminators discriminator = Discriminators::None;
    RateType rate_type = RateType::None;
    long double interest_rate = 0;
    PaymentRulePeriod base_payment_rule_period = PaymentRulePeriod::None;
    PaymentRulePeriod interest_payment_rule_period = PaymentRulePeriod::None;
    DayConvention day_convention;
    Date interest_payment_rule_start_dttm;
    Date base_payment_rule_start_dttm;
    long long current_principal_amt;
    Schedules time_table;

    Deal(
    int64_t id,
    Date load_d,
    Date maturity_d,
    Date deal_d,
    Discriminators disc,
    RateType rate_t,
    long double in_rate,
    PaymentRulePeriod base_payment_rule,
    PaymentRulePeriod interest_payment_rule,
    DayConvention day_conv,
    Date interest_payment_rule_start_d,
    Date base_payment_rule_start_d,
    long long current_principal_am
    ): deal_id(id),
        load_dtm(load_d),
        maturity_dttm(maturity_d),
        deal_dttm(deal_d),
        discriminator(disc),
        rate_type(rate_t),
        interest_rate(in_rate),
        base_payment_rule_period(base_payment_rule),
        interest_payment_rule_period(interest_payment_rule),
        day_convention(day_conv),
        interest_payment_rule_start_dttm(interest_payment_rule_start_d),
        base_payment_rule_start_dttm(base_payment_rule_start_d),
        current_principal_amt(current_principal_am)
    {
       
        Date Startdate = (load_dtm > deal_dttm ? load_dtm : deal_dttm);
        bool new_deal = Startdate==deal_dttm;
        Date interestStart = Startdate;
        
        if (!new_deal) {
            int sy, sm, sd;
            interest_payment_rule_start_dttm.ymd(sy, sm, sd);
            interestStart = add_months(Startdate, -1, sd);
        }

        while (interestStart < maturity_dttm) {
            time_table.interest_future.push_back(interestStart);
            interestStart = add_months(interestStart, 1);
        }
        while (interestStart < maturity_dttm) {
            time_table.regular_future.push_back(Startdate);
            Startdate = add_months(Startdate, 1);
        }
    }

};

