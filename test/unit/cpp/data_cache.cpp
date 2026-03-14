#include "dl_cache.hpp"
#include "json_ops.hpp"
#define BOOST_TEST_MODULE Data_Cache_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>

using JSON = nlohmann::json;

struct DataCacheFixture {
    DataLayCache dc;

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

    DataCacheFixture() {
    }

    ~DataCacheFixture() {
    }
};

BOOST_FIXTURE_TEST_CASE(AddAddr, DataCacheFixture)
{
    std::string addr_str{ "integer_address" };
    AInx addr_inx = dc.add_address(std::string{"integer_address"});

    BOOST_TEST(AInx::item_type == CIT::Address);
    BOOST_TEST(AInx::data_type == CDT::cdStr);
    BOOST_TEST(addr_inx.magic_index == 0x00140000);
    BOOST_TEST(addr_inx() == 0);
}

BOOST_FIXTURE_TEST_CASE(AddInt, DataCacheFixture)
{
    std::string addr_str{ "style_coloring" };
    AInx addr_inx = dc.add_address(addr_str);
    IntInx int_inx = dc.add_int(&style_coloring);
    BOOST_TEST(IntInx::item_type == CIT::Value);
    BOOST_TEST(IntInx::data_type == CDT::cdInt);
    BOOST_TEST(int_inx.magic_index == 0x00210000);
    BOOST_TEST(int_inx() == 0);
}

BOOST_FIXTURE_TEST_CASE(AddFloat, DataCacheFixture)
{
    AInx addr_inx = dc.add_address(std::string{ "font_scale_main" });
    FloatInx float_inx = dc.add_float(&font_scale_main);
    BOOST_TEST(FloatInx::item_type == CIT::Value);
    BOOST_TEST(FloatInx::data_type == CDT::cdFloat);
    BOOST_TEST(float_inx.magic_index == 0x00220000);
    BOOST_TEST(float_inx() == 0);
}

BOOST_FIXTURE_TEST_CASE(AddString, DataCacheFixture)
{
    AInx addr_inx = dc.add_address(std::string{ "_server_url" });
    StrInx str_inx = dc.add_string(server_url.c_str());
    BOOST_TEST(StrInx::item_type == CIT::Value);
    BOOST_TEST(StrInx::data_type == CDT::cdStr);
    BOOST_TEST(str_inx.magic_index == 0x00240001);
    BOOST_TEST(str_inx() == 1);
}

BOOST_FIXTURE_TEST_CASE(BadIndex, DataCacheFixture)
{
    // DCI ctor assert only throws in dbg
    BOOST_CHECK_THROW(AInx{ MAX_DCI + 1 }, std::exception);
}

BOOST_FIXTURE_TEST_CASE(AddServerLayout, DataCacheFixture)
{
    JSON layout = JParse<JSON>(add_server_layout);
    int layout_length = JSize(layout);
    for (int inx = 0; inx < layout_length; inx++) {
        const JSON& w(layout[inx]);
    }
}
