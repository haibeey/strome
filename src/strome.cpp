#include "strome.hpp"

#include "simple_handler.hpp"

#include "include/cef_browser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_command_line.h"

namespace {

// Window delegate - controls window behavior
class SimpleWindowDelegate : public CefWindowDelegate {
 public:
  explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    // Add the browser view and show the window
    window->AddChildView(browser_view_);
    window->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // Allow the window to close if the browser says it's OK
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(800, 600);
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};

// Browser view delegate
class SimpleBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  SimpleBrowserViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(SimpleBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(SimpleBrowserViewDelegate);
};

}  // namespace

SimpleApp::SimpleApp() = default;

void SimpleApp::OnContextInitialized() {
    CEF_REQUIRE_UI_THREAD();

      // Get command-line arguments
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();

      // Check for --url= parameter
      std::string url = command_line->GetSwitchValue("url");
      if (url.empty()) {
        url = "haibeey.site";  // Default URL
      }

      CefRefPtr<SimpleHandler> handler(new SimpleHandler());
      CefBrowserSettings browser_settings;

      CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
          handler, url, browser_settings, nullptr, nullptr,
          new SimpleBrowserViewDelegate());

      CefWindow::CreateTopLevelWindow(new SimpleWindowDelegate(browser_view));
}
