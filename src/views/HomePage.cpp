#include "HomePage.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include "include/cef_path_util.h"
#include "include/cef_scheme.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"

namespace views {
namespace {

    std::string get_executable_dir() {
        CefString path;
        if (!CefGetPath(PK_DIR_EXE, path)) {
            return "";
        }
        return path.ToString();
    }

    std::string read_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    std::string get_menu_html() {
        const std::string exe_dir = get_executable_dir();
        const std::string candidates[] = {
            exe_dir + "/../Resources/views/menu.html",
            exe_dir + "/views/menu.html",
            "views/menu.html",
            "src/views/menu.html",
        };

        for (const auto& path : candidates) {
            std::string html = read_file(path);
            if (!html.empty()) {
                return html;
            }
        }

        return "<html><body>menu.html not found</body></html>";
    }

class StromeSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {

    static std::string menu_html(get_menu_html());
    if (menu_html.empty()) {
      menu_html = "<html><body>menu page unavailable</body></html>";
    }

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        menu_html.data(), menu_html.size());
    return new CefStreamResourceHandler("text/html", stream);
  }

 private:
  IMPLEMENT_REFCOUNTING(StromeSchemeHandlerFactory);
};

}  // namespace

void Register() {
  CefRegisterSchemeHandlerFactory(scheme, "ui",
                                  new StromeSchemeHandlerFactory());
}

}
