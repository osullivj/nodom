#include <duckdb.h>
#define BOOST_TEST_MODULE Varchar_Tests
#include <boost/test/unit_test.hpp>

struct DuckDBFixture {
    duckdb_database db;
    duckdb_connection con;

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

        int32_t* idata = nullptr;
        int64_t* bidata = nullptr;
        uint64_t* colm_validity = nullptr;
        char buf[32];

        for (idx_t row = 0; row < row_count; row++) {
            for (idx_t col = 0; col < colm_count; col++) {
                duckdb_vector colm = duckdb_data_chunk_get_vector(res_chunk, col);
                colm_validity = duckdb_vector_get_validity(colm);
                duckdb_logical_type type_l = duckdb_vector_get_column_type(colm);
                duckdb_type type = duckdb_get_type_id(type_l);
                if (!duckdb_validity_row_is_valid(colm_validity, row)) {
                    printf("INVLD\t");
                    continue;
                }
                switch (type) {
                    // 32 bit ints
                case DUCKDB_TYPE_TINYINT:
                case DUCKDB_TYPE_SMALLINT:
                case DUCKDB_TYPE_INTEGER:
                    idata = (int32_t*)duckdb_vector_get_data(colm);
                    sprintf(buf, "%I32d\t", idata[row]);
                    break;

                case DUCKDB_TYPE_UTINYINT:
                case DUCKDB_TYPE_USMALLINT:
                case DUCKDB_TYPE_UINTEGER:
                    // 64 bit ints
                case DUCKDB_TYPE_UBIGINT:
                    break;
                case DUCKDB_TYPE_BIGINT:
                    bidata = (int64_t*)duckdb_vector_get_data(colm);
                    sprintf(buf, "%I64d\t", bidata[row]);
                    break;
                case DUCKDB_TYPE_VARCHAR:
                    printf("VCHR\t");
                    break;
                }
                printf(buf);
                buf[0] = 0;
            }
            printf("\n");
        }
    }
};

BOOST_FIXTURE_TEST_CASE(Varchar1, DuckDBFixture)
{
    duckdb_result result;

    // Create a table and insert data
    duckdb_query(con, "CREATE TABLE my_table(id INTEGER, name VARCHAR);", NULL);
    duckdb_query(con, "INSERT INTO my_table VALUES (1, 'Alice'), (2, 'BobLongerThan12'), (3, NULL);", NULL);

    auto rv = duckdb_query(con, "SELECT * FROM my_table;", &result);
    BOOST_TEST(rv == DuckDBSuccess);
    read_and_print_all_columns(result);

    // Destroy the result after use
    duckdb_destroy_result(&result);
}

BOOST_AUTO_TEST_CASE(Foo2)
{
    BOOST_CHECK(true);
}
