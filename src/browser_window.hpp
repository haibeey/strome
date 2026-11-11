#ifndef STROME_BROWSER_WINDOW_H_
#define STROME_BROWSER_WINDOW_H_

#include <map>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_request_context.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"

// Owns one top-level CefWindow and the tabs displayed inside it.
//
// Each tab is a CefBrowserView backed by its own CefRequestContext. A context
// created with an empty cache_path is an in-memory (incognito) profile, so two
// tabs never share cookies, local storage, cache or sessions.
class BrowserWindow : public CefWindowDelegate,
                      public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefTextfieldDelegate,
                      public CefButtonDelegate {
 public:
  explicit BrowserWindow(std::string initial_url);
  ~BrowserWindow() override;

  // Called by BrowserWindowManager to identify this window on destruction.
  void SetManagerId(int manager_id) { manager_id_ = manager_id; }
  int GetManagerId() const { return manager_id_; }

  // Brings the window to the front (used when the macOS dock icon is clicked).
  void Activate();

  // Requests that all tab browsers close. When |force_close| is false,
  // beforeunload handlers are allowed to run.
  void Close(bool force_close);
  bool IsClosing() const { return closing_window_; }

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;
  CefSize GetPreferredSize(CefRefPtr<CefView> view) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;
  cef_runtime_style_t GetWindowRuntimeStyle() override;

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }

  // CefDisplayHandler methods:
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefTextfieldDelegate methods:
  using CefWindowDelegate::OnKeyEvent;
  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                  const CefKeyEvent& event) override;

  // CefButtonDelegate methods:
  void OnButtonPressed(CefRefPtr<CefButton> button) override;

 private:
  struct Tab {
    int id = 0;
    int browser_id = -1;
    std::string title;
    std::string url;
    CefRefPtr<CefRequestContext> request_context;
    CefRefPtr<CefBrowserView> browser_view;
    CefRefPtr<CefPanel> strip_item;
    CefRefPtr<CefLabelButton> title_button;
    CefRefPtr<CefLabelButton> close_button;
  };

  void BuildUi(CefRefPtr<CefWindow> window);
  void CreateTab(const std::string& url,
                 CefRefPtr<CefRequestContext> request_context = nullptr);
  void ActivateTab(int tab_id);
  void CloseTab(int tab_id);
  void CloseActiveTab();
  void NavigateTo(const std::string& input);

  void UpdateTabStrip();
  void UpdateWindowTitle();

  Tab* GetTabById(int tab_id);
  Tab* GetTabByBrowserId(int browser_id);
  Tab* GetActiveTab();

  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefPanel> tab_strip_;
  CefRefPtr<CefPanel> toolbar_;
  CefRefPtr<CefPanel> content_;
  CefRefPtr<CefTextfield> url_textfield_;
  CefRefPtr<CefLabelButton> new_tab_button_;
  CefRefPtr<CefLabelButton> go_button_;
  CefRefPtr<CefBrowserViewDelegate> browser_view_delegate_;

  std::vector<Tab> tabs_;
  std::map<int, int> browser_to_tab_;
  int active_tab_id_ = 0;
  int next_tab_id_ = 1;
  int pending_tab_id_ = 0;
  int open_tab_browsers_ = 0;
  int manager_id_ = 0;
  std::string initial_url_;
  bool closing_window_ = false;

  IMPLEMENT_REFCOUNTING(BrowserWindow);
  DISALLOW_COPY_AND_ASSIGN(BrowserWindow);
};

#endif  // STROME_BROWSER_WINDOW_H_
