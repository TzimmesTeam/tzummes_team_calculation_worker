#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace calculation {

    using ll = long long;
    bool is_leap(int y);
    int  days_in_month(int y, int m);
    void date_from_days(int32_t z, int& y, int& m, int& d);
    inline constexpr int32_t days_from_date(int y, int m, int d) {
        y -= (m <= 2);
        int era = ((y >= 0 ? y : y - 399) / 400);
        unsigned yoe = static_cast<unsigned>(y - era * 400);
        unsigned doy = (153u * static_cast<unsigned>(m > 2 ? m - 3 : m + 9) + 2) / 5 + static_cast<unsigned>(d) - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + static_cast<int>(doe) - 719468;
    }

    struct Date {
        int32_t n = INT32_MIN;  // days since 1970-01-01; INT32_MIN == "no date"

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

    Date add_months(Date base, int months, int day = 0);
    int  month_diff(Date a, Date b);
    std::optional<Date> parse_date(std::string_view s);

    enum class Discriminators { ANN, RGM, SIMPLE, None };
    enum class RateType { FIXED, FLOAT, None };
    enum class DayConvention { ACTACT, ACTACTISDA, ACT365, None };
    enum class PaymentRulePeriod { ONEMonth, ENDDATE, None };
    enum class PaymentType { INTERESET, BASE, CLOSE, None };

    struct Schedules {
        std::vector<Date> regular_future;
        std::vector<Date> interest_future;
    };

    class CashPart {
    public:
        Date date;
        PaymentType payment_type;
        long double amount;
        long double balance_after;
        ll days_from_start;
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
        DayConvention day_convention = DayConvention::None;
        Date interest_payment_rule_start_dttm;
        Date base_payment_rule_start_dttm;
        long double current_principal_amt = 0;
        Schedules time_table;
        std::vector<CashPart> cash_flow;
        ll month_count = 0;

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
            long double current_principal_am
        );

        void calculate();

    private:
        int daysInYear(ll year) const;
        long double accrue(long double balance, Date from, Date to) const;

        void calcANN();
        void calcRGM();
        void calcSIMPLE();
    };

}