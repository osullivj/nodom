#include "duckdb.h"
#define BOOST_TEST_MODULE Bulk_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>
#define FMT_HEADER_ONLY
#include "db_cache.hpp"
#include "nd_types.hpp"

using BulkCache_t = BBDuckDBCache;


struct BulkCacheFixture {
    BulkCache_t bulk;
    std::queue<nlohmann::json> responses;

    std::string depth_scan_sql_fmt{ "BEGIN; DROP TABLE IF EXISTS depth; CREATE TABLE depth as select * from read_parquet('{}'); COMMIT;" };
    std::string depth_query_sql{ "select * from depth where LastTradeSize!=0 and AskQty5!=0 and BidQty5!=0 order by SeqNo;" };
    std::string depth_summary_sql{ "summarize select * from depth;" };
    std::string pq_url{ "https://localhost/api/parquet/FGBMU8_20080901_pd.parquet" };
    std::string depth_scan_sql;

    std::string scan_qid{ "depth_scan" };
    std::string select_qid{ "depth_select" };

    BulkCacheFixture() {
        depth_scan_sql = fmt::format(depth_scan_sql_fmt, pq_url);
        bulk.start_db_thread();
    }

    ~BulkCacheFixture() {
        bulk.set_done(true);
    }


    void db_dispatch(const std::string& request_type, const std::string& qid, const std::string& sql) {
        // implicitly using nlohmann JSON here via json_ops.hpp
        auto db_request = JNewObject();
        JSet(db_request, Static::nd_type_cs, request_type);
        JSet(db_request, Static::query_id_cs, qid);
        // BatchRequest just needs QID, no SQL; Command and Query need SQL
        if (request_type != Static::batch_request_cs) { 
            JSet(db_request, Static::sql_cs, sql);
        }
        bulk.db_dispatch(db_request);
    }

    void setup_depth_table() {
        Sleep(1000);
        bulk.get_db_responses(responses);
        BOOST_TEST(!responses.empty());
        BOOST_TEST(responses.size() == 1);
        nlohmann::json resp{ responses.back() };
        responses.pop();

        db_dispatch(Static::command_cs, scan_qid, depth_scan_sql);
        Sleep(1000);
        bulk.get_db_responses(responses);
        BOOST_TEST(!responses.empty());
        BOOST_TEST(responses.size() == 1);

        db_dispatch(Static::query_cs, select_qid, depth_query_sql);
        Sleep(1000);

        db_dispatch(Static::batch_request_cs, select_qid, Static::empty_cs);
        Sleep(1000);
    }
};
   

BOOST_FIXTURE_TEST_CASE(LargeXYRange, BulkCacheFixture)
{
    setup_depth_table();

    RSHandle h = bulk.get_handle(select_qid);
    BOOST_TEST(h != 0);
    uint32_t row_count = bulk.get_row_count(h);
    BOOST_TEST(row_count == 16507);
    // must init metdata
    uint32_t col_count{ 0 };
    int32_t total_plot_count{ 0 };
    bulk.get_meta_data(h, col_count, row_count);
    BOOST_TEST(col_count == 32);
    XYRange* range = bulk.init_xy_range(h, "SeqNo", "AskPrice1", 1340, 4000);
    range = bulk.next_xy_range(range);
    BOOST_TEST(range->plot_count == 708);
    total_plot_count += range->plot_count;
    while (range != nullptr) {
        range = bulk.next_xy_range(range);
        if (range != nullptr) {
            bool plot_count_ok = (range->plot_count == 2048 || range->plot_count == 1244);
            BOOST_TEST(plot_count_ok);
            total_plot_count += range->plot_count;
        }
    }
    BOOST_TEST(total_plot_count == 4000);
}

BOOST_FIXTURE_TEST_CASE(SmallXYRange, BulkCacheFixture)
{
    setup_depth_table();

    RSHandle h = bulk.get_handle(select_qid);
    BOOST_TEST(h != 0);
    uint32_t row_count = bulk.get_row_count(h);
    BOOST_TEST(row_count == 16507);
    // must init metdata
    uint32_t col_count{ 0 };
    int32_t total_plot_count{ 0 };
    bulk.get_meta_data(h, col_count, row_count);
    BOOST_TEST(col_count == 32);
    XYRange* range = bulk.init_xy_range(h, "SeqNo", "AskPrice1", 2780, 420);
    range = bulk.next_xy_range(range);
    BOOST_TEST(range->plot_count == 420);
    total_plot_count += range->plot_count;
    while (range != nullptr) {
        range = bulk.next_xy_range(range);
        if (range != nullptr) {
            bool plot_count_ok = (range->plot_count == 2048 || range->plot_count == 1244);
            BOOST_TEST(plot_count_ok);
            total_plot_count += range->plot_count;
        }
    }
    BOOST_TEST(total_plot_count == 420);
}
