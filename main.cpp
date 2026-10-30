#include "algorithms.hpp"

#include <cstdio>
#include <string>
#include <iostream>

using namespace calculation;

std::string type_name(PaymentType t) {
    switch (t) {
    case PaymentType::INTEREST: 
        return "interest";
    case PaymentType::BASE:   
        return "principal";
    case PaymentType::CLOSE:
        return "close";
    default:                  
        return "none";
    }
}

int main() {

    Deal deal(
        3,        
        *parse_date("31.08.2022"),            
        *parse_date("16.07.2030"),       
        *parse_date("17.12.2020"),      
        Discriminators::ANN,   
        RateType::FIXED, 
        0.074264L * 100.0,
        PaymentRulePeriod::ONEMonth,
        PaymentRulePeriod::ONEMonth,
        DayConvention::ACTACT,
        Date(),
        *parse_date("17.12.2020"),   
        4800112.48
    );

    deal.calculate();

    for (auto& cp : deal.cash_flow) {
        int y, m, d;
        cp.date.ymd(y, m, d);

        std::cout << d << "." << m << "." << y << " " << type_name(cp.payment_type) << " " << cp.amount << " " << cp.balance_after << " " << cp.days_from_start << "\n";

    }

    return 0;
}