#include <cassert>
#include <iostream>
#include <string>

#include "UrlUtil.h"

int main() {
  assert(strome::Trim("  hello \t\n") == "hello");
  assert(strome::Trim("   ") == "");
  assert(strome::Trim("") == "");

  assert(strome::HasScheme("https://example.com"));
  assert(strome::HasScheme("file:///tmp/a.html"));
  assert(strome::HasScheme("about:blank"));
  assert(strome::HasScheme("chrome://version"));
  assert(strome::HasScheme("view-source:https://x.com"));
  assert(!strome::HasScheme("example.com"));

  assert(strome::LooksLikeHost("example.com"));
  assert(strome::LooksLikeHost("localhost"));
  assert(strome::LooksLikeHost("localhost:3000"));
  assert(!strome::LooksLikeHost("hello world"));
  assert(!strome::LooksLikeHost("singleword"));

  assert(strome::NormalizeUrl("  https://example.com  ") ==
         "https://example.com");
  assert(strome::NormalizeUrl("example.com") == "https://example.com");
  assert(strome::NormalizeUrl("localhost:8080") == "http://localhost:8080");
  assert(strome::NormalizeUrl("hello world") ==
         "https://www.google.com/search?q=hello%20world");
  assert(strome::NormalizeUrl("   ") == "");

  assert(strome::UrlEncodeQuery("a b+c") == "a%20b%2Bc");

  assert(strome::ParseInt("42", 0) == 42);
  assert(strome::ParseInt("", 7) == 7);
  assert(strome::ParseInt("x", 7) == 7);
  assert(strome::ParseInt("12x", 7) == 7);

  assert(strome::JsonEscape("a\"b\\c\n\r\t") == "a\\\"b\\\\c\\n\\r\\t");
  assert(strome::JsonEscape("plain") == "plain");

  std::cout << "ok\n";
  return 0;
}
