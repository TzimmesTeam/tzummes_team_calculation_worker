#include "algorithms.hpp"

#include <algorithm>
#include <cmath>

namespace calculation {

    bool is_leap(int y) {
        return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    }

    int days_in_month(int y, int m) {
        static const int dm[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        return (m == 2 && is_leap(y)) ? 29 : dm[m - 1];
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

    Date add_months(Date base, int months, int day) {
        int y, m, d;
        base.ymd(y, m, d);
        const int total = y * 12 + (m - 1) + months;
        const int ny = total / 12;
        const int nm = total - ny * 12 + 1;
        const int dim = days_in_month(ny, nm);
        int nd = day == 0 ? d : (day < 0 ? dim : day);
        if (nd > dim) nd = dim;
        return Date::from_ymd(ny, nm, nd);
    }

    int month_diff(Date a, Date b) {
        int ya, ma, da, yb, mb, db;
        a.ymd(ya, ma, da);
        b.ymd(yb, mb, db);
        return (yb - ya) * 12 + (mb - ma);
    }

    std::optional<Date> parse_date(std::string_view s) {
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
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) 
            s.remove_prefix(1);
        int y = 0, m = 0, d = 0;
        if (s.size() >= 10 && s[4] == '-' && s[7] == '-' && num(0, 4, y) && num(5, 2, m) && num(8, 2, d)) {
        }
        else if (s.size() >= 10 && s[2] == '.' && s[5] == '.' && num(0, 2, d) && num(3, 2, m) && num(6, 4, y)) {
        }
        else {
            return std::nullopt;
        }
        if (m < 1 || m > 12 || d < 1 || d > days_in_month(y, m))
            return std::nullopt;
        return Date::from_ymd(y, m, d);
    }

    Deal::Deal(
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
        long double current_principal_am
    ) : deal_id(id),
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
        const Date Startdate = (load_dtm > deal_dttm ? load_dtm : deal_dttm);
        const bool new_deal = (Startdate == deal_dttm);

        month_count = month_diff(Startdate, maturity_dttm);

        int y, m, iDay = 0, bDay = 0;
        if (interest_payment_rule_start_dttm.valid()) interest_payment_rule_start_dttm.ymd(y, m, iDay);
        if (base_payment_rule_start_dttm.valid())     base_payment_rule_start_dttm.ymd(y, m, bDay);
        if (!iDay) 
            iDay = bDay;
        if (!bDay) 
            bDay = iDay;

        auto anchor = [&](int day) {
            if (new_deal) return Startdate;
            Date a = add_months(Startdate, 0, day);
            return a > Startdate ? add_months(Startdate, -1, day) : a;
         };

        const Date iAnchor = anchor(iDay);
        const Date bAnchor = anchor(bDay);
        const int iD = new_deal ? 0 : iDay;
        const int bD = new_deal ? 0 : bDay;

        for (int k = 0; ; ++k) {
            Date d = add_months(iAnchor, k, iD);
            if (d >= maturity_dttm) break;
            time_table.interest_future.push_back(d);
        }
        for (int k = 0; ; ++k) {
            Date d = add_months(bAnchor, k, bD);
            if (d >= maturity_dttm) break;
            time_table.regular_future.push_back(d);
        }
    }

    void Deal::calculate() {
        switch (discriminator) {
            case Discriminators::ANN:
                calcANN();
                break;
            case Discriminators::RGM:
                calcRGM();
                break;
            case Discriminators::SIMPLE:
                calcSIMPLE();
                break;
            default:
                break;
        }
    }

    int Deal::daysInYear(ll year) const {
        if (day_convention == DayConvention::ACT365)
            return 365;
        else if (day_convention == DayConvention::ACTACT)
            return (is_leap(static_cast<int>(year)) ? 366 : 365);
        else if (day_convention == DayConvention::ACTACTISDA)
            return (is_leap(static_cast<int>(year)) ? 366 : 365);
        else
            return 1;
    }

    long double Deal::accrue(long double balance, Date from, Date to) const {
        return balance * (interest_rate / 100.0L) * (to - from) / daysInYear(to.year());
    }

    void Deal::calcANN() {
        if (!cash_flow.empty())
            return;

        const size_t n = std::min(time_table.regular_future.size(), time_table.interest_future.size());

        if (n < 2)
            return;

        long double r = interest_rate / 12.0 / 100.0;
        ll periods = static_cast<ll>(n) - 1;
        long double pw = std::pow(1 + r, periods);
        long double pay = current_principal_amt * r * pw / (pw - 1);
        long double balance = current_principal_amt;
        ll days_i = 0, days_r = 0;

        for (size_t i = 1; i < n; ++i) {
            days_i += time_table.interest_future[i] - time_table.interest_future[i - 1];
            days_r += time_table.regular_future[i] - time_table.regular_future[i - 1];

            const long double interest = accrue(balance, time_table.interest_future[i - 1], time_table.interest_future[i]);
            const long double principal = std::min(pay - interest, balance);
            balance -= principal;

            cash_flow.push_back({ time_table.interest_future[i], PaymentType::INTEREST, interest, balance, days_i });
            cash_flow.push_back({ time_table.regular_future[i], PaymentType::BASE, principal, balance, days_r });
        }

        if (balance > 1e-6L) {
            const long double tail_interest = accrue(balance, time_table.interest_future[n - 1], maturity_dttm);
            days_i += maturity_dttm - time_table.interest_future[n - 1];
            days_r += maturity_dttm - time_table.regular_future[n - 1];
            cash_flow.push_back({ maturity_dttm, PaymentType::INTEREST, tail_interest, balance, days_i });
            cash_flow.push_back({ maturity_dttm, PaymentType::CLOSE, balance, 0, days_r });
        }
    }

    void Deal::calcRGM() {
        if (!cash_flow.empty())
            return;
        size_t n = std::min(time_table.regular_future.size(), time_table.interest_future.size());
        if (n < 2)
            return;

        ll periods = static_cast<ll>(n) - 1;
        long double step = current_principal_amt / periods;
        long double balance = current_principal_amt;
        ll days_i = 0, days_r = 0;

        for (size_t i = 1; i < n; ++i) {
            ll di = time_table.interest_future[i] - time_table.interest_future[i - 1];
            ll dr = time_table.regular_future[i] - time_table.regular_future[i - 1];
            days_i += di;
            days_r += dr;

            long double interest = accrue(balance, time_table.interest_future[i - 1], time_table.interest_future[i]);
            bool last = (i + 1 == n);
            long double principal = last ? balance : step;
            balance -= principal;

            cash_flow.push_back({ time_table.interest_future[i], PaymentType::INTEREST, interest, balance, days_i });
            cash_flow.push_back({ time_table.regular_future[i], last ? PaymentType::CLOSE : PaymentType::BASE, principal, balance, days_r });
        }
    }

    void Deal::calcSIMPLE() {
        if (!cash_flow.empty())
            return;
        if (time_table.interest_future.empty())
            return;

        long double balance = current_principal_amt;
        ll days_i = 0;

        for (size_t i = 1; i < time_table.interest_future.size(); ++i) {
            days_i += time_table.interest_future[i] - time_table.interest_future[i - 1];
            cash_flow.push_back({ time_table.interest_future[i], PaymentType::INTEREST,
                                  accrue(balance, time_table.interest_future[i - 1], time_table.interest_future[i]),
                                  balance, days_i });
        }

        Date lastI = time_table.interest_future.back();
        if (lastI < maturity_dttm) {
            days_i += maturity_dttm - lastI;
            cash_flow.push_back({ maturity_dttm, PaymentType::INTEREST, accrue(balance, lastI, maturity_dttm), balance, days_i });
        }
        Date start = time_table.interest_future.front();
        cash_flow.push_back({ maturity_dttm, PaymentType::CLOSE, balance, 0, maturity_dttm - start });
    }

}