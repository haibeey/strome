
Guide to total isolation of each tab.

- Call CefRequestContext::CreateContext every single time a window is opened.
- When spawning a new browser window, dynamically generate a unique, diskless context right before creation:
`cpp
CefBrowserSettings browser_settings;
CefRequestContextSettings context_settings;
`
- Leaving cache_path empty forces it into separate RAM allocations`
context_settings.cache_path = CefString(); 
context_settings.persist_session_cookies = false;`

- Create a brand-new, unique context pointer`
CefRefPtr<CefRequestContext> isolated_context = 
    CefRequestContext::CreateContext(context_settings, nullptr);
`
- Bind the new window strictly to this unique context`
CefWindowInfo window_info;
CefRefPtr<CefClient> client(new MyCefClient());
CefString startup_url = "https://github.com";
CefBrowserHost::CreateBrowser(window_info, client, startup_url, browser_settings, nullptr, isolated_context);
`
- Implement the CefLifeSpanHandler override to catch new popups (like OAuth Google/GitHub login popups). If you don't do this, popups will break out of your isolation walls.
cpp
```
bool MyLifeSpanHandler::OnBeforePopup(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& target_url,
    /* ... other params ... */
    CefRefPtr<CefRequestContext>& extra_info, // <-- ASSIGN HERE
    bool* no_javascript_access) override {

    // Grab the exact context of the parent browser window
    CefRefPtr<CefRequestContext> parent_context = browser->GetHost()->GetRequestContext();

    // Force the popup to stay inside the SAME isolated sandbox
    extra_info = parent_context;

    return false; // Let CEF handle creating the window automatically
}
```
