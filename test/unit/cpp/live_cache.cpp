#include <stdlib.h>
#include "lv_cache.hpp"
#define BOOST_TEST_MODULE Data_Cache_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>
#include <filesystem>


#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

using json_t = nlohmann::json;
using Tiingo_t = TiingoIEXMidRecords<json_t>;
using MktData_t = LiveCache<json_t, Tiingo_t>;

using dbl_nanosecs = std::chrono::duration<double, std::nano>;


struct LiveCacheFixture {
    MktData_t       live;
    nlohmann::json  update;
    nlohmann::json  sub_response;
    StringVec       ticker_list;

    LiveCacheFixture() {
        update[Static::nd_type_cs] = Static::live_update_cs;
        sub_response[Static::nd_type_cs] = Static::live_response_cs;
    }
    ~LiveCacheFixture() { }

    double now_as_double() {
        auto now = std::chrono::system_clock::now();
        auto duration_ns = std::chrono::duration_cast<dbl_nanosecs>(now.time_since_epoch());
        return duration_ns.count();
    }
};

BOOST_FIXTURE_TEST_CASE(SubToOneTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("spy");
    sub_response[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(sub_response);
    BOOST_TEST(live.ticker_count() == 1);
}

BOOST_FIXTURE_TEST_CASE(SubToTwoTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("spy");
    ticker_list.push_back("vym");
    sub_response[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(sub_response);
    BOOST_TEST(live.ticker_count() == 2);
}

BOOST_FIXTURE_TEST_CASE(SubToThreeTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("spy");
    ticker_list.push_back("vym");
    ticker_list.push_back("dell");
    sub_response[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(sub_response);
    BOOST_TEST(live.ticker_count() == 2);
}

BOOST_FIXTURE_TEST_CASE(UpdateOneTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("dell");
    sub_response[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(sub_response);
    update[Static::ticker_cs] = "dell";
    double mid{ 103.25 };
    double ts{ now_as_double() };
    update[Static::mid_cs] = mid;
    update[Static::timestamp_cs] = ts;
    live.on_update(update);
    BOOST_TEST(live.records.mid[0] == mid);
    BOOST_TEST(live.records.time_stamp[0] == ts);
}