#ifndef STROME_CHROME_UI_H_
#define STROME_CHROME_UI_H_

namespace chrome_ui {

// The custom scheme used to serve the browser chrome (tab strip + toolbar).
inline constexpr char kScheme[] = "strome";

// URL the chrome browser view loads.
inline constexpr char kUrl[] = "strome://ui/index.html";

// Registers the scheme handler that serves the chrome UI. Must be called once
// in the browser process after CEF is initialized.
void Register();

}  // namespace chrome_ui

#endif  // STROME_CHROME_UI_H_
