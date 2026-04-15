#include <stdlib.h>
#include "dl_cache.hpp"
#define BOOST_TEST_MODULE Data_Cache_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>
#include <filesystem>


#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

template <typename JSON>
struct TestDLC : public DataLayCache<JSON> {
    void on_init() {
        // create same inx consts that NDContext would 
        // use for eg ActionKey matching
    }
};

struct DataCacheFixture { 
#ifdef __EMSCRIPTEN__
    TestDLC<emscripten::val>    dc;
#else
    TestDLC<nlohmann::json>     dc;
#endif
    // cf NDContext::style_coloring
    int style_coloring{ StyleColor::Dark };
    // cf ImGuiStyle::FontScaleMain
    float font_scale_main{ 1.0 };
    // cf proxy.get_server_url()
    bool show_footer_db{ false };
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

    void assert_cache_state() {
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
    backed_str_count = 2;

    BOOST_TEST(AddrInx::item_type == CIT::Address);
    BOOST_TEST(AddrInx::data_type == CDT::cdStr);
    BOOST_TEST(addr_inx.magic_index == 0x01040001);
    BOOST_TEST(addr_inx() == 1);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddInt, DataCacheFixture)
{
    std::string addr_str{ "style_coloring" };
    AddrInx addr_inx = dc.add_address(addr_str);
    backed_str_count = 2;
    IntInx int_inx = dc.extern_int(&style_coloring);
    BOOST_TEST(IntInx::item_type == CIT::Value);
    BOOST_TEST(IntInx::data_type == CDT::cdInt);
    BOOST_TEST(int_inx.magic_index == 0x02010000);
    BOOST_TEST(int_inx() == 0);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddBool, DataCacheFixture)
{
    std::string addr_str{ "show_footer_db" };
    AddrInx addr_inx = dc.add_address(addr_str);
    backed_str_count = 2;
    BoolInx bool_inx = dc.extern_bool(&show_footer_db);   // not backed
    BOOST_TEST(IntInx::item_type == CIT::Value);
    BOOST_TEST(IntInx::data_type == CDT::cdInt);
    BOOST_TEST(bool_inx.magic_index == 0x02030000);
    BOOST_TEST(bool_inx() == 0);
    assert_cache_state();
}


BOOST_FIXTURE_TEST_CASE(AddFloat, DataCacheFixture)
{
    AddrInx addr_inx = dc.add_address(std::string{ "font_scale_main" });
    backed_str_count = 2;
    FloatInx float_inx = dc.extern_float(&font_scale_main);    // not backed
    BOOST_TEST(FloatInx::item_type == CIT::Value);
    BOOST_TEST(FloatInx::data_type == CDT::cdFloat);
    BOOST_TEST(float_inx.magic_index == 0x02020000);
    BOOST_TEST(float_inx() == 0);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddString, DataCacheFixture)
{
    const static std::string _server_url{ "_server_url" };
    AddrInx addr_inx = dc.add_address(_server_url);
    // DataCacheFixture::server_url is a standin for Proxy::server_url
    // _server_url is the special cache ref that's not in data
    StrInx str_inx = dc.add_string<CIT::Value>(server_url.c_str());
    backed_str_count = 3;
    BOOST_TEST(StrInx::item_type == CIT::Value);
    BOOST_TEST(StrInx::data_type == CDT::cdStr);
    BOOST_TEST(str_inx.magic_index == 0x02040002);
    BOOST_TEST(str_inx() == 2);
    BOOST_TEST(AddrInx::item_type == CIT::Address);
    BOOST_TEST(AddrInx::data_type == CDT::cdStr);
    BOOST_TEST(addr_inx.magic_index == 0x01040001);
    BOOST_TEST(addr_inx() == 1);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(BadIndex, DataCacheFixture)
{
    // DCI ctor assert only throws in dbg
    BOOST_CHECK_THROW(AddrInx{ MAX_DCI + 1 }, std::exception);
}

BOOST_FIXTURE_TEST_CASE(InitData, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    auto data = JParse<emscripten::val>(add_server_data);
#else
    std::string data_json_path = test_json_dir + "bb_init_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
    auto layout = JParse<nlohmann::json>(Static::empty_list_cs);
#endif
    // 3 addresses in AddServer test data
    backed_str_count = 6;
    dc.on_json(data, layout, [&]() {dc.on_init(); });
    BOOST_TEST(dc.addr_map_size() == 1);
    BOOST_TEST(dc.actions_size() == 2);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(InitLayout, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    // TODO: load from ems FS
    auto layout = JParse<emscripten::val>(add_server_layout);
#else
    std::string data_json_path = test_json_dir + "bb_init_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
    std::string layout_json_path = test_json_dir + "bb_init_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);
#endif
    backed_str_count = 9;
    backed_int_count = 0;
    dc.on_json(data, layout, [&]() { dc.on_init(); });
    BOOST_TEST(dc.widget_vec_size() == 2);
    BOOST_TEST(dc.pushables_size() == 1);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddServerData, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    auto data = JParse<emscripten::val>(add_server_data);
#else
    std::string data_json_path = test_json_dir + "test_add_server_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
    auto layout = JParse<nlohmann::json>(Static::empty_list_cs);
#endif
    // 3 addresses in AddServer test data
    backed_str_count = 3;
    dc.on_json(data, layout, [&]() { dc.on_init(); });
    BOOST_TEST(dc.addr_map_size() == 3);
    BOOST_TEST(dc.actions_size() == 0);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(AddServerLayout, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    // TODO: load from ems FS
    auto layout = JParse<emscripten::val>(add_server_layout);
#else
    std::string data_json_path = test_json_dir + "test_add_server_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
    std::string layout_json_path = test_json_dir + "test_add_server_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);
#endif
    backed_str_count = 4;   // Layout:[Home::title], Data:[op1,op2,op1_plus_op2]
    backed_int_count = 5;   // Layout:["step":1, "step":2], Data:[op1,op2,op1_plus_op2]
    dc.on_json(data, layout, [&]() { dc.on_init(); });
    BOOST_TEST(dc.widget_vec_size() == 1);
    BOOST_TEST(dc.pushables_size() == 0);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(ExfServerData, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    auto data = JParse<emscripten::val>(add_server_data);
#else
    std::string data_json_path = test_json_dir + "test_exf_server_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
    auto layout = JParse<nlohmann::json>(Static::empty_list_cs);

#endif
    backed_str_count = 21;
    dc.on_json(data, layout, [&]() { dc.on_init(); });
    BOOST_TEST(dc.addr_map_size() == 10);
    BOOST_TEST(dc.actions_size() == 4);
    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(ExfServerLayout, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    // TODO: load from ems FS
    auto layout = JParse<emscripten::val>(add_server_layout);
#else
    std::string data_json_path = test_json_dir + "test_exf_server_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);
    std::string layout_json_path = test_json_dir + "test_exf_server_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);
#endif
    backed_str_count = 41;
    backed_int_count = 15;
    dc.on_json(data, layout, [&]() { dc.on_init(); });
    BOOST_TEST(dc.widget_vec_size() == 3);
    BOOST_TEST(dc.pushables_size() == 2);
    assert_cache_state();
}

