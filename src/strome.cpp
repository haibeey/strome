#include "strome.hpp"

#include "browser_window_manager.hpp"
#include "browser_ui.hpp"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

StromeApp::StromeApp() = default;

void StromeApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  // The browser chrome is served from this scheme. It is treated as secure so
  // it behaves like a normal app page rather than an opaque origin.
  registrar->AddCustomScheme(chrome_ui::kScheme,
                             CEF_SCHEME_OPTION_STANDARD |
                                 CEF_SCHEME_OPTION_SECURE |
                                 CEF_SCHEME_OPTION_CORS_ENABLED);
}

void StromeApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  // Strome's chrome is always dark, so force the native/OS theme to match.
  command_line->AppendSwitch("force-dark-mode");
}

void StromeApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  chrome_ui::Register();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  std::string url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "https://google.com";
  }

  BrowserWindowManager::GetInstance()->CreateWindow(url);
}
