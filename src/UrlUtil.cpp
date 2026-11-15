#include "UrlUtil.h"

#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace strome {

std::string Trim(const std::string& value) {
  constexpr char kWhitespace[] = " \t\r\n";
  const size_t begin = value.find_first_not_of(kWhitespace);
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = value.find_last_not_of(kWhitespace);
  return value.substr(begin, end - begin + 1);
}

bool HasScheme(const std::string& value) {
  if (value.find("://") != std::string::npos) {
    return true;
  }
  constexpr const char* kSchemes[] = {"about:", "file:", "chrome:",
                                      "view-source:"};
  for (const char* scheme : kSchemes) {
    if (value.rfind(scheme, 0) == 0) {
      return true;
    }
  }
  return false;
}

bool LooksLikeHost(const std::string& value) {
  if (value.find_first_of(" \t\r\n") != std::string::npos) {
    return false;
  }
  return value.find('.') != std::string::npos ||
         value.rfind("localhost", 0) == 0 ||
         value.find(':') != std::string::npos;
}

std::string UrlEncodeQuery(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex << std::uppercase;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else if (c == ' ') {
      escaped << "%20";
    } else {
      escaped << '%' << std::setw(2) << static_cast<int>(c);
    }
  }
  return escaped.str();
}

std::string NormalizeUrl(const std::string& input) {
  const std::string text = Trim(input);
  if (text.empty()) {
    return std::string();
  }
  if (HasScheme(text)) {
    return text;
  }
  if (LooksLikeHost(text)) {
    if (text.rfind("localhost", 0) == 0) {
      return "http://" + text;
    }
    return "https://" + text;
  }
  return "https://www.google.com/search?q=" + UrlEncodeQuery(text);
}

int ParseInt(const std::string& value, int fallback) {
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const long result = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    return fallback;
  }
  return static_cast<int>(result);
}

std::string JsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20) {
          out += c;
        }
    }
  }
  return out;
}

}  // namespace strome
