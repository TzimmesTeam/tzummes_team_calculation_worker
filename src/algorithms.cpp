#include "algorithms.hpp"
#include "key_rates.hpp"

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

    std::optional<Period> parse_period_spec(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
            s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
            s.remove_suffix(1);
        if (s.empty())
            return Period{};
        size_t i = 0;
        int v = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            v = v * 10 + (s[i] - '0');
            ++i;
        }
        if (i == 0 || i + 1 != s.size() || v <= 0)
            return std::nullopt;
        Period p;
        if (s[i] == 'M' || s[i] == 'm')  
            p.months = v;
        else if (s[i] == 'D' || s[i] == 'd')
            p.days = v;
        else
            return std::nullopt;
        return p;
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
        long double current_principal_am,
        RepricingRule repricing_rule,
        std::optional<long double> cap,
        std::optional<long double> floor
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
        current_principal_amt(current_principal_am),
        repricing(repricing_rule),
        cap_limit(cap),
        floor_limit(floor)
    {
        Date Startdate = (load_dtm > deal_dttm ? load_dtm : deal_dttm);
        bool new_deal = (Startdate == deal_dttm);

        month_count = month_diff(Startdate, maturity_dttm);

        int y, m, iDay = 0, bDay = 0;
        if (interest_payment_rule_start_dttm.valid())
            interest_payment_rule_start_dttm.ymd(y, m, iDay);
        if (base_payment_rule_start_dttm.valid()) 
            base_payment_rule_start_dttm.ymd(y, m, bDay);
        if (!iDay)
            iDay = bDay;
        if (!bDay)
            bDay = iDay;

        auto anchor = [&](int day) {
            if (new_deal)
                return Startdate;
            Date a = add_months(Startdate, 0, day);
            return a > Startdate ? add_months(Startdate, -1, day) : a;
            };

        Date iAnchor = anchor(iDay);
        Date bAnchor = anchor(bDay);
        int iD = new_deal ? 0 : iDay;
        int bD = new_deal ? 0 : bDay;

        if (interest_payment_rule_period == PaymentRulePeriod::ENDDATE) {
            time_table.interest_future.push_back(deal_dttm);
        }
        else {
            for (int k = 0; ; ++k) {
                Date d = add_months(iAnchor, k, iD);
                if (d > maturity_dttm)
                    break;
                time_table.interest_future.push_back(d);
            }
        }
        if (base_payment_rule_period == PaymentRulePeriod::ENDDATE) {
            time_table.regular_future.push_back(deal_dttm);
        }
        else {
            for (int k = 0; ; ++k) {
                Date d = add_months(bAnchor, k, bD);
                if (d > maturity_dttm)
                    break;
                time_table.regular_future.push_back(d);
            }
        }

        if (rate_type == RateType::FLOAT)
            build_rate_timeline(Startdate);
    }

    long double Deal::float_rate(Date d) const {
        long double r = interest_rate + key_rate_percent(d);
        if (cap_limit && r > *cap_limit)     r = *cap_limit;
        if (floor_limit && r < *floor_limit) r = *floor_limit;
        return r;
    }

    void Deal::build_rate_timeline(Date start_date) {
        rate_timeline.clear();
        if (repricing.start.valid() && repricing.period.valid()) {
            for (int k = 0; k < 100000; ++k) {
                Date d = (repricing.period.months > 0 ? add_months(repricing.start, k * repricing.period.months, repricing.day): repricing.start + k * repricing.period.days);
                if (d > maturity_dttm)
                    break;
                rate_timeline.push_back({ d, float_rate(d) });
            }
        }
        if (rate_timeline.empty() || rate_timeline.front().from > start_date)
            rate_timeline.insert(rate_timeline.begin(), { start_date, float_rate(start_date) });
    }

    long double Deal::rate_at(Date d) const {
        if (rate_timeline.empty())
            return interest_rate;
        long double r = rate_timeline.front().rate;
        for (const auto& s : rate_timeline) {
            if (s.from <= d)
                r = s.rate;
            else
                break;
        }
        return r;
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
        long double total = 0;
        Date a = from;
        while (a < to) {
            Date b = to;
            for (const auto& s : rate_timeline) {
                if (s.from > a) { if (s.from < b) b = s.from; break; }
            }
            total += balance * (rate_at(a) / 100.0L) * (b - a) / daysInYear(b.year());
            a = b;
        }
        return total;
    }

    void Deal::calculate() {
        if (!cash_flow.empty() || discriminator == Discriminators::None)
            return;

        if (time_table.interest_future.empty() || time_table.regular_future.empty())
            return;

        Date Startdate = (load_dtm > deal_dttm ? load_dtm : deal_dttm);
        bool simple = (discriminator == Discriminators::SIMPLE);

        std::vector<Date> Iw = time_table.interest_future, Rw = time_table.regular_future;

        bool i_tail = Iw.back() < maturity_dttm;
        if (i_tail) 
            Iw.push_back(maturity_dttm);
        if (Rw.back() < maturity_dttm) 
            Rw.push_back(maturity_dttm);

        ll periods = std::max<ll>(1, static_cast<ll>(time_table.regular_future.size()) - 1);

        long double sign = current_principal_amt < 0 ? -1.0L : 1.0L;
        long double balance = std::fabs(current_principal_amt);
        long double step = balance / periods;

        auto annuity = [&](long double bal, long double rate_pct, ll m) {
            long double r = rate_pct / 12.0L / 100.0L;
            if (m <= 0) return bal;
            if (r == 0) return bal / m;
            long double pw = std::pow(1 + r, m);
            return bal * r * pw / (pw - 1);
            };

        long double pay = annuity(balance, rate_at(Startdate), periods);

        struct Ev { Date d; int kind; size_t idx; };   // kind- 0 проценты, 1 тело, 2 репрайсинг
        std::vector<Ev> ev;


        for (size_t i = 1; i < Iw.size(); ++i)
            ev.push_back({ Iw[i], 0, i });
        if (simple) 
            ev.push_back({ maturity_dttm, 1, Rw.size() - 1 });

        else for (size_t i = 1; i < Rw.size(); ++i) 
            ev.push_back({ Rw[i], 1, i });

        for (size_t k = 0; k < rate_timeline.size(); ++k)
            if (rate_timeline[k].from > Startdate && rate_timeline[k].from < maturity_dttm)
                ev.push_back({ rate_timeline[k].from, 2, k });

        std::stable_sort(ev.begin(), ev.end(), [](const Ev& a, const Ev& b) {
            return a.d != b.d ? a.d < b.d : a.kind < b.kind;
            });

        Date cursor = std::min(Iw[0], Rw[0]);
        long double accI = 0, accB = 0;   // проценты с последней выплаты процентов / с последнего платежа тела

        auto advance = [&](Date t) {
            if (t <= cursor)
                return;
            Date fi = std::max(cursor, Iw[0]);
            if (t > fi) 
                accI += accrue(balance, fi, t);
            Date fb = std::max(cursor, Rw[0]);
            if (t > fb)
                accB += accrue(balance, fb, t);
            cursor = t;
            };

        long double EPS = 1e-6L;
        size_t i = 0;

        while (i < ev.size()) {
            Date d = ev[i].d;
            size_t row0 = cash_flow.size();

            for (; i < ev.size() && ev[i].d == d; ++i) {
                Ev& e = ev[i];
                advance(d);

                if (e.kind == 0) {
                    long double interest = accI;
                    accI = 0;
                    bool tail = i_tail && e.idx == Iw.size() - 1;
                    if (tail && std::fabs(interest) < 1e-9L)
                        continue;
                    cash_flow.push_back({ d, PaymentType::INTEREST, sign * interest, 0, static_cast<ll>(d - Iw[0]) });
                }
                else if (e.kind == 1) {
                    long double principal;
                    PaymentType type = PaymentType::BASE;
                    if (d == maturity_dttm) {
                        principal = balance;
                        type = PaymentType::CLOSE;
                    }
                    else if (discriminator == Discriminators::ANN) {
                        principal = std::min(std::max(pay - accB, 0.0L), balance);
                    }
                    else {   // RGM
                        const bool last_regular = (static_cast<ll>(e.idx) == periods);
                        principal = (last_regular ? balance : std::min(step, balance));
                        if (last_regular)
                            type = PaymentType::CLOSE;
                    }
                    accB = 0;
                    if (type == PaymentType::CLOSE && balance <= EPS)
                        continue;
                    balance -= principal;
                    Date from = simple ? Iw[0] : Rw[0];
                    cash_flow.push_back({ d, type, sign * principal, 0, static_cast<ll>(d - from) });
                }
                else {
                    long double new_rate = rate_timeline[e.idx].rate;
                    if (discriminator == Discriminators::ANN) {
                        ll left = 0; 
                        for (ll k = 1; k <= periods; ++k)
                            if (Rw[static_cast<size_t>(k)] > d) ++left;
                        if (left > 0) 
                            pay = annuity(balance, new_rate, left);
                    }
                    cash_flow.push_back({ d, PaymentType::REPRICING, new_rate, 0, static_cast<ll>(d - Iw[0]) });
                }
            }

            for (size_t k = row0; k < cash_flow.size(); ++k)
                cash_flow[k].balance_after = balance > EPS ? sign * balance : 0.0;
        }
    }

}