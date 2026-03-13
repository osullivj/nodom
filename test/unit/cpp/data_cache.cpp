#include "data_cache.hpp"
#define BOOST_TEST_MODULE Data_Cache_Tests
#include <boost/test/unit_test.hpp>
#include <math.h>

struct DataCacheFixture {
    DataCache dc;

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
