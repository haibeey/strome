#ifndef STROME_CHROME_UI_H_
#define STROME_CHROME_UI_H_

#include <string>

namespace views {


inline constexpr char scheme[] = "strome";

inline constexpr char url[] = "strome://ui/index.html";

std::string get_menu_html();

void Register();

}  // namespace views

#endif  // STROME_CHROME_UI_H_
