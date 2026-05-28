#include <filesystem>
// imgui hdrs
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
// nodom hdrs
#include "static_strings.hpp"
#include "context.hpp"
#include "im_render.hpp"
#include "db_cache.hpp"
#include "websock.hpp"

// NoDOM: this main.cpp is intended to stay as close as possible
// to the imgui/examples/example_glfw_opengl3/main.cpp as that's 
// the appropriate rendeder for the Chrome codebase. JOS 2025-08-25

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

using json_t = nlohmann::json;
using DuckDB_t = BBDuckDBCache;
using NDContext_t = NDContext<json_t, DuckDB_t>;

int main(int argc, char* argv[]) {
    const static char* method = "main: ";

    if (argc < 3) {
        std::cout << Static::breadboard_usage_cs << std::endl;
        exit(1);
    }
    pix_init();

    // load config from localFS
    std::string init_data;
    std::string init_layout;
    try {
        std::string config_dir{ argv[1] };
        std::string app_key{ argv[2] };
        std::string config_file{ app_key };
        config_file += "_config.json";
        std::filesystem::path config_path(config_dir);
        config_path /= config_file;
        if (!std::filesystem::exists(config_path)) {
            std::cout << "cannot open " << config_path.string() << std::endl;
            exit(1);
        }
        std::cout << method << "loading " << config_path.string() << std::endl;
        std::string config_s = load_json(config_path.string().c_str());
        NDConfig<json_t>& cfg{ NDConfig<json_t>::get_instance() };
        cfg.initialize(config_s);

        // load init data and layout from...
        // <config_dir>/<app_key>_init_data.json
        // <config_dir>/<app_key>_init_layout.json
        // ...if they exist
        std::filesystem::path init_data_path(config_dir);
        std::string init_data_file(app_key);
        init_data_file += "_init_data.json";
        init_data_path /= init_data_file;
        if (std::filesystem::exists(init_data_path)) {
            std::cout << method << "loading " << init_data_path.string() << std::endl;
            init_data = load_json(init_data_path.string().c_str());
        }

        std::filesystem::path init_layout_path(config_dir);
        std::string init_layout_file(app_key);
        init_layout_file += "_init_layout.json";
        init_layout_path /= init_layout_file;
        if (std::filesystem::exists(init_layout_path)) {
            std::cout << method << "loading " << init_layout_path.string() << std::endl;
            init_layout = load_json(init_layout_path.string().c_str());
        }
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << ex.what() << std::endl;
        std::cout << Static::breadboard_usage_cs << std::endl;
        exit(1);
    }

    DuckDB_t server;
    NDContext<json_t, DuckDB_t> ctx(server,
        init_data.empty() ? nullptr : init_data.c_str(), 
        init_layout.empty() ? nullptr : init_layout.c_str());

    try {
        // launch DB thread: see db_loop impls
        server.start_db_thread();
        // now launch websock client with a boost::asio
        // event loop on the main thread to dispatch
        // the timeout and on_message callbacks
        NDWebSockClient<json_t, DuckDB_t> ws_client(server, ctx);
        ws_client.run();
    }
    catch (websocketpp::exception const& ex) {
        std::cout << method << ex.what() << std::endl;
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << method << ex.what() << std::endl;
    }
}
