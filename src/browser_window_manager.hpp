#ifndef STROME_BROWSER_WINDOW_MANAGER_H_
#define STROME_BROWSER_WINDOW_MANAGER_H_

#include <map>
#include <string>

#include "include/cef_base.h"

class BrowserWindow;


class BrowserWindowManager {
 public:
  static BrowserWindowManager* GetInstance();

  void CreateWindow(const std::string& initial_url);
  void CloseAllWindows(bool force_close);

  bool IsClosing() const { return is_closing_; }
  void ShowMainWindow();

 private:
  BrowserWindowManager() = default;
  ~BrowserWindowManager();
  BrowserWindowManager(const BrowserWindowManager&) = delete;
  BrowserWindowManager& operator=(const BrowserWindowManager&) = delete;

  friend class BrowserWindow;
  void OnWindowDestroyed(int manager_id);

  std::map<int, CefRefPtr<BrowserWindow>> windows_;
  int next_window_id_ = 1;
  bool is_closing_ = false;
};

#endif  // STROME_BROWSER_WINDOW_MANAGER_H_
