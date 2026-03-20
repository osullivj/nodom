#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <queue>
// #include "nd_types.hpp"
#include "logger.hpp"
#include "json_ops.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#else
#include "nlohmann.hpp"
#endif



// NDProxy encapsulates the server side.
template <typename DB>
class NDProxy : public DB {
public:

#ifndef __EMSCRIPTEN__
    NDProxy(int argc, char** argv) {
        std::string usage("breadboard <breadboard_config_json_path> [<init_data_json path> <init_layout_json_path>]");
        if (argc < 2) {
            std::cerr << usage << std::endl;
            exit(1);
        }
        char* exe = argv[0];
        char* bb_json_path = argv[1];

        try {
            std::string config_json(load_json(bb_json_path));
            // breadboard.json specifies the base paths for the embedded py
            config = JParse<nlohmann::json>(config_json);
            server_url = JAsString(config, "server_url");
            is_db_app = JAsBool(config, "db_app");
        }
        catch (nlohmann::json::exception& ex) {
            printf("JSON_CONFIG_FAIL %s\n%s\n", bb_json_path, ex.what());
            exit(1);
        }
        catch (...) {
            printf("cannot load %s\n", bb_json_path);
            exit(1);
        }

        std::stringstream log_buffer;
        log_buffer << "NoDOM starting with..." << std::endl;
        log_buffer << "exe: " << exe << std::endl;
        log_buffer << "Breadboard config json: " << bb_json_path << std::endl;
        log_buffer << "Server URL: " << server_url << std::endl;
        NDLogger::cout() << log_buffer.str() << std::endl;
    }
#else   // __EMSCRIPTEN__
    NDProxy() {
        const char* method = "NDProxy default ctor: ";
        // By the time the NDProxy ctor fires in a browser
        // the web page will be fully loaded, so grabbing
        // a global JS obj here should be fine
        emscripten::val window_global = emscripten::val::global("window");
        emscripten::val location = window_global["location"];
        std::string host = location["host"].as<std::string>();
        server_url = "wss://" + host + "/api/websock";
        NDLogger::cout() << method << server_url << std::endl;
    }
#endif  // __EMSCRIPTEN__
    virtual         ~NDProxy() {};

    bool            db_app() { return is_db_app; }
    void            set_done(bool d) { done = d; }
    std::string&    get_server_url() { return server_url; }

protected:
    bool            done{false};
private:
    bool            is_db_app;
    std::string     server_url;
};
