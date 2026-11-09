#include "handler.hpp"

#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"

namespace {
SimpleHandler* g_instance = nullptr;
}

SimpleHandler::SimpleHandler() {
  DCHECK(!g_instance);
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  browser_map_[browser->GetIdentifier()] = browser;
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
  if (browser_view) {
    CefRefPtr<CefWindow> window = browser_view->GetWindow();
    if (window) {
      window->SetTitle(title);
    }
  }
}

void SimpleHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode) {
  CEF_REQUIRE_UI_THREAD();

  if (!frame->IsMain()) {
    return;
  }
}


bool SimpleHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();


  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  browser_map_.erase(browser->GetIdentifier());
  if (browser_map_.empty()) {
    CefQuitMessageLoop();
  }
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
                                        force_close));
    return;
  }

  is_closing_ = true;

  if (browser_map_.empty()) {
    return;
  }

  BrowserMap browser_map_copy = browser_map_;
  for (auto& pair : browser_map_copy) {
    pair.second->GetHost()->CloseBrowser(force_close);
  }
}


void SimpleHandler::ShowMainWindow() {
  // Empty stub. A proper implementation would show/activate the main window.
  // Called on macOS when user clicks the dock icon while app is running.
}
