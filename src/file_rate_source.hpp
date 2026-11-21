#pragma once
#include <filesystem>
#include <utility>
#include "rate_source.hpp"

class JsonFileRateSource : public RateSource {
public:
    explicit JsonFileRateSource(std::filesystem::path file) : file_(std::move(file)) {}
    Rates load() override;

private:
    std::filesystem::path file_;
};