#pragma once
#include "rate_source.hpp"
#include <filesystem>
#include <utility>

class JsonFileRateSource : public RateSource {
public:
  explicit JsonFileRateSource(std::filesystem::path file)
      : file_(std::move(file)) {}
  Rates load() override;

private:
  std::filesystem::path file_;
};