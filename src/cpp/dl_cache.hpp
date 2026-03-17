#pragma once
#include "nd_types.hpp"
#include "dl_types.hpp"
#include "logger.hpp"

// Cache for data, layout and data.actions
template <typename JSON>
class DataLayCache {
private:
    // RHS data values: you can only be RHS value if
    // you have an LHS CacheName. All these vecs hold
    // ptrs to mem managed by someone else...
    std::vector<float*>         fp_float_ptrs;
    std::vector<int*>           fp_int_ptrs;
    std::vector<const char*>    fp_char_ptrs;

    // an LHS CacheName is an Address. All JSON sourced
    // strings from data and layout are moved into here,
    // whether LHS or RHS. Other strings, eg static_strings
    // may have a value stored elsewhere, but will also 
    // have a copy here for simplicity, so that cache_strings
    // and fp_char_ptrs can stay in a one to one.
    std::vector<std::string>   cache_strings;

    // Cache addresses
    std::set<AddrInx>   addr_set;
protected:
    template <CIT itype>
    auto intern_string(std::string&& s) {
        auto iter = std::find(cache_strings.begin(), cache_strings.end(), s);
        if (iter == cache_strings.end()) {
            cache_strings.emplace_back(std::move(s));
            fp_char_ptrs.push_back(cache_strings.back().c_str());
            return DataCacheIndex<itype,CDT::cdStr>(cache_strings.size() - 1);
        }
        uint32_t inx = std::distance(iter, cache_strings.begin());
        return DataCacheIndex<itype,CDT::cdStr>(inx);
    }

    template <CIT itype>
    auto intern_string(const std::string& s) {
        auto iter = std::find(cache_strings.begin(), cache_strings.end(), s);
        if (iter == cache_strings.end()) {
            cache_strings.push_back(s);
            fp_char_ptrs.push_back(cache_strings.back().c_str());
            return DataCacheIndex<itype, CDT::cdStr>(cache_strings.size() - 1);
        }
        uint32_t inx = std::distance(iter, cache_strings.begin());
        return DataCacheIndex<itype, CDT::cdStr>(inx);
    }

public:
    DataLayCache() { }

    void on_data(const JSON& data) {
        const static char* method = "DataLayCache::on_data: ";

        bool we_have_actions{ false };
        StringVec data_keys;
        std::string key;
        JKeys(data, data_keys);
        for (auto cit = data_keys.cbegin(); cit != data_keys.end(); ++cit) {
            key = *cit;
            if (key == Static::actions_cs) continue;
            AddrInx ainx = add_address(key);
            addr_set.insert(ainx);
            NDLogger::cout() << method << key << ":" << ainx << std::endl;
        }

        if (JContains(data, Static::actions_cs)) {
            const JSON& actions(data[Static::actions_cs]);

        }
        else {
            NDLogger::cout() << method << "DATA_NO_ACTIONS" << std::endl;
        }

    }

    void on_layout(const JSON& layout) {

    }

    AddrInx add_address(std::string&& addr) {
        return intern_string<CIT::Address>(std::move(addr));
    }

    AddrInx add_address(const std::string& addr) {
        return intern_string<CIT::Address>(addr);
    }

    IntInx add_int(int* v) {
        fp_int_ptrs.push_back(v);
        return IntInx(fp_int_ptrs.size() - 1);
    }

    FloatInx add_float(float* v) {
        fp_float_ptrs.push_back(v);
        return FloatInx(fp_float_ptrs.size() - 1);
    }

    StrInx add_string(const std::string& s) {
        return intern_string<CIT::Value>(s);
    }

    StrInx add_string(std::string&& s) {
        return intern_string<CIT::Value>(std::move(s));
    }

    RenderInx add_render_name(const std::string& rname) {
        return intern_string<CIT::RenderName>(rname);
    }

    const char* get_string_value(AddrInx inx) {
        return fp_char_ptrs[inx()];
    }
   
};
