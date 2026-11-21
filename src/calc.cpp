#include "calc.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace std::chrono;

namespace {

    const double kEps = 1e-6;

    //высокосный год или нет
    bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

    std::string iso(Day d) {
        year_month_day ymd{ d };
        char buf[16];
        std::snprintf(buf, sizeof buf, "%02u.%02u.%04d", unsigned(ymd.day()), int(ymd.year()), unsigned(ymd.month()));
        return buf;
    }

    Day add_months(Day d, int months, int dayOfMonth = 0) {
        year_month_day ymd{ d };
        int total = int(ymd.year()) * 12 + int(unsigned(ymd.month())) - 1 + months;
        year y{ total / 12 };
        month m{ unsigned(total % 12 + 1) };
        unsigned last = unsigned(year_month_day_last{ y, month_day_last{ m } }.day());
        unsigned wanted = dayOfMonth == 0 ? unsigned(ymd.day()) : (dayOfMonth < 0 ? last : unsigned(dayOfMonth));
        return Day{ year_month_day{ y, m, day{ std::min(wanted, last) } } };
    }

    Day shift_day(Day d, const Period& p) {
        if (p.months != 0)
            d = add_months(d, p.months);
        return d + days{ p.days };
    }

    // k дата серии правила
    Day nth_date(Day start, const Period& p, int dayOfMonth, int k) {
        return p.months != 0 ? add_months(start, k * p.months, dayOfMonth) : start + days{ k * p.days };
    }

    // Все даты правила не позже mat, начиная со старта
    std::vector<Day> series(const Rule& r, Day mat) {
        std::vector<Day> out;
        for (int k = 0; k < 100000; ++k) {
            Day d = nth_date(*r.start, r.period, r.day, k);
            if (d > mat)
                break;
            if (d >= *r.start)
                out.push_back(d);
        }
        return out;
    }

    std::vector<Day> sorted_unique(std::vector<Day> v) {
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
        return v;
    }

    std::vector<Day> close_at(std::vector<Day> v, Day mat) {
        v.erase(std::remove_if(v.begin(), v.end(), [&](Day d) { return d > mat; }), v.end());
        if (v.empty() || v.back() < mat)
            v.push_back(mat);
        return v;
    }

    //последняя дата <= d в отсортированном списке
    std::optional<Day> last_on_or_before(const std::vector<Day>& v, Day d) {
        std::optional<Day> found;
        for (Day x : v) {
            if (x > d) break;
            found = x;
        }
        return found;
    }

    // Ступенчатая ставка: с даты from действует rate
    struct Step {
        Day from;
        double rate;
    };

    double rate_on(const std::vector<Step>& steps, Day d) {
        double r = steps.front().rate;
        for (const Step& s : steps) {
            if (s.from > d)
                break;
            r = s.rate;
        }
        return r;
    }

    double year_length(int dayConv, Day d) {
        if (dayConv == 3)
            return 365.0;
        return is_leap(int(year_month_day{ d }.year())) ? 366.0 : 365.0;
    }

    double accrue(const std::vector<Step>& steps, int dayConv, double balance, Day from, Day to) {
        double total = 0;
        Day x = from;
        while (x < to) {
            Day end = to;
            for (const Step& s : steps)
                if (s.from > x) {
                    end = std::min(end, s.from);
                    break;
                }
            total += balance * rate_on(steps, x) * double((end - x).count()) / year_length(dayConv, end);
            x = end;
        }
        return total;
    }

    double annuity_payment(double balance, double yearRate, double periodYears, size_t n) {
        if (n == 0)
            return balance;
        double r = yearRate * periodYears;
        if (std::fabs(r) < 1e-15)
            return balance / double(n);
        double pw = std::pow(1.0 + r, double(n));
        return balance * r * pw / (pw - 1.0);
    }

    //ЗАГЛУШКА
    double monthly_prepayment_rate(const std::string& model) {
        if (model == "PREPAYMENT_CONSUMER")
            return 0.010;
        if (model == "PREPAYMENT_AUTO")
            return 0.010;
        if (model == "PREPAYMENT_MORTGAGE")
            return 0.005;
        if (model == "PREPAYMENT_MR8_CRED")
            return 0.005;
        return 0.0;
    }

    double period_years(const Period& p) {
        if (p.months != 0) return p.months / 12.0;
        if (p.days != 0) return p.days / 365.0;
        return 1.0 / 12.0;
    }

}

Period period_from_code(long long code) {
    Period p;
    if (code < 0) {
        p = period_from_code(-code);
        p.months = -p.months;
        p.days = -p.days;
        p.end = false;
        return p;
    }
    if (code == 0) p.end = true;
    else if (code < 1000000) p.months = int(code);
    else p.days = int(code - 1000000);
    return p;
}

std::vector<Flow> calculate(const DealIn& d, const Rates& rates) {

    const Day mat = d.maturity;
    if (mat <= d.report)
        throw std::runtime_error("");
    if (d.disc < 1 || d.disc > 3)
        throw std::runtime_error("");
    if (d.rate_type < 1 || d.rate_type > 2)
        throw std::runtime_error("");
    if (d.day_conv < 1 || d.day_conv > 3)
        throw std::runtime_error("");
    if (d.rate_type == 2 && d.base_indicator.empty())
        throw std::runtime_error("");
    if (d.balance == 0) return {};

    const bool ann = d.disc == 1;
    const bool simple = d.disc == 3;
    const bool explicit_principal = simple && !d.prin_dates.empty();
    const double sign = d.balance < 0 ? -1.0 : 1.0;
    const double start_balance = std::fabs(d.balance);

    //дата, с которой сделка учитывается
    const Day S = (d.deal_date && *d.deal_date > d.report) ? *d.deal_date : d.report;

    //расписания
    std::vector<Day> reg;
    if (d.reg_rule.period.end)
        reg = { mat };
    else if (d.reg_rule.usable())
        reg = series(d.reg_rule, mat);
    else if (simple)
        reg = { mat };
    else
        throw std::runtime_error("");
    if (reg.empty())
        reg = { mat };

    std::vector<Day> ints;  // проценты
    if (!d.int_dates.empty())
        ints = close_at(sorted_unique(d.int_dates), mat);
    else if (d.int_rule.period.end)
        ints = { mat };
    else if (d.int_rule.usable())
        ints = close_at(series(d.int_rule, mat), mat);
    else if (simple)
        ints = { mat };
    else
        ints = close_at(reg, mat);

    std::vector<Day> rep;
    if (d.rate_type == 2) {
        if (!d.rep_dates.empty())
            rep = close_at(sorted_unique(d.rep_dates), mat);
        else if (d.rep_rule.usable())
            rep = close_at(series(d.rep_rule, mat), mat);
        else
            rep = ints;
    }

    std::vector<Day> sched;
    for (Day x : reg)
        if (x > S) sched.push_back(x);
    const size_t n_sched = std::max<size_t>(1, sched.size());

    const Day anchorI = d.accrued ? d.report : last_on_or_before(ints, S).value_or(S);
    const Day anchorR = last_on_or_before(reg, S).value_or(S);
    const Day t0 = std::min(anchorI, anchorR);

    std::vector<Day> changes = rep;
    changes.insert(changes.end(), d.rs_dates.begin(), d.rs_dates.end());
    changes.insert(changes.end(), d.rf_dates.begin(), d.rf_dates.end());
    changes = sorted_unique(changes);

    std::vector<Day> resets = rep;
    resets.push_back(t0);
    resets.push_back(anchorI);
    resets = sorted_unique(resets);

    auto base_rate = [&](Day t) -> double {
        if (d.rate_type == 1) {
            double r = d.rate;
            for (size_t i = 0; i < d.rf_dates.size(); ++i)
                if (d.rf_dates[i] <= t) r = d.rf_rates[i];
            return r;
        }
        bool has_override = false;
        double override_rate = 0;
        for (size_t i = 0; i < d.rs_dates.size(); ++i)
            if (d.rs_dates[i] <= t) {
                override_rate = d.rs_rates[i];
                has_override = true;
            }
        if (has_override)
            return override_rate;

        const Day reset = last_on_or_before(resets, t).value_or(t0);
        const Day fixing = shift_day(reset, d.lag);
        double index;
        if (auto v = rates.value(d.base_indicator, fixing))
            index = *v;
        else if (d.base_rate)
            index = *d.base_rate;
        else
            throw std::runtime_error("");

        double r = d.rate + index;
        if (d.cap && r > *d.cap) r = *d.cap;
        if (d.floor && r < *d.floor) r = *d.floor;
        return r;
        };

    std::vector<Step> base_steps{ { t0, base_rate(t0) } };
    std::vector<Day> chg;
    for (Day x : changes)
        if (x > t0 && x < mat) {
            chg.push_back(x);
            base_steps.push_back({ x, base_rate(x) });
        }

    const SubsidyIn& s = d.sub;
    std::vector<Day> sub_pay, sub_rep, sub_resets;
    Day anchorS = S;
    std::vector<Step> sub_steps;
    if (s.present) {
        if (s.pay.period.end)
            sub_pay = { mat };
        else if (s.pay.usable())
            sub_pay = close_at(series(s.pay, mat), mat);
        else
            throw std::runtime_error("");

        if (s.pay.period.end)
            anchorS = S;
        else
            anchorS = last_on_or_before(sub_pay, S).value_or(sub_pay.front());

        if (s.rate_type == 2)
            sub_rep = s.rep.usable() ? close_at(series(s.rep, mat), mat) : sub_pay;
        sub_resets = sub_rep;
        sub_resets.push_back(anchorS);
        sub_resets = sorted_unique(sub_resets);
    }

    auto sub_rate = [&](Day t) -> double {
        if (s.rate_type == 1)
            return s.rate;
        const Day reset = last_on_or_before(sub_resets, t).value_or(anchorS);
        const Day fixing = shift_day(reset, s.lag);
        auto v = rates.value(s.indicator, fixing);
        if (!v)
            throw std::runtime_error("");
        double r = s.rate + s.coeff * *v;
        if (s.cap && r > *s.cap)
            r = *s.cap;
        if (s.floor && r < *s.floor)
            r = *s.floor;
        return r;
        };

    if (s.present) {
        sub_steps.push_back({ anchorS, sub_rate(anchorS) });
        for (Day x : sub_rep)
            if (x > anchorS && x < mat) sub_steps.push_back({ x, sub_rate(x) });
    }

    std::vector<Day> sub_chg;
    if (s.present && s.rate_type == 2 && s.rep.usable()) {
        for (Day x : sub_rep)
            if (x > S && x < mat) sub_chg.push_back(x);
    }

    std::vector<Day> prin;//даты платежей долга
    std::vector<std::optional<double>> prin_sum;
    if (explicit_principal) {
        if (d.prin_amounts.size() != d.prin_dates.size() || d.prin_amounts.empty())
            throw std::runtime_error("");
        for (size_t i = 0; i < d.prin_dates.size(); ++i) {
            if (d.prin_dates[i] <= S || d.prin_dates[i] > mat)
                continue;
            prin.push_back(d.prin_dates[i]);
            prin_sum.push_back(d.prin_amounts[i]);
        }
        if (prin.empty() || prin.back() < mat) {
            prin.push_back(mat);
            prin_sum.push_back(std::nullopt);
        }
    }
    else {
        prin = sched;
        if (prin.empty() || prin.back() < mat)
            prin.push_back(mat);
        prin_sum.assign(prin.size(), std::nullopt);
    }

    const double period_yrs = d.reg_rule.usable() ? period_years(d.reg_rule.period) : 1.0 / 12.0;
    double equal_part = start_balance / double(n_sched);

    const double months_per_period = d.int_rule.period.months > 0 ? d.int_rule.period.months : (d.int_rule.period.days > 0 ? d.int_rule.period.days / 30.4 : 1.0);
    const double smm_month = monthly_prepayment_rate(d.behavior);
    const double prepay_rate = 1.0 - std::pow(1.0 - smm_month, months_per_period);

    // kind: 0 проценты, 1 тело, 2 репрайсинг основного долга, 3 выплата субсидии, 4 репрайсинг субсидии
    struct Event {
        Day date;
        int kind;
        size_t index;
    };

    std::vector<Event> events;
    for (size_t i = 0; i < ints.size(); ++i)
        if (ints[i] > S)
            events.push_back({ ints[i], 0, i });
    for (size_t j = 0; j < prin.size(); ++j)
        events.push_back({ prin[j], 1, j });
    for (size_t k = 0; k < chg.size(); ++k)
        if (chg[k] > S) events.push_back({ chg[k], 2, k });
    if (s.present)
        for (size_t i = 0; i < sub_pay.size(); ++i)
            if (sub_pay[i] > anchorS) events.push_back({ sub_pay[i], 3, i });
    for (size_t k = 0; k < sub_chg.size(); ++k)
        events.push_back({ sub_chg[k], 4, k });
    std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) {
        return a.date != b.date ? a.date < b.date : a.kind < b.kind;
        });

    double balance = start_balance;
    double accInterest = d.accrued.value_or(0.0);
    double accPrincipal = 0;
    double accSubsidy = 0;
    Day cursor = t0;

    auto advance = [&](Day t) {
        if (t <= cursor) return;
        const Day fromI = std::max(cursor, anchorI);
        if (t > fromI)
            accInterest += accrue(base_steps, d.day_conv, balance, fromI, t);
        const Day fromR = std::max(cursor, anchorR);
        if (t > fromR)
            accPrincipal += accrue(base_steps, d.day_conv, balance, fromR, t);
        if (s.present) {
            const Day fromS = std::max(cursor, anchorS);
            if (t > fromS) accSubsidy += accrue(sub_steps, d.day_conv, balance, fromS, t);
        }
        cursor = t;
        };

    double annuity = ann ? annuity_payment(start_balance, base_rate(S), period_yrs, n_sched) : 0;
    std::vector<Flow> flows;
    size_t i = 0;
    while (i < events.size()) {
        const Day t = events[i].date;
        const size_t firstRow = flows.size();
        for (; i < events.size() && events[i].date == t; ++i) {
            const Event& e = events[i];
            advance(t);

            if (e.kind == 0) {
                const double paid = accInterest;
                accInterest = 0;
                if (t == mat && std::fabs(paid) < 1e-9) continue;
                flows.push_back({ t, "interest", sign * paid, 0, (t - anchorI).count() });

                if (prepay_rate > 0 && t < mat && balance > kEps) {
                    const double prepaid = balance * prepay_rate;
                    balance -= prepaid;
                    flows.push_back({ t, "prepayment", sign * prepaid, 0, (t - anchorI).count() });
                    size_t left = 0;
                    for (Day p : sched)
                        if (p >= t) ++left;
                    if (left > 0 && ann)
                        annuity = annuity_payment(balance, base_rate(t), period_yrs, left);
                    if (left > 0 && !ann && !explicit_principal)
                        equal_part = balance / double(left);
                }
            }
            else if (e.kind == 1) {
                std::string type = "principal";
                double pay;
                const bool last_scheduled = e.index + 1 >= n_sched;
                if (t == mat) {
                    pay = balance;
                    type = "close";
                }
                else if (explicit_principal) {
                    pay = prin_sum[e.index] ? std::min(std::fabs(*prin_sum[e.index]), balance) : balance;
                }
                else if (ann) {
                    pay = std::min(std::max(annuity - accPrincipal, 0.0), balance);
                }
                else {
                    pay = last_scheduled ? balance : std::min(equal_part, balance);
                    if (last_scheduled) type = "close";
                }
                accPrincipal = 0;
                if (pay <= kEps) continue;
                balance -= pay;
                if (std::fabs(balance) < kEps) balance = 0;
                flows.push_back({ t, type, sign * pay, 0, (t - anchorR).count() });
            }
            else if (e.kind == 2) {
                const Day x = chg[e.index];
                const double rate = base_rate(x);
                if (ann) {
                    size_t left = 0;
                    for (Day p : sched)
                        if (p > x) ++left;
                    if (left > 0) annuity = annuity_payment(balance, rate, period_yrs, left);
                }
                flows.push_back({ x, "repricing", rate, 0, (x - anchorI).count() });
            }
            else if (e.kind == 3) {
                const double paid = accSubsidy;
                accSubsidy = 0;
                if (std::fabs(paid) < 1e-9) continue;
                flows.push_back({ t, "subsidy", sign * paid, 0, (t - anchorS).count() });
            }
            else {
                const Day x = sub_chg[e.index];
                flows.push_back({ x, "subsidy_repricing", sub_rate(x), 0, (x - anchorS).count() });
            }
        }
        for (size_t k = firstRow; k < flows.size(); ++k)
            flows[k].balance_after = balance > kEps ? sign * balance : 0.0;
    }
    return flows;
}