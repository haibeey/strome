#include "strome.hpp"

#include "browser_window_manager.hpp"
#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

StromeApp::StromeApp() = default;

void StromeApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  std::string url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "https://google.com";
  }

  BrowserWindowManager::GetInstance()->CreateWindow(url);
}
