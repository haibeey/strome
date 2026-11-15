#include "HomePage.hpp"

#include <string>
#include <fstream>
#include <sstream>


#include "include/cef_scheme.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"

namespace views {
namespace {

    std::string get_menu_html() {

        std::ifstream file("views/menu.html");
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        }

        return "";

    }

class StromeSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {

    static const std::string menu_html(get_menu_html());
    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        const_cast<char*>(menu_html.data()), menu_html.size());
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
