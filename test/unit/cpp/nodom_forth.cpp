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
    TestDLC<nlohmann::json>     dc;

    NDFMachine forth;

    // for test data paths
    std::string nd_home;
    std::string test_json_dir;

    // reset these at the top of your test method
    // for the dtor asserts
    int str_count{ 0 };
    int int_count{ 2 };     // consts 0 and 1
    int float_count{ 0 };

    int extern_str_count{ 0 };
    int extern_int_count{ 0 };
    int extern_float_count{ 0 };
    int unmapped{ 0 };

    DataCacheFixture()
        :nd_home(getenv("ND_HOME"))
    {
        std::stringstream buf;
        buf << nd_home << "\\cfg\\";
        test_json_dir = buf.str();
        std::cout << "==== " << boost::unit_test::framework::current_test_case().p_name << std::endl;
    }

    void assert_cache_state() {
        dc.report_sanity_check();
        dc.report_cache_errors();

        int sc = dc.report_cache_strings(extern_str_count);
        BOOST_TEST(str_count == sc);

        int ic = dc.report_cache_ints(extern_int_count);
        BOOST_TEST(int_count == ic);

        int fc = dc.report_cache_floats(extern_float_count);
        BOOST_TEST(float_count == fc);

        dc.report_address_map(unmapped);
        dc.report_data_refs();
        dc.report_func_maps();
        dc.report_actions();
        std::cout << std::endl;
    }

    ~DataCacheFixture() { }
};


BOOST_FIXTURE_TEST_CASE(NDFNotForth, DataCacheFixture)
{
    std::string data_json_path = test_json_dir + "test_ndf_not_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);

    std::string layout_json_path = test_json_dir + "test_ndf_not_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);

    str_count = 33;
    int_count = 6;
    dc.on_json(data, layout, [&]() { dc.on_init(); });

    BOOST_TEST(dc.widget_vec_size() == 2);
    BOOST_TEST(dc.pushables_size() == 1);

    // find the Button Widgets
    WidgetVec matches;
    dc.find_widget(RenderMethod::Button, matches);
    BOOST_TEST(matches.size() == 2);
    WidgetPtr button_widget1{ matches[0] };
    WidgetPtr button_widget2{ matches[1] };

    // check the result of the NDF computation, which
    // should be queries[0]. NB NDWidget validity window
    // comments re: changed. We cannot use changed here
    // in the absence of NDContext change mgmt.
    nlohmann::json save_enabled = data["save_enabled"];
    nlohmann::json discard_enabled = data["discard_enabled"];

    // "save_enabled not" should give us true as save_enabled==false
    DataRef* ndf_save_bool_data_ref = dc.cspec_data_ref(CacheSpecifier::cs_disabled, button_widget1);
    BOOST_TEST(ndf_save_bool_data_ref != nullptr);
    BoolInx binx1(ndf_save_bool_data_ref->ref_inx);
    bool* bool_ptr1 = dc.get_bool_value(binx1);
    BOOST_TEST(bool_ptr1 != nullptr);
    BOOST_TEST(*bool_ptr1 == true);

    // "discard_enabled not" should give us false as discard_enabled==true
    DataRef* ndf_discard_bool_data_ref = dc.cspec_data_ref(CacheSpecifier::cs_disabled, button_widget2);
    BOOST_TEST(ndf_discard_bool_data_ref != nullptr);
    BoolInx binx2(ndf_discard_bool_data_ref->ref_inx);
    bool* bool_ptr2 = dc.get_bool_value(binx2);
    BOOST_TEST(bool_ptr2 != nullptr);
    BOOST_TEST(*bool_ptr2 == false);

    // change underlying values and recalc
    std::string save_enabled_cs{ "save_enabled" };
    AddrInx save_enabled_inx = dc.get_addr_inx(save_enabled_cs);
    DataRef* save_enabled_data_ref = dc.get_data_ref(save_enabled_inx);
    BOOST_TEST(save_enabled_data_ref != nullptr);
    BoolInx binx1a(save_enabled_data_ref->ref_inx);
    bool* bool_ptr1a = dc.get_bool_value(binx1a);
    BOOST_TEST(bool_ptr1a != nullptr);
    // check underlying val of save_enabled is still false
    BOOST_TEST(*bool_ptr1a == false);

    std::string discard_enabled_cs{ "discard_enabled" };
    AddrInx discard_enabled_inx = dc.get_addr_inx(discard_enabled_cs);
    DataRef* discard_enabled_data_ref = dc.get_data_ref(discard_enabled_inx);
    BOOST_TEST(discard_enabled_data_ref != nullptr);
    BoolInx binx2a(discard_enabled_data_ref->ref_inx);
    bool* bool_ptr2a = dc.get_bool_value(binx2a);
    BOOST_TEST(bool_ptr2a != nullptr);
    // check underlying val of discard_enabled is still true
    BOOST_TEST(*bool_ptr2a == true);

    // Now change the underlying val and force a recalc
    *bool_ptr1a = true;
    *bool_ptr2a = false;

    // To trigger the recalc we repro some of the
    // NDContext::end_render_cycle() dirty vec impl
    UintVec dirty_bool_addr_vec;
    UintVec dirty_bool_ref_vec;
    dirty_bool_addr_vec.push_back(save_enabled_data_ref->addr_inx());
    dirty_bool_ref_vec.push_back(save_enabled_data_ref->ref_inx);
    dirty_bool_addr_vec.push_back(discard_enabled_data_ref->addr_inx());
    dirty_bool_ref_vec.push_back(discard_enabled_data_ref->ref_inx);
    dc.on_dirty(dirty_bool_addr_vec, dirty_bool_ref_vec,
        dc.get_bool_driven_widget_vecs(), dc.get_bool_driven_cspec_vecs());

    // and now "save_enabled not" should give us false as save_enabled==true
    ndf_save_bool_data_ref = dc.cspec_data_ref(CacheSpecifier::cs_disabled, button_widget1);
    BOOST_TEST(ndf_save_bool_data_ref != nullptr);
    BoolInx binx1b(ndf_save_bool_data_ref->ref_inx);
    bool* bool_ptr1b = dc.get_bool_value(binx1b);
    BOOST_TEST(bool_ptr1b != nullptr);
    BOOST_TEST(*bool_ptr1b == false);

    // and now "discard_enabled not" should give us true as discard_enabled==false
    ndf_discard_bool_data_ref = dc.cspec_data_ref(CacheSpecifier::cs_disabled, button_widget2);
    BOOST_TEST(ndf_discard_bool_data_ref != nullptr);
    BoolInx binx2b(ndf_discard_bool_data_ref->ref_inx);
    bool* bool_ptr2b = dc.get_bool_value(binx2b);
    BOOST_TEST(bool_ptr2b != nullptr);
    BOOST_TEST(*bool_ptr2b == true);

    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(NDFInForth, DataCacheFixture)
{
    std::string data_json_path = test_json_dir + "test_ndf_not_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);

    std::string layout_json_path = test_json_dir + "test_ndf_not_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);

    str_count = 33;
    int_count = 6;
    dc.on_json(data, layout, [&]() { dc.on_init(); });

    BOOST_TEST(dc.widget_vec_size() == 2);
    BOOST_TEST(dc.pushables_size() == 1);

    // check for true result
    std::string source1{ "queries query2 in" };
    bool compiled = dc.ut_compile_forth(forth, cdBool, source1, data);
    BOOST_TEST(compiled == true);
    bool execed = dc.ut_execute_forth(forth);
    BOOST_TEST(execed == true);

    // check the result
    BOOST_TEST(forth.result_type == cdBool);
    BOOST_TEST(forth.stack.size() == 1);

    DataRef* result_data_ref = forth.stack.back();
    BOOST_TEST(result_data_ref != nullptr);
    BOOST_TEST(result_data_ref->tipe == cdBool);

    bool* query2_in_queries = dc.get_bool_value(result_data_ref->ref_inx);
    BOOST_TEST(query2_in_queries != nullptr);
    BOOST_TEST(*query2_in_queries == true);

    // check for false result
    std::string source2{ "queries query3 in" };
    compiled = dc.ut_compile_forth(forth, cdBool, source2, data);
    BOOST_TEST(compiled == true);
    execed = dc.ut_execute_forth(forth);
    BOOST_TEST(execed == true);

    // check the result
    BOOST_TEST(forth.result_type == cdBool);
    BOOST_TEST(forth.stack.size() == 1);

    result_data_ref = forth.stack.back();
    BOOST_TEST(result_data_ref != nullptr);
    BOOST_TEST(result_data_ref->tipe == cdBool);

    bool* query3_in_queries = dc.get_bool_value(result_data_ref->ref_inx);
    BOOST_TEST(query3_in_queries != nullptr);
    BOOST_TEST(*query3_in_queries == false);

    assert_cache_state();
}

BOOST_FIXTURE_TEST_CASE(NDFImportForth, DataCacheFixture)
{
    std::string data_json_path = test_json_dir + "test_ndf_import_data.json";
    std::string data_json = load_json(data_json_path.c_str());
    auto data = JParse<nlohmann::json>(data_json);

    std::string layout_json_path = test_json_dir + "test_ndf_import_layout.json";
    std::string layout_json = load_json(layout_json_path.c_str());
    auto layout = JParse<nlohmann::json>(layout_json);

    str_count = 33;
    int_count = 6;
    dc.on_json(data, layout, [&]() { dc.on_init(); });

    BOOST_TEST(dc.widget_vec_size() == 2);
    BOOST_TEST(dc.pushables_size() == 1);
}