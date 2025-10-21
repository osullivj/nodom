#include <duckdb.h>
#define BOOST_TEST_MODULE Varchar_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>

struct DuckDBFixture {
    duckdb_database db;
    duckdb_connection con;
    std::map<idx_t, std::vector<std::string>> values;

    DuckDBFixture() {
        auto ret_val = duckdb_open(NULL, &db);
        BOOST_TEST(ret_val == DuckDBSuccess);
        ret_val = duckdb_connect(db, &con);
        BOOST_TEST(ret_val == DuckDBSuccess);
        printf("DB connected\n");
    }

    ~DuckDBFixture() {
        duckdb_disconnect(&con);
        duckdb_close(&db);
        printf("DB closed\n");
    }

    void read_and_print_all_columns(duckdb_result& result) {
        duckdb_data_chunk res_chunk = duckdb_result_get_chunk(result, 0);
        idx_t colm_count = duckdb_data_chunk_get_column_count(res_chunk);
        idx_t row_count = duckdb_data_chunk_get_size(res_chunk);
        duckdb_vector vec = duckdb_data_chunk_get_vector(res_chunk, 0);

        int16_t*            sidata = nullptr;
        int32_t*            idata = nullptr;
        int64_t*            bidata = nullptr;
        duckdb_hugeint*     hidata = nullptr;
        duckdb_string_t*    vcdata = nullptr;
        double*             dbldata = nullptr;
        uint64_t*   colm_validity = nullptr;
        duckdb_type decimal_type;
        uint8_t decimal_width = 0;
        uint8_t decimal_scale = 0;
        double  decimal_divisor = 1.0;
        double  decimal_value = 0.0;

        char buf[32];
        char fmtbuf[16];

        for (idx_t row = 0; row < row_count; row++) {
            for (idx_t col = 0; col < colm_count; col++) {
                duckdb_vector colm = duckdb_data_chunk_get_vector(res_chunk, col);
                colm_validity = duckdb_vector_get_validity(colm);
                duckdb_logical_type type_l = duckdb_vector_get_column_type(colm);
                duckdb_type type = duckdb_get_type_id(type_l);
                // yes, this fires the default ctor for std::vector<std::string>
                // on 1st visit to a column
                std::vector<std::string>& strvec(values[col]);
                if (!duckdb_validity_row_is_valid(colm_validity, row)) {
                    printf("NULL\t");
                    continue;
                }
                // https://duckdb.org/docs/stable/clients/c/vector
                // NB the type mappings
                switch (type) {
                case DUCKDB_TYPE_TINYINT:   // int8_t
                    BOOST_FAIL("DUCKDB_TYPE_TINYINT unimplemented");
                    break;
                case DUCKDB_TYPE_SMALLINT:  // int16_t
                    BOOST_FAIL("DUCKDB_TYPE_SMALLINT unimplemented");
                    break;
                case DUCKDB_TYPE_INTEGER:   // int32_t
                    idata = (int32_t*)duckdb_vector_get_data(colm);
                    sprintf(buf, "%I32d", idata[row]);
                    printf(buf);
                    strvec.push_back(buf);
                    break;
                case DUCKDB_TYPE_UTINYINT:  // uint8_t
                    BOOST_FAIL("DUCKDB_TYPE_UTINYINT unimplemented");
                    break;
                case DUCKDB_TYPE_USMALLINT: // uint16_t
                    BOOST_FAIL("DUCKDB_TYPE_USMALLINT unimplemented");
                    break;
                case DUCKDB_TYPE_UINTEGER:  // uint32_t
                    BOOST_FAIL("DUCKDB_TYPE_UINTEGER unimplemented");
                    break;
                case DUCKDB_TYPE_UBIGINT:   // uint64_t
                    BOOST_FAIL("DUCKDB_TYPE_TINYINT unimplemented");
                    break;
                case DUCKDB_TYPE_BIGINT:    // int64_t
                    bidata = (int64_t*)duckdb_vector_get_data(colm);
                    sprintf(buf, "%I64d", bidata[row]);
                    printf(buf);
                    strvec.push_back(buf);
                    break;
                case DUCKDB_TYPE_DOUBLE:    // double
                    dbldata = (double*)duckdb_vector_get_data(colm);
                    sprintf(buf, "%f", dbldata[row]);
                    printf(buf);
                    strvec.push_back(buf);
                    break;
                case DUCKDB_TYPE_VARCHAR:   // duckdb_string_t
                    // https://duckdb.org/docs/stable/clients/c/vector#strings
                    vcdata = (duckdb_string_t*)duckdb_vector_get_data(colm);
                    if (duckdb_string_is_inlined(vcdata[row])) {
                        // if inlined is 12 chars, there will be no zero terminator
                        memcpy(buf, vcdata[row].value.inlined.inlined, vcdata[row].value.inlined.length);
                        buf[vcdata[row].value.inlined.length] = 0;
                        printf(buf);
                        strvec.push_back(buf);
                    }
                    else {
                        printf(vcdata[row].value.pointer.ptr);
                        strvec.push_back(vcdata[row].value.pointer.ptr);
                    }
                    break;
                case DUCKDB_TYPE_DECIMAL:
                    // https://duckdb.org/docs/stable/clients/c/vector#decimals
                    decimal_width = duckdb_decimal_width(type_l);
                    decimal_scale = duckdb_decimal_scale(type_l);
                    decimal_type = duckdb_decimal_internal_type(type_l);
                    decimal_divisor = pow(10, decimal_scale);

                    switch (decimal_type) {
                    case DUCKDB_TYPE_SMALLINT:  // int16_t
                        sidata = (int16_t*)duckdb_vector_get_data(colm);
                        decimal_value = (double)sidata[row] / decimal_divisor;
                        break;
                    case DUCKDB_TYPE_INTEGER:   // int32_t
                        idata = (int32_t*)duckdb_vector_get_data(colm);
                        decimal_value = (double)idata[row] / decimal_divisor;
                        break;
                    case DUCKDB_TYPE_BIGINT:    // int64_t
                        bidata = (int64_t*)duckdb_vector_get_data(colm);
                        decimal_value = (double)bidata[row] / decimal_divisor;
                        break;
                    case DUCKDB_TYPE_HUGEINT:   // duckdb_hugeint: 128
                        hidata = (duckdb_hugeint*)duckdb_vector_get_data(colm);
                        decimal_value = duckdb_hugeint_to_double(hidata[row]) / decimal_divisor;
                        break;
                    }
                    // decimal scale 2 means we want "%.2f"
                    // NB %% is the "escape sequence" for %
                    // in a printf format, not \%.
                    sprintf(fmtbuf, "%%.%df", decimal_scale);
                    sprintf(buf, fmtbuf, decimal_value);
                    printf(buf);
                    strvec.push_back(buf);
                    break;
                }
                printf("\t");   // column separator for readability
                buf[0] = 0;
            }
            printf("\n");
        }
    }
};

BOOST_FIXTURE_TEST_CASE(Varchar1, DuckDBFixture)
{
    duckdb_result result;
    std::vector<std::string> expected_varchars({"Alice","BobLongerThan12", "TwelveTwelve"});

    // Create a table and insert data
    duckdb_query(con, "CREATE TABLE my_table(id INTEGER, name VARCHAR);", NULL);
    duckdb_query(con, "INSERT INTO my_table VALUES (1, 'Alice'), (2, 'BobLongerThan12'), (3, 'TwelveTwelve'), (4, NULL);", NULL);

    auto rv = duckdb_query(con, "SELECT * FROM my_table;", &result);
    BOOST_TEST(rv == DuckDBSuccess);
    read_and_print_all_columns(result);
    // [1] is a map key from colm index: NB NULLs don't count as values
    BOOST_TEST(values[0].size() == 4);    // 4 non NULL in id col
    BOOST_TEST(values[1].size() == 3);    // 3 non NULL in name col
    BOOST_TEST(values[1] == expected_varchars);
    // Destroy the result after use
    duckdb_destroy_result(&result);
}

BOOST_FIXTURE_TEST_CASE(Summarize, DuckDBFixture)
{
    duckdb_result result;
    // sumarize varchar colms: name:0, type:1, min:2, max:3, avg:5, std:6, q25:7, q50:8, q75:9
    // other colms: apxu:4, cnt:10, null:11
    std::vector<idx_t> expected_colms({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
    std::vector<std::string> expected_null_counts({ "0.00","25.00" });  // col 11
    std::vector<std::string> expected_counts({ "4","4" });              // col 10

    // Create a table and insert data
    duckdb_query(con, "CREATE TABLE my_table(id INTEGER, name VARCHAR);", NULL);
    duckdb_query(con, "INSERT INTO my_table VALUES (1, 'Alice'), (2, 'BobLongerThan12'), (3, 'TwelveTwelve'), (4, NULL);", NULL);

    auto rv = duckdb_query(con, "summarize SELECT * FROM my_table;", &result);
    BOOST_TEST(rv == DuckDBSuccess);
    read_and_print_all_columns(result);
    // check we have 12 colms 0..11
    auto evc_iter = expected_colms.begin();
    for (; evc_iter != expected_colms.end(); ++evc_iter) {
        idx_t col_xpctd(*evc_iter);
        auto map_iter = values.find(col_xpctd);
        if (map_iter != values.end()) {
            idx_t col_found = map_iter->first;
            BOOST_TEST(col_found == col_xpctd);
        }
    }

    // column 11 is the NULL count as a 2 DP decimal
    BOOST_TEST(values[11] == expected_null_counts);
    // column 10 is count: should be 4 on both rows
    BOOST_TEST(values[10] == expected_counts);

    // Destroy the result after use
    duckdb_destroy_result(&result);
}
