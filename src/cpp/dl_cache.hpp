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

    // interned str for eg cname, title, title_font
    std::map<CacheSpecifier, StrInx> cspec_indices;

    inline static std::array<const char*, cs_end_cache_specs> atomic_cspec_names{
        Static::title_cs,      // cs_title
        Static::title_font_cs,      // cs_title_font
        Static::title_font_size_cs,      // cs_title_font_size
        Static::body_font_cs,      // cs_body_font
        Static::body_font_size_cs,      // cs_body_font_size
        Static::button_font_cs,      // cs_button_font
        Static::button_font_size_cs,      // cs_button_font_size
        Static::year_month_font_cs,      // cs_year_month_font
        Static::year_month_font_size_cs,      // cs_year_month_font_size
        Static::day_date_font_cs,      // cs_day_date_font
        Static::day_date_font_size_cs,      // cs_day_date_font_size
        Static::font_cs,                    // cs_font
        Static::font_size_cs,               // cs_font_size
        Static::label_cs,      // cs_label
        Static::text_cs,      // cs_text
        Static::step_cs,      // cs_step,
        Static::step_fast_cs,      // cs_step_fast
        Static::spinner_radius_cs,      // cs_spinner_radius
        Static::spinner_thickness_cs,      // cs_spinner_thickness
        Static::flags_cs,      // cs_flags
        Static::table_flags_cs,      // cs_table_flags
        Static::combo_flags_cs,      // cs_combo_flags
        Static::window_flags_cs,      // cs_window_flags
        Static::db_cs,     // cs_db,
        Static::fps_cs,     // cs_fps,
        Static::demo_cs,     // cs_demo,
        Static::id_stack_cs,     // cs_id_stack,
        Static::font_scale_cs,    // cs_font_scale,
        Static::style_cs,        // cs_style
        Static::cname_cs,
        Static::index_cs,
        Static::qname_cs
    };
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

    void init() {
        const static char* method = "DataLayCache::init: ";

        // intern the cache spec names so each has a str val DCI
        for (uint32_t spec_inx{ cs_title }; spec_inx < cs_end_cache_specs; ++spec_inx) {
            StrInx sinx = intern_string<Value>(atomic_cspec_names[spec_inx]);
            NDLogger::cout() << method << atomic_cspec_names[spec_inx] << ":" << sinx << std::endl;
            cspec_indices.emplace(std::make_pair<CacheSpecifier, StrInx>(CacheSpecifier{ spec_inx }, std::move(sinx)));
        }
    }

    size_t addr_set_size() { return addr_set.size(); }

    void on_data(const JSON& data) {
        const static char* method = "DataLayCache::on_data: ";

        bool we_have_actions{ false };
        StringVec data_keys;
        std::string key;
        JKeys(data, data_keys);
        for (auto cit = data_keys.cbegin(); cit != data_keys.end(); ++cit) {
            key = *cit;
            if (key == Static::actions_cs) {
                we_have_actions = true;
                continue;
            }
            AddrInx ainx = add_address(key);
            addr_set.insert(ainx);
            NDLogger::cout() << method << key << ":" << ainx << std::endl;
            // We could try and parse the values here, but how do
            // we tell an int from a float? We cannot, because JS only
            // has "number". We can figure atomic vs array, and string vs
            // number, but not float vs int. However, when we parse
            // cspecs in on_layout, we can know from context what type
            // is required.
        }

        if (we_have_actions) {
            const JSON& actions(data[Static::actions_cs]);

        }
        else {
            NDLogger::cout() << method << "DATA_NO_ACTIONS" << std::endl;
        }

    }

    void on_layout(const JSON& layout) {
        int layout_length = JSize(layout);
        for (int inx = 0; inx < layout_length; inx++) {
            const JSON& w(layout[inx]);
        }
    }

    const char* get_string_value(AddrInx inx) {
        return fp_char_ptrs[inx()];
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


   
};
