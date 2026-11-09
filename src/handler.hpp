#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_

#include <map>

#include "include/cef_client.h"

class SimpleHandler : public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler {
 public:
  SimpleHandler();
  ~SimpleHandler() override;

  static SimpleHandler* GetInstance();

  // CefClient methods:
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefProcessId source_process,
                                 CefRefPtr<CefProcessMessage> message) override;

  // CefDisplayHandler methods:
  void OnTitleChange(CefRefPtr<CefBrowser> browser,
                     const CefString& title) override;

  // CefLifeSpanHandler methods:
  // NEW: Track browser creation
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefLoadHandler methods:
  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override;

  void CloseAllBrowsers(bool force_close);
  bool IsClosing() const { return is_closing_; }
  void ShowMainWindow();

 private:
  // NEW: Track all browser instances (keyed by browser ID)
  // These members are only accessed on the UI thread (OnAfterCreated,
  // OnBeforeClose, and CloseAllBrowsers all run on TID_UI), so no
  // additional synchronization is needed.
  using BrowserMap = std::map<int, CefRefPtr<CefBrowser>>;
  BrowserMap browser_map_;

  // NEW: Track whether we're closing
  bool is_closing_ = false;

  IMPLEMENT_REFCOUNTING(SimpleHandler);
  DISALLOW_COPY_AND_ASSIGN(SimpleHandler);
};

#endif  // CEF_TESTS_CEFSIMPLE_SIMPLE_HANDLER_H_
