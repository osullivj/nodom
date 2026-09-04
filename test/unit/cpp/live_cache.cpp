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

struct LiveCacheFixture {
    MktData_t       live;
    nlohmann::json  update;
    nlohmann::json  response;
    StringVec       ticker_list;

    LiveCacheFixture() {
        update[Static::nd_type_cs] = Static::live_update_cs;
    }
    ~LiveCacheFixture() { }
};

BOOST_FIXTURE_TEST_CASE(SubToOneTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("spy");
    update[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(update);
    BOOST_TEST(live.ticker_count() == 1);
}

BOOST_FIXTURE_TEST_CASE(SubToTwoTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("spy");
    ticker_list.push_back("vym");
    update[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(update);
    BOOST_TEST(live.ticker_count() == 2);
}

BOOST_FIXTURE_TEST_CASE(SubToThreeTicker, LiveCacheFixture)
{
    live.init(2);
    ticker_list.push_back("spy");
    ticker_list.push_back("vym");
    ticker_list.push_back("dell");
    update[Static::tickers_cs] = JArray(ticker_list);
    live.on_sub(update);
    BOOST_TEST(live.ticker_count() == 2);
}