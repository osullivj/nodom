#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <queue>
#include "logger.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#else
#include "nlohmann.hpp"
#endif

#define ND_WC_BUF_SZ 256


// NDProxy encapsulates the server side.
template <typename DB>
class NDProxy : public DB {
public:

#ifndef __EMSCRIPTEN__
    NDProxy(int argc, char** argv) {
        std::string usage("breadboard <breadboard_config_json_path> [<server_url>]");
        if (argc < 2) {
            std::cerr << usage << std::endl;
            exit(1);
        }
        exe = argv[0];
        bb_json_path = argv[1];

        if (!std::filesystem::exists(bb_json_path)) {
            NDLogger::cerr() << usage << std::endl << "Cannot load breadboard config json from " << bb_json_path << std::endl;
            exit(1);
        }
        try {
            // breadboard.json specifies the base paths for the embedded py
            std::stringstream json_buffer;
            std::ifstream in_file_stream(bb_json_path);
            json_buffer << in_file_stream.rdbuf();
            bb_config = nlohmann::json::parse(json_buffer);
            server_url = bb_config["server_url"];
            is_db_app = bb_config["db_app"];
        }
        catch (nlohmann::json::exception& ex) {

        }
        catch (...) {
            printf("cannot load breadboard.json");
            exit(1);
        }

        std::stringstream log_buffer;
        log_buffer << "NoDOM starting with..." << std::endl;
        log_buffer << "exe: " << exe << std::endl;
        log_buffer << "Breadboard config json: " << bb_json_path << std::endl;
        log_buffer << "Server URL: " << server_url << std::endl;
        NDLogger::cout() << log_buffer.str() << std::endl;
    }
    nlohmann::json  get_breadboard_config() { return bb_config; }
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
#ifndef __EMSCRIPTEN__
    nlohmann::json  bb_config;
    char*           bb_json_path;
    char*           exe;    // argv[0]
    std::string     test_module_name;
#endif
    bool                                is_db_app;
    std::string                         server_url;
};
