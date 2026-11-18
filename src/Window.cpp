
#include <cstdlib>
#include <string>
#include <utility>


#include "include/cef_color_ids.h"
#include "include/cef_load_handler.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_request_handler.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_panel_delegate.h"
#include "include/wrapper/cef_helpers.h"

#include "Window.hpp"
#include "Manager.hpp"
#include "UrlUtil.h"
#include "views/HomePage.hpp"

namespace {

constexpr int window_id = 1;
constexpr int window_height = 700;
constexpr int window_width = 1000;

// Accelerator command IDs.
constexpr int command_new_tab = 30001;
constexpr int command_close_tab = 30002;
constexpr int command_focus_url = 30003;

// Height reserved at the top of the window for the chrome UI.
constexpr int tab_strip_height = 40;
constexpr int toolbar_height = 36;
constexpr int chrome_height = tab_strip_height + toolbar_height;

// Width reserved on the left for the native macOS window buttons.
constexpr int window_buttons_width = 80;

constexpr char default_url[] = "https://google.com";
constexpr cef_color_t content_background = 0xFF202124;
constexpr cef_color_t tab_strip_background = 0xFF1B1B1E;
constexpr cef_color_t toolbar_background = 0xFF2A2B2E;

class AlloyBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  explicit AlloyBrowserViewDelegate(BrowserWindow* owner) : owner_(owner) {}

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

  bool OnPopupBrowserViewCreated(
      CefRefPtr<CefBrowserView> browser_view,
      CefRefPtr<CefBrowserView> popup_browser_view,
      bool is_devtools) override {
    return owner_ && owner_->AdoptPopupView(popup_browser_view);
  }

 private:
  BrowserWindow* owner_;
  IMPLEMENT_REFCOUNTING(AlloyBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(AlloyBrowserViewDelegate);
};

class ViewDelegate : public CefBrowserViewDelegate {
 public:
  ViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(ViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(ViewDelegate);
};


class FixedSizePanelDelegate : public CefPanelDelegate {
 public:
  FixedSizePanelDelegate(int width, int height)
      : width_(width), height_(height) {}

  CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
    return CefSize(width_, height_);
  }

 private:
  const int width_;
  const int height_;
  IMPLEMENT_REFCOUNTING(FixedSizePanelDelegate);
};

}  // namespace

BrowserWindow::BrowserWindow(std::string initial_url)
    : initial_url_(std::move(initial_url)) {}

BrowserWindow::~BrowserWindow() = default;

void BrowserWindow::Activate() {
  if (window_) {
    window_->Show();
    window_->Activate();
  }
}

void BrowserWindow::Close(bool force_close) {
  CEF_REQUIRE_UI_THREAD();

  closing_window_ = true;

  bool any_open = false;
  for (auto& tab : tabs_) {
    CefRefPtr<CefBrowser> browser =
        tab.browser_view ? tab.browser_view->GetBrowser() : nullptr;
    if (browser) {
      any_open = true;
      browser->GetHost()->CloseBrowser(force_close);
    }
  }

  if (!any_open && window_) {
    window_->Close();
  }
}

void BrowserWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();

  BuildUi(window);
  CreateTab(initial_url_);
  window->CenterWindow(CefSize(window_width, window_height));
  window->Show();
}

void BrowserWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();

  if (message_router_) {
    message_router_->RemoveHandler(this);
    message_router_ = nullptr;
  }

  browser_to_tab_.clear();
  tabs_.clear();
  content_ = nullptr;
  view_ = nullptr;
  browser_ = nullptr;
  view_delegate_ = nullptr;
  browser_view_delegate_ = nullptr;
  window_ = nullptr;

  BrowserWindowManager::GetInstance()->OnWindowDestroyed(manager_id_);
}

bool BrowserWindow::CanClose(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();

  bool any_open = false;
  bool all_ready = true;
  for (auto& tab : tabs_) {
    CefRefPtr<CefBrowser> browser =
        tab.browser_view ? tab.browser_view->GetBrowser() : nullptr;
    if (!browser) {
      continue;
    }
    any_open = true;
    if (!browser->GetHost()->IsReadyToBeClosed()) {
      all_ready = false;
      break;
    }
  }

  if (!any_open || all_ready) {
    return true;
  }

  // Ask the browsers to close (running beforeunload handlers) and wait for
  // OnBeforeClose to re-request the window close once they are gone.
  closing_window_ = true;
  for (auto& tab : tabs_) {
    CefRefPtr<CefBrowser> browser =
        tab.browser_view ? tab.browser_view->GetBrowser() : nullptr;
    if (browser) {
      browser->GetHost()->TryCloseBrowser();
    }
  }
  return false;
}

CefRect BrowserWindow::GetInitialBounds(CefRefPtr<CefWindow> window) {
  return CefRect(0, 0, window_width, window_height);
}

CefSize BrowserWindow::GetPreferredSize(CefRefPtr<CefView> view) {
  if (view && view->GetID() == window_id) {
    return CefSize(window_width, window_height);
  }
  return CefSize();
}

bool BrowserWindow::OnAccelerator(CefRefPtr<CefWindow> window,
                                  int command_id) {
  CEF_REQUIRE_UI_THREAD();

  switch (command_id) {
    case command_new_tab:
      CreateTab(default_url);
      return true;
    case command_close_tab:
      CloseActiveTab();
      return true;
    case command_focus_url:
      if (browser_ && browser_->GetMainFrame()) {
        browser_->GetMainFrame()->ExecuteJavaScript(
            "window.strome && window.strome.focusAddress();",
            browser_->GetMainFrame()->GetURL(), 0);
      }
      return true;
    default:
      return false;
  }
}

cef_runtime_style_t BrowserWindow::GetWindowRuntimeStyle() {
  return CEF_RUNTIME_STYLE_ALLOY;
}

bool BrowserWindow::IsFrameless(CefRefPtr<CefWindow> window) {
  // The chrome UI provides the entire window frame, so hide the native title
  // bar. Standard window buttons are still shown on top (see below).
  return true;
}

bool BrowserWindow::WithStandardWindowButtons(CefRefPtr<CefWindow> window) {
  // Keep the native macOS traffic lights, overlaid on the tab strip.
  return true;
}

bool BrowserWindow::GetTitlebarHeight(CefRefPtr<CefWindow> window,
                                      float* titlebar_height) {
  // Position the traffic lights halfway down the tab strip.
  *titlebar_height = static_cast<float>(tab_strip_height);
  return true;
}

void BrowserWindow::OnDraggableRegionsChanged(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const std::vector<CefDraggableRegion>& regions) {
  CEF_REQUIRE_UI_THREAD();

  if (!window_ || !browser_ || !browser->IsSame(browser_)) {
    return;
  }

  // The chrome view is offset by the window-buttons gap, so shift its
  // coordinates into window space.
  std::vector<CefDraggableRegion> window_regions = regions;
  for (auto& region : window_regions) {
    region.bounds.x += window_buttons_width;
  }
  window_->SetDraggableRegions(window_regions);
}

bool BrowserWindow::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  return message_router_ &&
         message_router_->OnProcessMessageReceived(browser, frame,
                                                   source_process, message);
}

bool BrowserWindow::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefRequest> request,
                                   bool user_gesture,
                                   bool is_redirect) {
  if (message_router_) {
    message_router_->OnBeforeBrowse(browser, frame);
  }
  return false;
}

void BrowserWindow::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  CEF_REQUIRE_UI_THREAD();

  if (!frame->IsMain()) {
    return;
  }

  Tab* tab = GetTabByBrowserId(browser->GetIdentifier());
  if (!tab) {
    return;
  }

  tab->url = url.ToString();
  if (tab->id == active_tab_id_) {
    PushState();
  }
}

void BrowserWindow::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  Tab* tab = GetTabByBrowserId(browser->GetIdentifier());
  if (!tab) {
    return;
  }

  tab->title = title.ToString();
  if (tab->id == active_tab_id_) {
    UpdateWindowTitle();
  }
  PushState();
}

void BrowserWindow::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  Tab* tab = GetTabByBrowserId(browser->GetIdentifier());
  if (tab && tab->id == active_tab_id_) {
    PushState();
  }
}

void BrowserWindow::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Popups are not tabs. They keep the opener's request context automatically.
  if (browser->IsPopup()) {
    return;
  }

  CefRefPtr<CefBrowserView> view = CefBrowserView::GetForBrowser(browser);
  if (view && view_ && view->IsSame(view_)) {
    browser_ = browser;
    return;
  }

  Tab* tab = nullptr;
  if (view) {
    for (auto& candidate : tabs_) {
      if (candidate.browser_view.get() == view.get()) {
        tab = &candidate;
        break;
      }
    }
  }
  if (!tab && pending_tab_id_ != 0) {
    tab = GetTabById(pending_tab_id_);
  }
  if (!tab || tab->browser_id >= 0) {
    return;
  }

  tab->browser_id = browser->GetIdentifier();
  browser_to_tab_[tab->browser_id] = tab->id;
  ++open_tab_browsers_;
}

void BrowserWindow::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (message_router_) {
    message_router_->OnBeforeClose(browser);
  }

  if (browser_ && browser->IsSame(browser_)) {
    browser_ = nullptr;
    return;
  }

  auto it = browser_to_tab_.find(browser->GetIdentifier());
  if (it == browser_to_tab_.end()) {
    return;
  }

  Tab* tab = GetTabById(it->second);
  browser_to_tab_.erase(it);
  if (!tab) {
    return;
  }

  tab->browser_id = -1;
  if (open_tab_browsers_ > 0) {
    --open_tab_browsers_;
  }

  // A tab browser can close itself (for example window.close() from a popup
  // that was adopted as a tab). Drop its UI in that case. During a window
  // close the tabs are already being torn down, so only do bookkeeping.
  if (!closing_window_) {
    const int tab_id = tab->id;
    for (size_t i = 0; i < tabs_.size(); ++i) {
      if (tabs_[i].id != tab_id) {
        continue;
      }
      if (tabs_[i].browser_view && content_) {
        content_->RemoveChildView(tabs_[i].browser_view);
      }
      tabs_.erase(tabs_.begin() + i);
      break;
    }

    if (tabs_.empty()) {
      if (window_) {
        window_->Close();
      }
      return;
    }

    if (active_tab_id_ == tab_id) {
      active_tab_id_ = tabs_.front().id;
    }
    ActivateTab(active_tab_id_);
  }

  if (open_tab_browsers_ == 0 && closing_window_ && window_) {
    window_->Close();
  }
}

bool BrowserWindow::OnQuery(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            int64_t query_id,
                            const CefString& request,
                            bool persistent,
                            CefRefPtr<Callback> callback) {
  CEF_REQUIRE_UI_THREAD();

  // Only the chrome UI may drive the window.
  if (!browser_ || !browser->IsSame(browser_)) {
    return false;
  }

  const std::string req = request.ToString();
  const size_t sep = req.find('|');
  const std::string command = sep == std::string::npos ? req : req.substr(0, sep);
  const std::string arg =
      sep == std::string::npos ? std::string() : req.substr(sep + 1);

  if (command == "ready") {
    PushState();
  } else if (command == "newtab") {
    CreateTab(default_url);
  } else if (command == "activate") {
    const int id = strome::ParseInt(arg, 0);
    if (id != 0) {
      ActivateTab(id);
    }
  } else if (command == "close") {
    const int id = strome::ParseInt(arg, 0);
    if (id != 0) {
      CloseTab(id);
    }
  } else if (command == "renametab") {
    const size_t arg_sep = arg.find('|');
    if (arg_sep != std::string::npos) {
      const int id = strome::ParseInt(arg.substr(0, arg_sep), 0);
      const std::string title = arg.substr(arg_sep + 1);
      if (id != 0) {
        Tab* tab = GetTabById(id);
        if (tab) {
          tab->title = title.empty() ? std::string("New Tab") : title;
          UpdateWindowTitle();
          PushState();
        }
      }
    }
  } else if (command == "navigate") {
    NavigateTo(arg);
  } else if (command == "back" || command == "forward" ||
             command == "reload") {
    Tab* tab = GetActiveTab();
    CefRefPtr<CefBrowser> target =
        tab && tab->browser_view ? tab->browser_view->GetBrowser() : nullptr;
    if (target) {
      if (command == "back") {
        target->GoBack();
      } else if (command == "forward") {
        target->GoForward();
      } else {
        target->Reload();
      }
    }
  }

  callback->Success("");
  return true;
}

void BrowserWindow::BuildUi(CefRefPtr<CefWindow> window) {
  window_ = window;
  window->SetID(window_id);
  window->SetTitle("Strome");

  // Dark theme so the native/OS chrome and any gaps match the UI.
  window->SetThemeColor(CEF_ColorPrimaryBackground, content_background);

  CefBoxLayoutSettings window_settings;
  window_settings.horizontal = false;
  CefRefPtr<CefBoxLayout> window_layout =
      window->SetToBoxLayout(window_settings);

  // Top row: a fixed 80px gap for the native macOS window buttons, then the
  // chrome UI browser view filling the rest. The chrome view must not overlap
  // the button region because browser views render above the native buttons.
  CefRefPtr<CefPanel> row =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, chrome_height));
  CefBoxLayoutSettings row_settings;
  row_settings.horizontal = true;
  CefRefPtr<CefBoxLayout> row_layout = row->SetToBoxLayout(row_settings);
  window->AddChildView(row);

  CefRefPtr<CefPanel> gap =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(window_buttons_width,
                                                       chrome_height));
  CefBoxLayoutSettings gap_settings;
  gap_settings.horizontal = false;
  gap->SetToBoxLayout(gap_settings);
  CefRefPtr<CefPanel> gap_strip =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, tab_strip_height));
  gap_strip->SetBackgroundColor(tab_strip_background);
  gap->AddChildView(gap_strip);
  CefRefPtr<CefPanel> gap_toolbar =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, toolbar_height));
  gap_toolbar->SetBackgroundColor(toolbar_background);
  gap->AddChildView(gap_toolbar);
  row->AddChildView(gap);

  CefRefPtr<CefPanel> panel =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, chrome_height));
  panel->SetToFillLayout();
  panel->SetBackgroundColor(tab_strip_background);
  row->AddChildView(panel);
  row_layout->SetFlexForView(panel, 1);

  view_delegate_ = new ViewDelegate();
  CefBrowserSettings settings;
  view_ = CefBrowserView::CreateBrowserView(
      CefRefPtr<CefClient>(this), views::url, settings, nullptr,
      nullptr, view_delegate_);
  panel->AddChildView(view_);

  // All tab BrowserViews overlap here; only the active one is visible.
  content_ = CefPanel::CreatePanel(nullptr);
  content_->SetToFillLayout();
  content_->SetBackgroundColor(content_background);
  window->AddChildView(content_);
  window_layout->SetFlexForView(content_, 1);

  browser_view_delegate_ = new AlloyBrowserViewDelegate(this);

  message_router_ =
      CefMessageRouterBrowserSide::Create(CefMessageRouterConfig());
  message_router_->AddHandler(this, /*first=*/false);

  // On macOS CEF maps ctrl_pressed to the Command key, so these become Cmd+T,
  // Cmd+W and Cmd+L there and Ctrl+T/W/L on Windows and Linux.
  window->SetAccelerator(command_new_tab, 'T', false, true, false, true);
  window->SetAccelerator(command_close_tab, 'W', false, true, false, true);
  window->SetAccelerator(command_focus_url, 'L', false, true, false, true);

  window->ThemeChanged();
}

void BrowserWindow::CreateTab(const std::string& url,
                              CefRefPtr<CefRequestContext> request_context) {
  CEF_REQUIRE_UI_THREAD();

  if (!request_context) {
    // An empty cache_path makes this an in-memory profile: no disk cache and
    // no shared cookies/storage with any other tab.
    CefRequestContextSettings context_settings;
    context_settings.persist_session_cookies = false;
    request_context =
        CefRequestContext::CreateContext(context_settings, nullptr);
  }

  tabs_.emplace_back();
  Tab& tab = tabs_.back();
  tab.id = next_tab_id_++;
  tab.url = url;
  tab.title = "New Tab";
  tab.request_context = request_context;

  CefBrowserSettings browser_settings;
  tab.browser_view = CefBrowserView::CreateBrowserView(
      CefRefPtr<CefClient>(this), CefString(url), browser_settings, nullptr,
      request_context, browser_view_delegate_);
  tab.browser_view->SetVisible(false);

  pending_tab_id_ = tab.id;
  if (content_) {
    content_->AddChildView(tab.browser_view);
  }
  pending_tab_id_ = 0;

  ActivateTab(tab.id);
}

bool BrowserWindow::AdoptPopupView(CefRefPtr<CefBrowserView> popup_view) {
  CEF_REQUIRE_UI_THREAD();

  if (!popup_view || !content_) {
    return false;
  }

  CefRefPtr<CefBrowser> browser = popup_view->GetBrowser();

  tabs_.emplace_back();
  Tab& tab = tabs_.back();
  tab.id = next_tab_id_++;
  tab.title = "Sign in";
  tab.browser_view = popup_view;
  if (browser) {
    tab.browser_id = browser->GetIdentifier();
    browser_to_tab_[tab.browser_id] = tab.id;
    tab.request_context = browser->GetHost()->GetRequestContext();
    ++open_tab_browsers_;
  }

  popup_view->SetVisible(false);
  content_->AddChildView(popup_view);
  ActivateTab(tab.id);
  return true;
}

void BrowserWindow::ActivateTab(int tab_id) {
  Tab* target = GetTabById(tab_id);
  if (!target) {
    return;
  }

  active_tab_id_ = tab_id;
  for (auto& tab : tabs_) {
    if (tab.browser_view) {
      tab.browser_view->SetVisible(tab.id == tab_id);
    }
  }

  if (target->browser_view) {
    target->browser_view->RequestFocus();
  }
  if (content_) {
    content_->Layout();
  }

  UpdateWindowTitle();
  PushState();
}

void BrowserWindow::CloseTab(int tab_id) {
  int index = -1;
  for (size_t i = 0; i < tabs_.size(); ++i) {
    if (tabs_[i].id == tab_id) {
      index = static_cast<int>(i);
      break;
    }
  }
  if (index < 0) {
    return;
  }

  int next_active = 0;
  if (tabs_.size() > 1) {
    next_active = (index + 1 < static_cast<int>(tabs_.size()))
                      ? tabs_[index + 1].id
                      : tabs_[index - 1].id;
  }

  Tab& tab = tabs_[index];

  if (tab.browser_view && content_) {
    content_->RemoveChildView(tab.browser_view);
  }

  CefRefPtr<CefBrowserView> browser_view = tab.browser_view;
  CefRefPtr<CefBrowser> browser =
      browser_view ? browser_view->GetBrowser() : nullptr;

  if (tab.browser_id >= 0) {
    auto it = browser_to_tab_.find(tab.browser_id);
    if (it != browser_to_tab_.end()) {
      browser_to_tab_.erase(it);
      if (open_tab_browsers_ > 0) {
        --open_tab_browsers_;
      }
    }
    tab.browser_id = -1;
  }

  tabs_.erase(tabs_.begin() + index);

  if (browser) {
    browser->GetHost()->CloseBrowser(false);
  }

  if (tabs_.empty()) {
    if (window_) {
      window_->Close();
    }
    return;
  }

  if (active_tab_id_ == tab_id) {
    active_tab_id_ = next_active != 0 ? next_active : tabs_.front().id;
  }
  ActivateTab(active_tab_id_);
}

void BrowserWindow::CloseActiveTab() {
  if (active_tab_id_ != 0) {
    CloseTab(active_tab_id_);
  }
}

void BrowserWindow::NavigateTo(const std::string& input) {
  const std::string url = strome::NormalizeUrl(input);
  if (url.empty()) {
    return;
  }

  Tab* tab = GetActiveTab();
  if (!tab || !tab->browser_view) {
    return;
  }

  CefRefPtr<CefBrowser> browser = tab->browser_view->GetBrowser();
  if (!browser) {
    return;
  }

  tab->url = url;
  browser->GetMainFrame()->LoadURL(url);
  tab->browser_view->RequestFocus();
  PushState();
}

void BrowserWindow::PushState() {
  if (!browser_ || !browser_->GetMainFrame()) {
    return;
  }
  CefRefPtr<CefFrame> frame = browser_->GetMainFrame();
  frame->ExecuteJavaScript("window.strome && window.strome.update(" +
                               BuildState() + ");",
                           frame->GetURL(), 0);
}

std::string BrowserWindow::BuildState() {
  Tab* active = GetActiveTab();

  std::string json = "{\"tabs\":[";
  bool first = true;
  for (const auto& tab : tabs_) {
    if (!first) {
      json += ",";
    }
    first = false;
    json += "{\"id\":" + std::to_string(tab.id) + ",\"title\":\"" +
            strome::JsonEscape(tab.title) + "\",\"active\":" +
            (tab.id == active_tab_id_ ? "true" : "false") + "}";
  }
  json += "]";

  json += ",\"url\":\"" + strome::JsonEscape(active ? active->url : std::string()) + "\"";

  bool can_back = false;
  bool can_forward = false;
  CefRefPtr<CefBrowser> browser =
      active && active->browser_view ? active->browser_view->GetBrowser()
                                     : nullptr;
  if (browser) {
    can_back = browser->CanGoBack();
    can_forward = browser->CanGoForward();
  }
  json += std::string(",\"canBack\":") + (can_back ? "true" : "false");
  json += std::string(",\"canForward\":") + (can_forward ? "true" : "false");
  json += "}";
  return json;
}

void BrowserWindow::UpdateWindowTitle() {
  if (!window_) {
    return;
  }
  Tab* tab = GetActiveTab();
  window_->SetTitle(
      (tab && !tab->title.empty()) ? tab->title : std::string("Strome"));
}

BrowserWindow::Tab* BrowserWindow::GetTabById(int tab_id) {
  for (auto& tab : tabs_) {
    if (tab.id == tab_id) {
      return &tab;
    }
  }
  return nullptr;
}

BrowserWindow::Tab* BrowserWindow::GetTabByBrowserId(int browser_id) {
  auto it = browser_to_tab_.find(browser_id);
  if (it == browser_to_tab_.end()) {
    return nullptr;
  }
  return GetTabById(it->second);
}

BrowserWindow::Tab* BrowserWindow::GetActiveTab() {
  return GetTabById(active_tab_id_);
}
