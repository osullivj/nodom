#include <stdio.h>
#include "static_strings.hpp"
#include "dl_cache.hpp"

// NDCC: NoDOM compiler

template <typename JSON>
struct DLC : public DataLayCache<JSON> {
    void on_init() { }
};

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage" << std::endl;
        std::cout << "ndcc.exe <jsonDataPath> <jsonLayoutPath>" << std::endl;
        return 1;
    }

    DLC<nlohmann::json> data_lay_cache;
    try {
        std::string init_data(load_json(argv[1]));
        std::string init_layout(load_json(argv[2]));
        auto data = JParse<nlohmann::json>(init_data);
        auto layout = JParse<nlohmann::json>(init_layout);

        data_lay_cache.on_json(data, layout, [&]() {data_lay_cache.on_init(); });

        data_lay_cache.report_sanity_check();
        data_lay_cache.report_cache_state();
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << ex.what() << std::endl;
    }
}
