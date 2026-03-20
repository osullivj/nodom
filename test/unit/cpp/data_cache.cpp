#include "json_ops.hpp"
#include "dl_cache.hpp"
#include "static_strings.hpp"
#define BOOST_TEST_MODULE Data_Cache_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>
#include <filesystem>
#include <stdlib.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif


struct DataCacheFixture { 
#ifdef __EMSCRIPTEN__
    DataLayCache<emscripten::val>   dc;
#else
    DataLayCache<nlohmann::json>   dc;
#endif

    // cf NDContext::style_coloring
    int style_coloring{ StyleColor::Dark };
    // cf ImGuiStyle::FontScaleMain
    float font_scale_main{ 1.0 };
    // cf proxy.get_server_url()
    std::string server_url{ "wss://localhost/api/websock" };

    std::string add_server_layout = R"([)"  "\n"  // [
        R"(  {)"                                                    "\n"
        R"(    "rname":"Home",)"                                    "\n"
        R"(    "cspec":{"title":"Server side addition"},)"          "\n"
        R"(    "children":[)"                                       "\n"
        R"(      {)"                                                "\n"
        R"(        "rname":"InputInt",)"                            "\n"
        R"(        "cspec":{"cname":"op1", "step":1})"              "\n"
        R"(      },)"                                               "\n"
        R"(      {)"                                                "\n"
        R"(        "rname":"InputInt",)"                            "\n"
        R"(        "cspec":{"cname":"op2", "step":2})"              "\n"
        R"(      },)"                                               "\n"
        R"(      {)"                                                "\n"
        R"(        "rname":"InputInt",)"                            "\n"
        R"(        "cspec":{"cname":"op1_plus_op2"})"               "\n"
        R"(      })"                                                "\n"
        R"(    ])"                                                  "\n"
        R"(  })"                                                    "\n"
        R"(])"; // ]
    std::string add_server_data = R"({)"    "\n"    // {
        R"(  "op1":2,)"                     "\n"
        R"(  "op2":3,)"                     "\n"
        R"(  "op1_plus_op2":5)"             "\n"
        R"(})";

    std::string nd_home;
    std::string test_json_dir;
    DataCacheFixture()
        :nd_home(getenv("ND_HOME"))
    {
        std::stringstream buf;
        buf << nd_home << "\\cfg\\";
        test_json_dir = buf.str();
    }

    ~DataCacheFixture() {
    }
};

BOOST_FIXTURE_TEST_CASE(AddAddr, DataCacheFixture)
{
    std::string addr_str{ "integer_address" };
    AddrInx addr_inx = dc.add_address(std::string{"integer_address"});

    BOOST_TEST(AddrInx::item_type == CIT::Address);
    BOOST_TEST(AddrInx::data_type == CDT::cdStr);
    BOOST_TEST(addr_inx.magic_index == 0x01040000);
    BOOST_TEST(addr_inx() == 0);
}

BOOST_FIXTURE_TEST_CASE(AddInt, DataCacheFixture)
{
    std::string addr_str{ "style_coloring" };
    AddrInx addr_inx = dc.add_address(addr_str);
    IntInx int_inx = dc.add_int(&style_coloring);
    BOOST_TEST(IntInx::item_type == CIT::Value);
    BOOST_TEST(IntInx::data_type == CDT::cdInt);
    BOOST_TEST(int_inx.magic_index == 0x02010000);
    BOOST_TEST(int_inx() == 0);
}

BOOST_FIXTURE_TEST_CASE(AddFloat, DataCacheFixture)
{
    AddrInx addr_inx = dc.add_address(std::string{ "font_scale_main" });
    FloatInx float_inx = dc.add_float(&font_scale_main);
    BOOST_TEST(FloatInx::item_type == CIT::Value);
    BOOST_TEST(FloatInx::data_type == CDT::cdFloat);
    BOOST_TEST(float_inx.magic_index == 0x02020000);
    BOOST_TEST(float_inx() == 0);
}

BOOST_FIXTURE_TEST_CASE(AddString, DataCacheFixture)
{
    AddrInx addr_inx = dc.add_address(std::string{ "_server_url" });
    StrInx str_inx = dc.add_string(server_url.c_str());
    BOOST_TEST(StrInx::item_type == CIT::Value);
    BOOST_TEST(StrInx::data_type == CDT::cdStr);
    BOOST_TEST(str_inx.magic_index == 0x02040001);
    BOOST_TEST(str_inx() == 1);
}

BOOST_FIXTURE_TEST_CASE(AddRenderMethod, DataCacheFixture)
{
    // TODO: test for RenderMethod in DLCache
    // eg Static::rm_home_cs as RInx
    // NB we need them for NDAction::pop_ui
    RenderInx home_inx = dc.add_render_name(Static::rm_home_cs);
    RenderInx input_int_inx = dc.add_render_name(Static::rm_input_int_cs);
    RenderInx combo_inx = dc.add_render_name(Static::rm_combo_cs);
    BOOST_TEST(combo_inx.magic_index == 0x06040002);
    BOOST_TEST(combo_inx() == 2);
}

BOOST_FIXTURE_TEST_CASE(BadIndex, DataCacheFixture)
{
    // DCI ctor assert only throws in dbg
    BOOST_CHECK_THROW(AddrInx{ MAX_DCI + 1 }, std::exception);
}

BOOST_FIXTURE_TEST_CASE(BadWidgetIndex, DataCacheFixture)
{
    // Create an EntityInx without a CIST ctor param
    BOOST_CHECK_THROW(EntityInx{ 0 }, std::exception);
}

BOOST_FIXTURE_TEST_CASE(AddServerData, DataCacheFixture)
{
#ifdef __EMSCRIPTEN__
    auto data = JParse<emscripten::val>(add_server_data);
#else
    auto data = JParse<nlohmann::json>(add_server_data);
#endif
    dc.on_data(data);
    BOOST_TEST(dc.addr_set_size() == 3);
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
    dc.on_layout(layout);
    BOOST_TEST(dc.widget_vec_size() == 1);
    BOOST_TEST(dc.pushables_size() == 0);
}

