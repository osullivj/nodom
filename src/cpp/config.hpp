#pragma once
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <queue>
#include "logger.hpp"
#include "json_ops.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten/val.h>
#else
#include "nlohmann.hpp"
#endif


// NDProxy encapsulates the server side.
template <typename JSON>
class NDConfig {
public:
    static NDConfig& get_instance() {
        // using the Scott Meyers singleton pattern
        static NDConfig instance;
        return instance;
    }

    void initialize(const std::string& json, const char* config_dir = nullptr) {
        config = JParse<JSON>(json);
        if (config_dir != nullptr) {
            JSet(config, Static::config_dir_cs, config_dir);
        }
        init();
    }

    void initialize(const JSON& json) {
        config = json;
        init();
    }

    bool get_value(const char* key, std::string& val) {
        if (JContains(config, key)) {
            val = JAsString(config, key);
            return true;
        }
        return false;
    }

    bool get_value(const char* key, float& val) {
        if (JContains(config, key)) {
            val = JAsFloat(config, key);
            return true;
        }
        return false;
    }

    bool get_nested(const char* key, StringStringMap& ssmap) {
        if (JContains(config, key)) {
            StringVec svec;
            JSON& db_config{ config[key] };
            JKeys(db_config, svec);
            ssmap.clear();
            for (auto it = svec.begin(); it != svec.end(); ++it) {
                ssmap[*it] = JAsString(db_config, it->c_str());
            }
            return true;
        }
        return false;
    }

private:
    void init() {
        static const char* method = "Config::init: ";
#ifdef __EMSCRIPTEN__
        // By the time this method fires in a browser
        // the web page will be fully loaded, so grabbing
        // a global JS obj here should be fine
        emscripten::val window_global = emscripten::val::global("window");
        emscripten::val location = window_global["location"];
        std::string host = location["host"].as<std::string>();
        std::string url("wss://");
        url += host;
        url += "/api/websock";
        config.set(Static::server_url_cs, url.c_str());
#endif
        NDLogger::cout() << method << config << std::endl;
    }

private:
    JSON            config;
};
