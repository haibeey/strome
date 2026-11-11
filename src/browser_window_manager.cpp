#include "browser_window_manager.hpp"

#include "browser_window.hpp"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

namespace {
constexpr char kDefaultUrl[] = "https://google.com";
}  // namespace

BrowserWindowManager* BrowserWindowManager::GetInstance() {
  static BrowserWindowManager instance;
  return &instance;
}

BrowserWindowManager::~BrowserWindowManager() = default;

void BrowserWindowManager::CreateWindow(const std::string& initial_url) {
  CEF_REQUIRE_UI_THREAD();

  CefRefPtr<BrowserWindow> window = new BrowserWindow(initial_url);
  const int manager_id = next_window_id_++;
  window->SetManagerId(manager_id);
  windows_[manager_id] = window;

  CefWindow::CreateTopLevelWindow(window);
}

void BrowserWindowManager::CloseAllWindows(bool force_close) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&BrowserWindowManager::CloseAllWindows,
                               base::Unretained(this), force_close));
    return;
  }

  is_closing_ = true;

  auto windows_copy = windows_;
  for (auto& pair : windows_copy) {
    if (pair.second) {
      pair.second->Close(force_close);
    }
  }
}

void BrowserWindowManager::ShowMainWindow() {
  CEF_REQUIRE_UI_THREAD();

  if (!windows_.empty()) {
    auto it = windows_.begin();
    if (it->second) {
      it->second->Activate();
    }
    return;
  }

  CreateWindow(kDefaultUrl);
}

void BrowserWindowManager::OnWindowDestroyed(int manager_id) {
  windows_.erase(manager_id);

  if (windows_.empty()) {
    CefQuitMessageLoop();
  }
}
