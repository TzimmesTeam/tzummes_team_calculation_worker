#include "json.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

namespace {

struct Parser {
  const std::string &s;
  size_t pos = 0;

  void skip() {
    while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos])))
      ++pos;
  }
  char peek() {
    skip();
    if (pos >= s.size())
      throw std::runtime_error("");
    return s[pos];
  }
  void expect(char c) {
    if (peek() != c)
      throw std::runtime_error("");
    ++pos;
  }
  std::string str() {
    expect('"');
    std::string out;
    while (pos < s.size() && s[pos] != '"') {
      if (s[pos] == '\\' && pos + 1 < s.size())
        ++pos;
      out += s[pos++];
    }
    expect('"');
    return out;
  }
  Json value() {
    Json j;
    char c = peek();
    if (c == '{') {
      j.kind = Json::Object;
      ++pos;
      if (peek() == '}') {
        ++pos;
        return j;
      }
      while (true) {
        std::string key = str();
        expect(':');
        j.fields[key] = value();
        if (peek() == ',') {
          ++pos;
          continue;
        }
        expect('}');
        return j;
      }
    }
    if (c == '[') {
      j.kind = Json::Array;
      ++pos;
      if (peek() == ']') {
        ++pos;
        return j;
      }
      while (true) {
        j.items.push_back(value());
        if (peek() == ',') {
          ++pos;
          continue;
        }
        expect(']');
        return j;
      }
    }
    if (c == '"') {
      j.kind = Json::String;
      j.text = str();
      return j;
    }
    if (s.compare(pos, 4, "null") == 0) {
      pos += 4;
      return j;
    }
    char *end = nullptr;
    j.number = std::strtod(s.c_str() + pos, &end);
    if (end == s.c_str() + pos)
      throw std::runtime_error("");
    j.kind = Json::Number;
    pos = static_cast<size_t>(end - s.c_str());
    return j;
  }
};

} // namespace

Json parse_json(const std::string &text) {
  Parser p{text};
  return p.value();
}