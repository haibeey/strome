#include "browser_ui.hpp"

#include <string>

#include "include/cef_scheme.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"

namespace chrome_ui {
namespace {

const char kHtml[] = R"HTML(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body {
    height: 100%; overflow: hidden; background: #1b1b1e; color: #f5f5f5;
    font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "Segoe UI", Roboto, sans-serif;
    font-size: 12px; user-select: none; -webkit-user-select: none;
  }
  #chrome { display: flex; flex-direction: column; height: 100%; -webkit-app-region: drag; }
  .tab, .iconbtn, .navbtn, #address { -webkit-app-region: no-drag; }
  #tabs { display: flex; align-items: flex-end; gap: 2px; height: 40px; padding: 6px 8px 0; background: #1b1b1e; }
  .tab {
    display: flex; align-items: center; gap: 8px; height: 32px; padding: 0 6px 0 10px;
    border-radius: 9px 9px 0 0; background: transparent; color: #9aa0a6;
    min-width: 120px; max-width: 220px; flex: 1 1 0; overflow: hidden;
  }
  .tab.active { background: #34363b; color: #f5f5f5; }
  .tab .favicon { flex: 0 0 auto; display: flex; color: #9aa0a6; }
  .tab.active .favicon { color: #f5f5f5; }
  .tab .title { flex: 1 1 auto; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .tab .close {
    flex: 0 0 auto; width: 18px; height: 18px; border-radius: 50%; display: flex;
    align-items: center; justify-content: center; font-size: 15px; line-height: 1;
    color: #c9ccd1; opacity: 0;
  }
  .tab.active .close, .tab:hover .close { opacity: 1; }
  .tab .close:hover { background: rgba(255,255,255,.16); }
  .iconbtn {
    flex: 0 0 auto; width: 28px; height: 28px; margin-bottom: 2px; border-radius: 7px;
    display: flex; align-items: center; justify-content: center; color: #c9ccd1; font-size: 17px;
  }
  .iconbtn:hover { background: rgba(255,255,255,.1); color: #fff; }
  #toolbar { display: flex; align-items: center; gap: 6px; height: 36px; padding: 0 8px; background: #2a2b2e; }
  .navbtn {
    flex: 0 0 auto; width: 28px; height: 28px; border-radius: 7px; display: flex;
    align-items: center; justify-content: center; color: #c9ccd1;
  }
  .navbtn:hover { background: rgba(255,255,255,.1); color: #fff; }
  .navbtn.disabled { opacity: .35; }
  #address {
    flex: 1 1 auto; height: 28px; border: none; outline: none; border-radius: 14px;
    background: #3a3b3f; color: #f5f5f5; padding: 0 14px; font-size: 12.5px;
  }
  #address::placeholder { color: #9aa0a6; }
  #address:focus { background: #43444a; }
</style>
</head>
<body>
<div id="chrome">
  <div id="tabs"></div>
  <div id="toolbar">
    <div class="navbtn" id="back" title="Back">
      <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M10 3L5 8l5 5"/></svg>
    </div>
    <div class="navbtn" id="forward" title="Forward">
      <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M6 3l5 5-5 5"/></svg>
    </div>
    <div class="navbtn" id="reload" title="Reload">
      <svg width="16" height="16" viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M13 8a5 5 0 1 1-1.46-3.54"/><path d="M13 3.5v3h-3"/></svg>
    </div>
    <input id="address" placeholder="Search or enter address" spellcheck="false" autocomplete="off">
  </div>
</div>
<script>
  var GLOBE = '<svg viewBox="0 0 16 16" width="14" height="14" fill="none" stroke="currentColor" stroke-width="1.1"><circle cx="8" cy="8" r="6.3"/><ellipse cx="8" cy="8" rx="2.9" ry="6.3"/><path d="M1.7 8h12.6"/></svg>';

  var tabsEl = document.getElementById('tabs');
  var addr = document.getElementById('address');
  var backBtn = document.getElementById('back');
  var forwardBtn = document.getElementById('forward');
  var state = { tabs: [], url: '', canBack: false, canForward: false };
  var editing = false;

  function query(req) {
    if (window.cefQuery) {
      window.cefQuery({ request: req, onSuccess: function () {}, onFailure: function () {} });
    }
  }

  function makeTab(t) {
    var el = document.createElement('div');
    el.className = 'tab' + (t.active ? ' active' : '');
    var fav = document.createElement('span');
    fav.className = 'favicon';
    fav.innerHTML = GLOBE;
    var title = document.createElement('span');
    title.className = 'title';
    title.textContent = t.title || 'New Tab';
    var close = document.createElement('span');
    close.className = 'close';
    close.textContent = '\u00D7';
    close.addEventListener('click', function (e) { e.stopPropagation(); query('close|' + t.id); });
    el.appendChild(fav);
    el.appendChild(title);
    el.appendChild(close);
    el.addEventListener('click', function () { query('activate|' + t.id); });
    return el;
  }

  function render() {
    tabsEl.textContent = '';
    for (var i = 0; i < state.tabs.length; i++) tabsEl.appendChild(makeTab(state.tabs[i]));
    var plus = document.createElement('div');
    plus.className = 'iconbtn';
    plus.textContent = '+';
    plus.title = 'New tab';
    plus.addEventListener('click', function () { query('newtab'); });
    tabsEl.appendChild(plus);

    backBtn.classList.toggle('disabled', !state.canBack);
    forwardBtn.classList.toggle('disabled', !state.canForward);
    if (!editing) addr.value = state.url || '';
  }

  window.strome = {
    update: function (s) { state = s || state; render(); },
    focusAddress: function () { editing = true; addr.focus(); addr.select(); }
  };

  addr.addEventListener('focus', function () { editing = true; });
  addr.addEventListener('blur', function () { editing = false; addr.value = state.url || ''; });
  addr.addEventListener('keydown', function (e) {
    if (e.key === 'Enter') { editing = false; query('navigate|' + addr.value); addr.blur(); }
  });
  backBtn.addEventListener('click', function () { query('back'); });
  forwardBtn.addEventListener('click', function () { query('forward'); });
  document.getElementById('reload').addEventListener('click', function () { query('reload'); });

  query('ready');
</script>
</body>
</html>
)HTML";

class ChromeSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    // The HTML is a process-lifetime constant, so the stream can reference it
    // without copying.
    static const std::string html(kHtml);
    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        const_cast<char*>(html.data()), html.size());
    return new CefStreamResourceHandler("text/html", stream);
  }

 private:
  IMPLEMENT_REFCOUNTING(ChromeSchemeHandlerFactory);
};

}  // namespace

void Register() {
  CefRegisterSchemeHandlerFactory(kScheme, "ui",
                                  new ChromeSchemeHandlerFactory());
}

}  // namespace chrome_ui
