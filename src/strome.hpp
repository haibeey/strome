#ifndef CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_
#define CEF_TESTS_CEFSIMPLE_SIMPLE_APP_H_

#include "include/cef_app.h"

class SimpleApp : public CefApp, public CefBrowserProcessHandler {
 public:
  SimpleApp();

  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  void OnContextInitialized() override;

 private:
  IMPLEMENT_REFCOUNTING(SimpleApp);
  DISALLOW_COPY_AND_ASSIGN(SimpleApp);
};

#endif
