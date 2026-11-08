#include "simple_handler.hpp"

#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"

namespace {
// Anonymous namespace - variables here are only visible in this file
SimpleHandler* g_instance = nullptr;

}  // namespace

SimpleHandler::SimpleHandler() {
  DCHECK(!g_instance);  // Debug assertion - crashes if instance already exists
  g_instance = this;
}

SimpleHandler::~SimpleHandler() {
  g_instance = nullptr;
}

// static
SimpleHandler* SimpleHandler::GetInstance() {
  return g_instance;
}

void SimpleHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Add browser to our tracking map
  browser_map_[browser->GetIdentifier()] = browser;
}

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                   const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  // Get the browser view from the browser
  CefRefPtr<CefBrowserView> browser_view =
      CefBrowserView::GetForBrowser(browser);
  if (browser_view) {
    // Get the window and update its title
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

  // Inject test buttons
  std::stringstream ss;
  // "Call C++ Function" button
  ss << "var button1 = document.createElement('button');"
     << "button1.textContent = 'Call C++ Function';"
     << "button1.style.position = 'fixed';"
     << "button1.style.top = '10px';"
     << "button1.style.left = '10px';"
     << "button1.style.zIndex = '10000';"
     << "button1.style.backgroundColor = '#4CAF50';"
     << "button1.style.color = 'white';"
     << "button1.style.padding = '10px 15px';"
     << "button1.style.border = 'none';"
     << "button1.style.borderRadius = '4px';"
     << "button1.style.cursor = 'pointer';"
     << "button1.onclick = function() {"
     << "  var result = sayHello('Hello from JavaScript!');"
     << "  alert(result);"
     << "};"
     << "document.body.appendChild(button1);"
     // "Create Popup Window" button
     << "var button2 = document.createElement('button');"
     << "button2.textContent = 'Create Popup Window';"
     << "button2.style.position = 'fixed';"
     << "button2.style.top = '50px';"
     << "button2.style.left = '10px';"
     << "button2.style.zIndex = '10000';"
     << "button2.style.backgroundColor = '#2196F3';"
     << "button2.style.color = 'white';"
     << "button2.style.padding = '10px 15px';"
     << "button2.style.border = 'none';"
     << "button2.style.borderRadius = '4px';"
     << "button2.style.cursor = 'pointer';"
     << "button2.onclick = function() {"
     << "  window.open('https://www.wikipedia.org', '_blank', 'width=800,height=600');"
     << "};"
     << "document.body.appendChild(button2);";

  frame->ExecuteJavaScript(ss.str(), frame->GetURL(), 0);
}

// Process message received in the browser process
bool SimpleHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();

  // Handle message from MyHandler::Execute() in the renderer process
  if (message->GetName() == "greeting") {
    CefString msg = message->GetArgumentList()->GetString(0);

    // Show the message in a JavaScript alert
    std::stringstream ss;
    ss << "alert('C++ received: " << msg.ToString() << "');";
    frame->ExecuteJavaScript(ss.str(), frame->GetURL(), 0);

    return true;
  }

  return false;
}

void SimpleHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Remove browser from our tracking map
  browser_map_.erase(browser->GetIdentifier());

  // Only quit when all browsers are closed
  if (browser_map_.empty()) {
    CefQuitMessageLoop();
  }
}

void SimpleHandler::CloseAllBrowsers(bool force_close) {
  // CloseAllBrowsers can be called from anywhere (e.g., macOS menu's Quit command),
  // but CEF browser operations must run on the UI thread. Check if we're already
  // on the UI thread; if not, post a task to run this method on the UI thread.
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&SimpleHandler::CloseAllBrowsers, this,
                                        force_close));
    return;
  }

  is_closing_ = true;

  if (browser_map_.empty()) {
    return;
  }

  // Iterate over browsers and request close
  // Use a copy because browser_map_ will be modified during iteration (OnBeforeClose)
  BrowserMap browser_map_copy = browser_map_;
  for (auto& pair : browser_map_copy) {
    pair.second->GetHost()->CloseBrowser(force_close);
  }
}


void SimpleHandler::ShowMainWindow() {
  // Empty stub. A proper implementation would show/activate the main window.
  // Called on macOS when user clicks the dock icon while app is running.
}
