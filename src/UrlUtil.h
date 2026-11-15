#ifndef STROME_URL_UTIL_H_
#define STROME_URL_UTIL_H_

#include <string>

namespace strome {

std::string Trim(const std::string& value);
bool HasScheme(const std::string& value);
bool LooksLikeHost(const std::string& value);
std::string UrlEncodeQuery(const std::string& value);
std::string NormalizeUrl(const std::string& input);
int ParseInt(const std::string& value, int fallback);
std::string JsonEscape(const std::string& value);

}  // namespace strome

#endif  // STROME_URL_UTIL_H_
