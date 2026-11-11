#include "browser_window.hpp"

#include <string>
#include <utility>

#include "browser_window_manager.hpp"
#include "include/cef_parser.h"
#include "include/cef_request_context_handler.h"
#include "include/views/cef_box_layout.h"
#include "include/views/cef_fill_layout.h"
#include "include/wrapper/cef_helpers.h"

namespace {

constexpr int kWindowId = 1;
constexpr int kUrlTextfieldId = 2;
constexpr int kGoButtonId = 3;
constexpr int kNewTabButtonId = 4;

// View IDs for tab controls. The tab id is added to each base value, so the
// two ranges must not overlap and must stay below kTabCloseIdBase for titles.
constexpr int kTabTitleIdBase = 1000;
constexpr int kTabCloseIdBase = 100000;

// Accelerator command IDs are separate from View IDs.
constexpr int kCommandNewTab = 30001;
constexpr int kCommandCloseTab = 30002;
constexpr int kCommandFocusUrl = 30003;

constexpr int kReturnKey = 0x0D;
constexpr char kDefaultUrl[] = "https://google.com";

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

std::string DisplayTitle(const std::string& title) {
  const std::string text = title.empty() ? "New Tab" : title;
  constexpr size_t kMaxLength = 24;
  if (text.size() <= kMaxLength) {
    return text;
  }
  return text.substr(0, kMaxLength - 3) + "...";
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

  browser_to_tab_.clear();
  tabs_.clear();
  tab_strip_ = nullptr;
  toolbar_ = nullptr;
  content_ = nullptr;
  url_textfield_ = nullptr;
  new_tab_button_ = nullptr;
  go_button_ = nullptr;
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
  // This object is also the delegate for buttons and the text field, so only
  // report a preferred size for the top-level window.
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
      if (url_textfield_) {
        url_textfield_->RequestFocus();
        url_textfield_->SelectAll(true);
      }
      return true;
    default:
      return false;
  }
}

cef_runtime_style_t BrowserWindow::GetWindowRuntimeStyle() {
  return CEF_RUNTIME_STYLE_ALLOY;
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

  // Do not fight the user while they are editing the address bar.
  if (tab->id == active_tab_id_ && url_textfield_ &&
      !url_textfield_->HasFocus()) {
    url_textfield_->SetText(url);
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
  UpdateTabStrip();
  if (tab->id == active_tab_id_) {
    UpdateWindowTitle();
  }
}

void BrowserWindow::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  // Popups are not tabs. They keep the opener's request context automatically.
  if (browser->IsPopup()) {
    return;
  }

  Tab* tab = nullptr;
  CefRefPtr<CefBrowserView> view = CefBrowserView::GetForBrowser(browser);
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

bool BrowserWindow::OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                               const CefKeyEvent& event) {
  CEF_REQUIRE_UI_THREAD();

  if (event.type == KEYEVENT_RAWKEYDOWN &&
      event.windows_key_code == kReturnKey) {
    NavigateTo(textfield->GetText().ToString());
    return true;
  }
  return false;
}

void BrowserWindow::OnButtonPressed(CefRefPtr<CefButton> button) {
  CEF_REQUIRE_UI_THREAD();

  const int id = button->GetID();
  if (id == kNewTabButtonId) {
    CreateTab(kDefaultUrl);
  } else if (id == kGoButtonId) {
    NavigateTo(url_textfield_ ? url_textfield_->GetText().ToString()
                              : std::string());
  } else if (id >= kTabTitleIdBase && id < kTabCloseIdBase) {
    ActivateTab(id - kTabTitleIdBase);
  } else if (id >= kTabCloseIdBase) {
    CloseTab(id - kTabCloseIdBase);
  }
}

void BrowserWindow::BuildUi(CefRefPtr<CefWindow> window) {
  window_ = window;
  window->SetID(kWindowId);
  window->SetTitle("Strome");

  CefBoxLayoutSettings window_settings;
  window_settings.horizontal = false;
  CefRefPtr<CefBoxLayout> window_layout =
      window->SetToBoxLayout(window_settings);

  // Tab strip.
  tab_strip_ = CefPanel::CreatePanel(nullptr);
  CefBoxLayoutSettings tab_settings;
  tab_settings.horizontal = true;
  tab_settings.between_child_spacing = 2;
  tab_settings.inside_border_horizontal_spacing = 4;
  tab_settings.inside_border_vertical_spacing = 2;
  tab_strip_->SetToBoxLayout(tab_settings);
  window->AddChildView(tab_strip_);

  new_tab_button_ =
      CefLabelButton::CreateLabelButton(CefRefPtr<CefButtonDelegate>(this), "+");
  new_tab_button_->SetID(kNewTabButtonId);
  new_tab_button_->SetFocusable(false);
  new_tab_button_->SetTooltipText("New tab");
  tab_strip_->AddChildView(new_tab_button_);

  // Address bar.
  toolbar_ = CefPanel::CreatePanel(nullptr);
  CefBoxLayoutSettings toolbar_settings;
  toolbar_settings.horizontal = true;
  toolbar_settings.between_child_spacing = 4;
  toolbar_settings.inside_border_horizontal_spacing = 4;
  toolbar_settings.inside_border_vertical_spacing = 2;
  CefRefPtr<CefBoxLayout> toolbar_layout =
      toolbar_->SetToBoxLayout(toolbar_settings);

  url_textfield_ =
      CefTextfield::CreateTextfield(CefRefPtr<CefTextfieldDelegate>(this));
  url_textfield_->SetID(kUrlTextfieldId);
  url_textfield_->SetAccessibleName("Address");
  toolbar_->AddChildView(url_textfield_);
  toolbar_layout->SetFlexForView(url_textfield_, 1);

  go_button_ =
      CefLabelButton::CreateLabelButton(CefRefPtr<CefButtonDelegate>(this), "Go");
  go_button_->SetID(kGoButtonId);
  go_button_->SetFocusable(false);
  toolbar_->AddChildView(go_button_);
  window->AddChildView(toolbar_);

  // All BrowserViews overlap; only the active one is visible.
  content_ = CefPanel::CreatePanel(nullptr);
  content_->SetToFillLayout();
  window->AddChildView(content_);
  window_layout->SetFlexForView(content_, 1);

  browser_view_delegate_ = new AlloyBrowserViewDelegate();

  // On macOS CEF maps ctrl_pressed to the Command key, so these become Cmd+T,
  // Cmd+W and Cmd+L there and Ctrl+T/W/L on Windows and Linux.
  window->SetAccelerator(kCommandNewTab, 'T', false, true, false, true);
  window->SetAccelerator(kCommandCloseTab, 'W', false, true, false, true);
  window->SetAccelerator(kCommandFocusUrl, 'L', false, true, false, true);
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

  tab.strip_item = CefPanel::CreatePanel(nullptr);
  CefBoxLayoutSettings strip_item_settings;
  strip_item_settings.horizontal = true;
  strip_item_settings.between_child_spacing = 2;
  strip_item_settings.inside_border_horizontal_spacing = 6;
  strip_item_settings.inside_border_vertical_spacing = 2;
  tab.strip_item->SetToBoxLayout(strip_item_settings);

  tab.title_button = CefLabelButton::CreateLabelButton(
      CefRefPtr<CefButtonDelegate>(this), DisplayTitle(tab.title));
  tab.title_button->SetID(kTabTitleIdBase + tab.id);
  tab.title_button->SetFocusable(false);
  tab.strip_item->AddChildView(tab.title_button);

  tab.close_button = CefLabelButton::CreateLabelButton(
      CefRefPtr<CefButtonDelegate>(this), "x");
  tab.close_button->SetID(kTabCloseIdBase + tab.id);
  tab.close_button->SetFocusable(false);
  tab.close_button->SetTooltipText("Close tab");
  tab.close_button->SetMinimumSize(CefSize(0, 0));
  tab.strip_item->AddChildView(tab.close_button);

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

  if (tab_strip_) {
    const int insert_index =
        static_cast<int>(tab_strip_->GetChildViewCount()) - 1;
    tab_strip_->AddChildViewAt(tab.strip_item, insert_index);
  }

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

  if (url_textfield_) {
    url_textfield_->SetText(target->url);
  }
  UpdateTabStrip();
  UpdateWindowTitle();

  if (target->browser_view) {
    target->browser_view->RequestFocus();
  }
  if (content_) {
    content_->Layout();
  }
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

  if (tab.strip_item && tab_strip_) {
    tab_strip_->RemoveChildView(tab.strip_item);
  }
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
  if (url_textfield_) {
    url_textfield_->SetText(url);
  }
  browser->GetMainFrame()->LoadURL(url);
  tab->browser_view->RequestFocus();
}

void BrowserWindow::UpdateTabStrip() {
  if (!tab_strip_) {
    return;
  }

  const cef_color_t active_color = CefColorSetARGB(0xFF, 0xE3, 0xE8, 0xF0);
  const cef_color_t inactive_color = CefColorSetARGB(0xFF, 0xF5, 0xF5, 0xF5);

  for (auto& tab : tabs_) {
    if (tab.title_button) {
      tab.title_button->SetText(DisplayTitle(tab.title));
    }
    if (tab.strip_item) {
      tab.strip_item->SetBackgroundColor(
          tab.id == active_tab_id_ ? active_color : inactive_color);
    }
  }
  tab_strip_->Layout();
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
