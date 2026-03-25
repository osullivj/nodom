#include <stdlib.h>
#include "json_ops.hpp"
#include "dl_cache.hpp"
#include "static_strings.hpp"
#define BOOST_TEST_MODULE Data_Cache_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>
#include <filesystem>


#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

template <typename JSON>
struct TestDLC : public DataLayCache<JSON> {
    int report_cache_strings() {
        int fp_len = fp_char_ptrs.size();
        int cs_len = cache_strings.size();
        int ptr_val{ 0 };
        int backed{ 0 };
        std::cout << "== report_cache_strings ptrs:" 
            << fp_len << ", cached:" << cs_len << std::endl;
        for (int inx = 0; inx < cs_len; inx++) {
            std::cout << std::setfill('0') << std::setw(2) << inx << ":";
            std::cout << cache_strings[inx] << ":";
            const char* cache_ptr = cache_strings[inx].c_str();
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << (int)cache_ptr << ":";
            const char* fast_ptr = fp_char_ptrs[inx];
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << (int)fast_ptr;
            if (cache_ptr == fast_ptr) {
                std::cout << ":BACKED";
                backed++;
            }
            std::cout << std::endl;
        }
        return backed;
    }

    int report_cache_ints() {
        int fp_len = fp_int_ptrs.size();
        int cs_len = cache_ints.size();
        int ptr_val{ 0 };
        int backed{ 0 };
        std::cout << "== report_cache_ints ptrs:"
            << fp_len << ", cached:" << cs_len << std::endl;
        for (int inx = 0; inx < cs_len; inx++) {
            std::cout << std::setfill('0') << std::setw(2) << inx << ":";
            std::cout << cache_ints[inx] << ":";
            int* cache_ptr = &(cache_ints[inx]);
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << (int)cache_ptr << ":";
            int* fast_ptr = fp_int_ptrs[inx];
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << (int)fast_ptr;
            if (cache_ptr == fast_ptr) {
                std::cout << ":BACKED";
                backed++;
            }
            std::cout << std::endl;
        }
        return backed;
    }

    int report_cache_floats() {
        int fp_len = fp_float_ptrs.size();
        int cs_len = cache_floats.size();
        int ptr_val{ 0 };
        int backed{ 0 };
        std::cout << "== report_cache_floats ptrs:"
            << fp_len << ", cached:" << cs_len << std::endl;
        for (int inx = 0; inx < cs_len; inx++) {
            std::cout << std::setfill('0') << std::setw(2) << inx << ":";
            std::cout << cache_floats[inx] << ":";
            float* cache_ptr = &(cache_floats[inx]);
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << (int)cache_ptr << ":";
            float* fast_ptr = fp_float_ptrs[inx];
            std::cout << "0x" << std::setfill('0') << std::setw(8) << std::hex << (int)fast_ptr;
            if (cache_ptr == fast_ptr) {
                std::cout << ":BACKED";
                backed++;
            }
            std::cout << std::endl;
        }
        return backed;
    }

    void report_address_map() {
        int len = address_map.size();
        int inx{ 0 };
        std::cout << "== report_address_map len:" << len << std::endl;
        for (auto cit = address_map.cbegin(); cit != address_map.cend(); ++cit) {
            std::cout << std::setfill('0') << std::setw(2) << inx++ << ":";
            std::cout << cit->first << ":" << cit->second << std::endl;
        }
    }

    void report_actions() {
        int key_inx{ 0 };
        int actions_len = actions.size();
        std::cout << "== report_action_map len:" << actions_len << std::endl;
        for (auto cit = actions.cbegin(); cit != actions.cend(); ++cit) {
            const ActionKey& key{ cit->first };
            const ActionVec& action_vec{ cit->second };
            // action_intern_vec should be same len as action_vec (see 
            // DLC::parse_actions()). So the resize() below should be a 
            // null op. But if not we'll dflt ctor NDActionInterned
            // to match lengths. The dflt constructed NDActionInterned will
            // all show as nullptr populated anyway...
            ActionInternVec& action_intern_vec{ actions_interned[key] };
            action_intern_vec.resize(action_vec.size());
            std::cout << std::setfill('0') << std::setw(2) << key_inx++ << ":";
            std::cout << key << std::endl;
            for (int act_inx = 0; act_inx < action_vec.size(); act_inx++) {
                print_parsed_action(action_vec[act_inx], action_intern_vec[act_inx]);
            }
        }
    }
};

struct DataCacheFixture { 
#ifdef __EMSCRIPTEN__
    TestDLC<emscripten::val>   dc;
#else
    TestDLC<nlohmann::json>   dc;
#endif

    // cf NDContext::style_coloring
    int style_coloring{ StyleColor::Dark };
    // cf ImGuiStyle::FontScaleMain
    float font_scale_main{ 1.0 };
    // cf proxy.get_server_url()
    std::string server_url{ "wss://localhost/api/websock" };
    // for test data paths
    std::string nd_home;
    std::string test_json_dir;

    // reset these at the top of your test method
    // for the dtor asserts
    int backed_str_count{ 0 };
    int backed_int_count{ 0 };
    int backed_float_count{ 0 };

    DataCacheFixture()
        :nd_home(getenv("ND_HOME"))
    {
        std::stringstream buf;
        buf << nd_home << "\\cfg\\";
        test_json_dir = buf.str();
        std::cout << "==== " << boost::unit_test::framework::current_test_case().p_name << std::endl;
    }

    void report_cache_state() {
        int bsc = dc.report_cache_strings();
        BOOST_TEST(backed_str_count == bsc);
        int bic = dc.report_cache_ints();
        BOOST_TEST(backed_int_count == bic);
        int bfc = dc.report_cache_floats();
        BOOST_TEST(backed_float_count == bfc);
        dc.report_address_map();
        dc.report_actions();
        std::cout << std::endl;
    }

    ~DataCacheFixture() { }
};

BOOST_FIXTURE_TEST_CASE(AddAddr, DataCacheFixture)
{
    std::string addr_str{ "integer_address" };
    AddrInx addr_inx = dc.add_address(std::string{"integer_address"});
    backed_str_count = 1;

    BOOST_TEST(AddrInx::item_type == CIT::Address);
    BOOST_TEST(AddrInx::data_type == CDT::cdStr);
    BOOST_TEST(addr_inx.magic_index == 0x01040000);
    BOOST_TEST(addr_inx() == 0);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddInt, DataCacheFixture)
{
    std::string addr_str{ "style_coloring" };
    AddrInx addr_inx = dc.add_address(addr_str);
    backed_str_count = 1;
    IntInx int_inx = dc.add_int(&style_coloring);   // not backed
    BOOST_TEST(IntInx::item_type == CIT::Value);
    BOOST_TEST(IntInx::data_type == CDT::cdInt);
    BOOST_TEST(int_inx.magic_index == 0x02010000);
    BOOST_TEST(int_inx() == 0);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddFloat, DataCacheFixture)
{
    AddrInx addr_inx = dc.add_address(std::string{ "font_scale_main" });
    backed_str_count = 1;
    FloatInx float_inx = dc.add_float(&font_scale_main);    // not backed
    BOOST_TEST(FloatInx::item_type == CIT::Value);
    BOOST_TEST(FloatInx::data_type == CDT::cdFloat);
    BOOST_TEST(float_inx.magic_index == 0x02020000);
    BOOST_TEST(float_inx() == 0);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddString, DataCacheFixture)
{
    const static std::string _server_url{ "_server_url" };
    AddrInx addr_inx = dc.add_address(_server_url);
    StrInx str_inx = dc.add_string(_server_url.c_str()); // already exists as address
    backed_str_count = 1;
    BOOST_TEST(StrInx::item_type == CIT::Value);
    BOOST_TEST(StrInx::data_type == CDT::cdStr);
    BOOST_TEST(str_inx.magic_index == 0x02040000);
    BOOST_TEST(str_inx() == 0);
    BOOST_TEST(AddrInx::item_type == CIT::Address);
    BOOST_TEST(AddrInx::data_type == CDT::cdStr);
    BOOST_TEST(addr_inx.magic_index == 0x01040000);
    BOOST_TEST(addr_inx() == 0);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(BadIndex, DataCacheFixture)
{
    // DCI ctor assert only throws in dbg
    BOOST_CHECK_THROW(AddrInx{ MAX_DCI + 1 }, std::exception);
}

BOOST_FIXTURE_TEST_CASE(AddServerData, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    auto data = JParse<emscripten::val>(add_server_data);
#else
    std::string data_json_path = test_json_dir + "test_add_server_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
#endif
    // 3 addresses in AddServer test data
    backed_str_count = 3;
    dc.on_data(data);
    BOOST_TEST(dc.addr_map_size() == 3);
    BOOST_TEST(dc.actions_size() == 0);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddServerLayout, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    // TODO: load from ems FS
    auto layout = JParse<emscripten::val>(add_server_layout);
#else
    std::string layout_json_path = test_json_dir + "test_add_server_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);
#endif
    backed_str_count = 1;   // Home::title
    backed_int_count = 2;   // "step":1, "step":2
    dc.on_layout(layout);
    BOOST_TEST(dc.widget_vec_size() == 1);
    BOOST_TEST(dc.pushables_size() == 0);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(ExfServerData, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    auto data = JParse<emscripten::val>(add_server_data);
#else
    std::string data_json_path = test_json_dir + "test_exf_server_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
#endif
    backed_str_count = 20;
    dc.on_data(data);
    BOOST_TEST(dc.addr_map_size() == 10);
    BOOST_TEST(dc.actions_size() == 4);
    report_cache_state();
}

BOOST_FIXTURE_TEST_CASE(ExfServerLayout, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    // TODO: load from ems FS
    auto layout = JParse<emscripten::val>(add_server_layout);
#else
    std::string layout_json_path = test_json_dir + "test_exf_server_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);
#endif
    backed_str_count = 15;
    backed_int_count = 14;
    dc.on_layout(layout);
    BOOST_TEST(dc.widget_vec_size() == 3);
    BOOST_TEST(dc.pushables_size() == 2);
    report_cache_state();
}

