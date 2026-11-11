#include "include/cef_app.h"
#include "include/wrapper/cef_library_loader.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"

#include "browser_ui.hpp"

// Minimal app used by all CEF sub-processes (renderer, GPU, utility, plugin).
// The renderer process needs the renderer side of the message router so that
// the chrome UI page can use cefQuery() to talk to the browser process, and
// every process must register the custom scheme.
class StromeHelperApp : public CefApp, public CefRenderProcessHandler {
 public:
  StromeHelperApp() = default;

  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  void OnRegisterCustomSchemes(
      CefRawPtr<CefSchemeRegistrar> registrar) override {
    registrar->AddCustomScheme(chrome_ui::kScheme,
                               CEF_SCHEME_OPTION_STANDARD |
                                   CEF_SCHEME_OPTION_SECURE |
                                   CEF_SCHEME_OPTION_CORS_ENABLED);
  }

  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    if (!renderer_router_) {
      renderer_router_ =
          CefMessageRouterRendererSide::Create(CefMessageRouterConfig());
    }
    renderer_router_->OnContextCreated(browser, frame, context);
  }

  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override {
    if (renderer_router_) {
      renderer_router_->OnContextReleased(browser, frame, context);
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    return renderer_router_ && renderer_router_->OnProcessMessageReceived(
                                   browser, frame, source_process, message);
  }

 private:
  CefRefPtr<CefMessageRouterRendererSide> renderer_router_;

  IMPLEMENT_REFCOUNTING(StromeHelperApp);
  DISALLOW_COPY_AND_ASSIGN(StromeHelperApp);
};

int main(int argc, char* argv[]) {
  CefScopedLibraryLoader library_loader;
  if (!library_loader.LoadInHelper()) {
    return 1;
  }

  CefMainArgs main_args(argc, argv);
  CefRefPtr<StromeHelperApp> app(new StromeHelperApp);
  return CefExecuteProcess(main_args, app, nullptr);
}
