#include "browser_window.hpp"

#include <cstdlib>
#include <string>
#include <utility>

#include "browser_window_manager.hpp"
#include "browser_ui.hpp"
#include "include/cef_color_ids.h"
#include "include/cef_load_handler.h"
#include "include/cef_parser.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_request_handler.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_panel_delegate.h"
#include "include/wrapper/cef_helpers.h"

namespace {

constexpr int kWindowId = 1;

// Accelerator command IDs.
constexpr int kCommandNewTab = 30001;
constexpr int kCommandCloseTab = 30002;
constexpr int kCommandFocusUrl = 30003;

// Height reserved at the top of the window for the chrome UI.
constexpr int kTabStripHeight = 40;
constexpr int kToolbarHeight = 36;
constexpr int kChromeHeight = kTabStripHeight + kToolbarHeight;

// Width reserved on the left for the native macOS window buttons.
constexpr int kWindowButtonsWidth = 80;

constexpr char kDefaultUrl[] = "https://google.com";
constexpr cef_color_t kContentBackground = 0xFF202124;
constexpr cef_color_t kTabStripBackground = 0xFF1B1B1E;
constexpr cef_color_t kToolbarBackground = 0xFF2A2B2E;

class AlloyBrowserViewDelegate : public CefBrowserViewDelegate {
 public:
  AlloyBrowserViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(AlloyBrowserViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(AlloyBrowserViewDelegate);
};

// The chrome browser view: alloy style.
class ChromeViewDelegate : public CefBrowserViewDelegate {
 public:
  ChromeViewDelegate() = default;

  cef_runtime_style_t GetBrowserRuntimeStyle() override {
    return CEF_RUNTIME_STYLE_ALLOY;
  }

 private:
  IMPLEMENT_REFCOUNTING(ChromeViewDelegate);
  DISALLOW_COPY_AND_ASSIGN(ChromeViewDelegate);
};

// A panel with a fixed preferred size on either axis (0 = unconstrained).
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

std::string Trim(const std::string& value) {
  constexpr char kWhitespace[] = " \t\r\n";
  const size_t begin = value.find_first_not_of(kWhitespace);
  if (begin == std::string::npos) {
    return std::string();
  }
  const size_t end = value.find_last_not_of(kWhitespace);
  return value.substr(begin, end - begin + 1);
}

bool HasScheme(const std::string& value) {
  if (value.find("://") != std::string::npos) {
    return true;
  }
  constexpr const char* kSchemes[] = {"about:", "file:", "chrome:",
                                      "view-source:"};
  for (const char* scheme : kSchemes) {
    if (value.rfind(scheme, 0) == 0) {
      return true;
    }
  }
  return false;
}

bool LooksLikeHost(const std::string& value) {
  if (value.find_first_of(" \t\r\n") != std::string::npos) {
    return false;
  }
  return value.find('.') != std::string::npos ||
         value.rfind("localhost", 0) == 0 ||
         value.find(':') != std::string::npos;
}

// Turns whatever the user typed into something a browser can load. A bare host
// becomes an https URL, and anything else becomes a search query.
std::string NormalizeUrl(const std::string& input) {
  const std::string text = Trim(input);
  if (text.empty()) {
    return std::string();
  }
  if (HasScheme(text)) {
    return text;
  }
  if (LooksLikeHost(text)) {
    if (text.rfind("localhost", 0) == 0) {
      return "http://" + text;
    }
    return "https://" + text;
  }
  return "https://www.google.com/search?q=" +
         CefURIEncode(CefString(text), false).ToString();
}

int ParseInt(const std::string& value, int fallback) {
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const long result = std::strtol(value.c_str(), &end, 10);
  if (end == value.c_str() || *end != '\0') {
    return fallback;
  }
  return static_cast<int>(result);
}

std::string JsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char c : value) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) >= 0x20) {
          out += c;
        }
    }
  }
  return out;
}

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
  window->CenterWindow(CefSize(1000, 700));
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
  chrome_view_ = nullptr;
  chrome_browser_ = nullptr;
  chrome_view_delegate_ = nullptr;
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
  return CefRect(0, 0, 1000, 700);
}

CefSize BrowserWindow::GetPreferredSize(CefRefPtr<CefView> view) {
  // Only report a preferred size for the top-level window.
  if (view && view->GetID() == kWindowId) {
    return CefSize(1000, 700);
  }
  return CefSize();
}

bool BrowserWindow::OnAccelerator(CefRefPtr<CefWindow> window,
                                  int command_id) {
  CEF_REQUIRE_UI_THREAD();

  switch (command_id) {
    case kCommandNewTab:
      CreateTab(kDefaultUrl);
      return true;
    case kCommandCloseTab:
      CloseActiveTab();
      return true;
    case kCommandFocusUrl:
      if (chrome_browser_ && chrome_browser_->GetMainFrame()) {
        chrome_browser_->GetMainFrame()->ExecuteJavaScript(
            "window.strome && window.strome.focusAddress();",
            chrome_browser_->GetMainFrame()->GetURL(), 0);
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
  *titlebar_height = static_cast<float>(kTabStripHeight);
  return true;
}

void BrowserWindow::OnDraggableRegionsChanged(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const std::vector<CefDraggableRegion>& regions) {
  CEF_REQUIRE_UI_THREAD();

  if (!window_ || !chrome_browser_ || !browser->IsSame(chrome_browser_)) {
    return;
  }

  // The chrome view is offset by the window-buttons gap, so shift its
  // coordinates into window space.
  std::vector<CefDraggableRegion> window_regions = regions;
  for (auto& region : window_regions) {
    region.bounds.x += kWindowButtonsWidth;
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
    PushChromeState();
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
  PushChromeState();
}

void BrowserWindow::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  Tab* tab = GetTabByBrowserId(browser->GetIdentifier());
  if (tab && tab->id == active_tab_id_) {
    PushChromeState();
  }
}

void BrowserWindow::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Popups are not tabs. They keep the opener's request context automatically.
  if (browser->IsPopup()) {
    return;
  }

  CefRefPtr<CefBrowserView> view = CefBrowserView::GetForBrowser(browser);
  if (view && chrome_view_ && view->IsSame(chrome_view_)) {
    chrome_browser_ = browser;
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

  if (chrome_browser_ && browser->IsSame(chrome_browser_)) {
    chrome_browser_ = nullptr;
    return;
  }

  auto it = browser_to_tab_.find(browser->GetIdentifier());
  if (it == browser_to_tab_.end()) {
    return;
  }

  Tab* tab = GetTabById(it->second);
  browser_to_tab_.erase(it);
  if (tab) {
    tab->browser_id = -1;
  }
  if (open_tab_browsers_ > 0) {
    --open_tab_browsers_;
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
  if (!chrome_browser_ || !browser->IsSame(chrome_browser_)) {
    return false;
  }

  const std::string req = request.ToString();
  const size_t sep = req.find('|');
  const std::string command = sep == std::string::npos ? req : req.substr(0, sep);
  const std::string arg =
      sep == std::string::npos ? std::string() : req.substr(sep + 1);

  if (command == "ready") {
    PushChromeState();
  } else if (command == "newtab") {
    CreateTab(kDefaultUrl);
  } else if (command == "activate") {
    const int id = ParseInt(arg, 0);
    if (id != 0) {
      ActivateTab(id);
    }
  } else if (command == "close") {
    const int id = ParseInt(arg, 0);
    if (id != 0) {
      CloseTab(id);
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
  window->SetID(kWindowId);
  window->SetTitle("Strome");

  // Dark theme so the native/OS chrome and any gaps match the UI.
  window->SetThemeColor(CEF_ColorPrimaryBackground, kContentBackground);

  CefBoxLayoutSettings window_settings;
  window_settings.horizontal = false;
  CefRefPtr<CefBoxLayout> window_layout =
      window->SetToBoxLayout(window_settings);

  // Top row: a fixed 80px gap for the native macOS window buttons, then the
  // chrome UI browser view filling the rest. The chrome view must not overlap
  // the button region because browser views render above the native buttons.
  CefRefPtr<CefPanel> chrome_row =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, kChromeHeight));
  CefBoxLayoutSettings row_settings;
  row_settings.horizontal = true;
  CefRefPtr<CefBoxLayout> row_layout = chrome_row->SetToBoxLayout(row_settings);
  window->AddChildView(chrome_row);

  CefRefPtr<CefPanel> gap =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(kWindowButtonsWidth,
                                                       kChromeHeight));
  CefBoxLayoutSettings gap_settings;
  gap_settings.horizontal = false;
  gap->SetToBoxLayout(gap_settings);
  CefRefPtr<CefPanel> gap_strip =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, kTabStripHeight));
  gap_strip->SetBackgroundColor(kTabStripBackground);
  gap->AddChildView(gap_strip);
  CefRefPtr<CefPanel> gap_toolbar =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, kToolbarHeight));
  gap_toolbar->SetBackgroundColor(kToolbarBackground);
  gap->AddChildView(gap_toolbar);
  chrome_row->AddChildView(gap);

  CefRefPtr<CefPanel> chrome_panel =
      CefPanel::CreatePanel(new FixedSizePanelDelegate(0, kChromeHeight));
  chrome_panel->SetToFillLayout();
  chrome_panel->SetBackgroundColor(kTabStripBackground);
  chrome_row->AddChildView(chrome_panel);
  row_layout->SetFlexForView(chrome_panel, 1);

  chrome_view_delegate_ = new ChromeViewDelegate();
  CefBrowserSettings chrome_settings;
  chrome_view_ = CefBrowserView::CreateBrowserView(
      CefRefPtr<CefClient>(this), chrome_ui::kUrl, chrome_settings, nullptr,
      nullptr, chrome_view_delegate_);
  chrome_panel->AddChildView(chrome_view_);

  // All tab BrowserViews overlap here; only the active one is visible.
  content_ = CefPanel::CreatePanel(nullptr);
  content_->SetToFillLayout();
  content_->SetBackgroundColor(kContentBackground);
  window->AddChildView(content_);
  window_layout->SetFlexForView(content_, 1);

  browser_view_delegate_ = new AlloyBrowserViewDelegate();

  message_router_ =
      CefMessageRouterBrowserSide::Create(CefMessageRouterConfig());
  message_router_->AddHandler(this, /*first=*/false);

  // On macOS CEF maps ctrl_pressed to the Command key, so these become Cmd+T,
  // Cmd+W and Cmd+L there and Ctrl+T/W/L on Windows and Linux.
  window->SetAccelerator(kCommandNewTab, 'T', false, true, false, true);
  window->SetAccelerator(kCommandCloseTab, 'W', false, true, false, true);
  window->SetAccelerator(kCommandFocusUrl, 'L', false, true, false, true);

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
  PushChromeState();
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
  const std::string url = NormalizeUrl(input);
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
  PushChromeState();
}

void BrowserWindow::PushChromeState() {
  if (!chrome_browser_ || !chrome_browser_->GetMainFrame()) {
    return;
  }
  CefRefPtr<CefFrame> frame = chrome_browser_->GetMainFrame();
  frame->ExecuteJavaScript("window.strome && window.strome.update(" +
                               BuildChromeState() + ");",
                           frame->GetURL(), 0);
}

std::string BrowserWindow::BuildChromeState() {
  Tab* active = GetActiveTab();

  std::string json = "{\"tabs\":[";
  bool first = true;
  for (const auto& tab : tabs_) {
    if (!first) {
      json += ",";
    }
    first = false;
    json += "{\"id\":" + std::to_string(tab.id) + ",\"title\":\"" +
            JsonEscape(tab.title) + "\",\"active\":" +
            (tab.id == active_tab_id_ ? "true" : "false") + "}";
  }
  json += "]";

  json += ",\"url\":\"" + JsonEscape(active ? active->url : std::string()) + "\"";

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
