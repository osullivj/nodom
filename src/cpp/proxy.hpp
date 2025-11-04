#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <queue>
#ifndef __EMSCRIPTEN__
#include <duckdb.h>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#endif
#include "json.hpp"

#define ND_WC_BUF_SZ 256


// NDProxy encapsulates the server side. In Breadboard it also hosts
// the DuckDB instance.
template <typename DB>
class NDProxy : public DB {
public:
    NDProxy(int argc, char** argv) {
        std::string usage("breadboard <breadboard_config_json_path> [<server_url>]");
        if (argc < 2) {
            std::cerr << usage << std::endl;
            exit(1);
        }
        exe = argv[0];
        bb_json_path = argv[1];

        if (!std::filesystem::exists(bb_json_path)) {
            std::cerr << usage << std::endl << "Cannot load breadboard config json from " << bb_json_path << std::endl;
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
        std::cout << log_buffer.str() << std::endl;
    }
    virtual         ~NDProxy() {};

    bool            db_app() { return is_db_app; }
    void            set_done(bool d) { done = d; }
    nlohmann::json  get_breadboard_config() { return bb_config; }
    std::string&    get_server_url() { return server_url; }

protected:

private:
    nlohmann::json                      bb_config;
    bool                                is_db_app;
    char* exe;    // argv[0]
    wchar_t                             wc_buf[ND_WC_BUF_SZ];
    char* bb_json_path;

    // these three test config strings are written once at startup
    // time in the cpp thread, and read from the py thread
    std::string                         test_module_name;
    std::queue<nlohmann::json>          server_responses;
    std::string                         server_url;
};
