#ifndef STROME_BROWSER_WINDOW_H_
#define STROME_BROWSER_WINDOW_H_

#include <map>
#include <string>
#include <vector>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_drag_handler.h"
#include "include/cef_request_context.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_panel.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_message_router.h"

// Owns one top-level CefWindow and the tabs displayed inside it.
//
// The browser chrome (tab strip + toolbar) is an HTML page served over the
// "strome://" scheme and shown in a CefBrowserView overlaid on top of the
// window. Each tab is a separate CefBrowserView backed by its own
// CefRequestContext. A context created with an empty cache_path is an in-memory
// (incognito) profile, so two tabs never share cookies, local storage, cache or
// sessions.
class BrowserWindow : public CefWindowDelegate,
                      public CefClient,
                      public CefDisplayHandler,
                      public CefDragHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefRequestHandler,
                      public CefMessageRouterBrowserSide::Handler {
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
  bool IsFrameless(CefRefPtr<CefWindow> window) override;
  bool WithStandardWindowButtons(CefRefPtr<CefWindow> window) override;
  bool GetTitlebarHeight(CefRefPtr<CefWindow> window,
                         float* titlebar_height) override;
  cef_runtime_style_t GetWindowRuntimeStyle() override;

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefDragHandler> GetDragHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  // CefDisplayHandler methods:
  void OnAddressChange(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       const CefString& url) override;
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefDragHandler method:
  void OnDraggableRegionsChanged(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const std::vector<CefDraggableRegion>& regions) override;

  // CefLifeSpanHandler methods:
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override;

  // CefRequestHandler methods:
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override;

  // CefMessageRouterBrowserSide::Handler method:
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override;

 private:
  struct Tab {
    int id = 0;
    int browser_id = -1;
    std::string title;
    std::string url;
    CefRefPtr<CefRequestContext> request_context;
    CefRefPtr<CefBrowserView> browser_view;
  };

  void BuildUi(CefRefPtr<CefWindow> window);
  void CreateTab(const std::string& url,
                 CefRefPtr<CefRequestContext> request_context = nullptr);
  void ActivateTab(int tab_id);
  void CloseTab(int tab_id);
  void CloseActiveTab();
  void NavigateTo(const std::string& input);

  void PushChromeState();
  std::string BuildChromeState();
  void UpdateWindowTitle();

  Tab* GetTabById(int tab_id);
  Tab* GetTabByBrowserId(int browser_id);
  Tab* GetActiveTab();

  CefRefPtr<CefWindow> window_;
  CefRefPtr<CefPanel> content_;
  CefRefPtr<CefBrowserView> chrome_view_;
  CefRefPtr<CefBrowser> chrome_browser_;
  CefRefPtr<CefBrowserViewDelegate> chrome_view_delegate_;
  CefRefPtr<CefBrowserViewDelegate> browser_view_delegate_;
  CefRefPtr<CefMessageRouterBrowserSide> message_router_;

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
