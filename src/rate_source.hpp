#pragma once
#include "rates.hpp"

class RateSource {
public:
    virtual ~RateSource() = default;
    virtual Rates load() = 0;
};