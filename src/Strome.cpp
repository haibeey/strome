

#include "include/cef_command_line.h"
#include "include/wrapper/cef_helpers.h"

#include "Strome.hpp"
#include "Manager.hpp"
#include "views/HomePage.hpp"

StromeApp::StromeApp() = default;

void StromeApp::OnRegisterCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {

  registrar->AddCustomScheme(views::scheme,
                             CEF_SCHEME_OPTION_STANDARD |
                                 CEF_SCHEME_OPTION_SECURE |
                                 CEF_SCHEME_OPTION_CORS_ENABLED);
}

void StromeApp::OnBeforeCommandLineProcessing(
    const CefString& process_type,
    CefRefPtr<CefCommandLine> command_line) {
  command_line->AppendSwitch("force-dark-mode");
}

void StromeApp::OnContextInitialized() {
  CEF_REQUIRE_UI_THREAD();

  views::Register();

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  std::string url = command_line->GetSwitchValue("url");
  if (url.empty()) {
    url = "https://google.com";
  }

  BrowserWindowManager::GetInstance()->CreateWindow(url);
}
