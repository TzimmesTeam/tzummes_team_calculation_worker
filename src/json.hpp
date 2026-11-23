#pragma once
#include <map>
#include <string>
#include <vector>

struct Json {
  enum Kind { Null, Number, String, Array, Object } kind = Null;
  double number = 0;
  std::string text;
  std::vector<Json> items;
  std::map<std::string, Json> fields;
};

Json parse_json(const std::string &text);