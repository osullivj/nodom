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
    std::vector<std::string>    cache_strings;
    std::vector<int>            cache_ints;
    std::vector<float>          cache_floats;

    WidgetVec                   widget_vec;
    PushableMap                 pushables;

    // TODO: sort out std::pair key problem
    // ActionMap                   actions;

    // Valid cache addresses
    std::set<AddrInx>   addr_set;

    // interned str for cspec names eg cname, title, title_font
    std::map<CacheSpecifier, StrInx> cspec_indices;

    std::set<EntityInx> action_key_entity_indices;
    std::set<EventInx> action_key_event_indices;


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
    inline static std::array<CacheDataType, cs_end_cache_specs> atomic_cspec_types{
    cdStr,      // cs_title
    cdStr,      // cs_title_font
    cdInt,      // cs_title_font_size
    cdStr,      // cs_body_font
    cdInt,      // cs_body_font_size
    cdStr,      // cs_button_font
    cdInt,      // cs_button_font_size
    cdStr,      // cs_year_month_font
    cdInt,      // cs_year_month_font_size
    cdStr,      // cs_day_date_font
    cdInt,      // cs_day_date_font_size
    cdStr,      // cs_font
    cdInt,      // cs_font_size
    cdStr,      // cs_label
    cdStr,      // cs_text
    cdInt,      // cs_step,
    cdInt,      // cs_step_fast
    cdInt,      // cs_spinner_radius
    cdInt,      // cs_spinner_thickness
    cdInt,      // cs_flags
    cdInt,      // cs_table_flags
    cdInt,      // cs_combo_flags
    cdInt,      // cs_window_flags
    cdBool,     // cs_db,
    cdBool,     // cs_fps,
    cdBool,     // cs_demo,
    cdBool,     // cs_id_stack,
    cdFloat,    // cs_font_scale,
    cdInt,      // cs_style
    cdAny,      //.cs_cname
    cdInt,      // cs_index
    cdResultSet // cs_qname
    };

    inline static std::map<RenderMethod, CacheDataType> cname_cspec_types{
        {Combo, cdStrVec},
        {InputInt, cdInt},
        {Checkbox, cdBool},
        {DatePicker, cdIntVec},
        {LoadingModal, cdStrVec}
    };
    inline static std::map<RenderMethod, CacheDataType> index_cspec_types{
        {Combo, cdInt}
    };
    inline static  std::map<RenderMethod, CacheSpecVec> atomic_cspecs{
        {Home, {cs_title, cs_title_font, cs_title_font_size}},
        {InputInt, {cs_label, cs_step, cs_step_fast, cs_flags}},
        {Combo, {cs_label, cs_step}},
        {Checkbox, {cs_label}},
        {Text, {cs_text}},
        {Button, {cs_text}},
        {Table, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_table_flags, cs_window_flags}},
        {Footer, {cs_db, cs_fps, cs_demo, cs_id_stack, cs_font_scale, cs_style}},
        {DatePicker, {cs_year_month_font, cs_year_month_font_size,
                        cs_day_date_font, cs_day_date_font_size,
                        cs_table_flags, cs_combo_flags}},
        {DuckTableSummaryModal, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_table_flags, cs_window_flags,
                    cs_button_font, cs_button_font_size}},
        {LoadingModal, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_spinner_thickness, cs_spinner_radius,
                    cs_window_flags}},
        {PushFont, {cs_font, cs_font_size}}
    };
protected:
    template <CIT itype>
    auto intern_string(std::string&& s, CST stype = CST::None) {
        auto iter = std::find(cache_strings.begin(), cache_strings.end(), s);
        if (iter == cache_strings.end()) {
            cache_strings.emplace_back(std::move(s));
            fp_char_ptrs.push_back(cache_strings.back().c_str());
            return DataCacheIndex<itype,CDT::cdStr>(cache_strings.size() - 1, stype);
        }
        uint32_t inx = std::distance(iter, cache_strings.begin());
        return DataCacheIndex<itype,CDT::cdStr>(inx, stype);
    }

    template <CIT itype>
    auto intern_string(const std::string& s, CST stype = CST::None) {
        auto iter = std::find(cache_strings.begin(), cache_strings.end(), s);
        if (iter == cache_strings.end()) {
            cache_strings.push_back(s);
            fp_char_ptrs.push_back(cache_strings.back().c_str());
            return DataCacheIndex<itype, CDT::cdStr>(cache_strings.size() - 1, stype);
        }
        uint32_t inx = std::distance(iter, cache_strings.begin());
        return DataCacheIndex<itype, CDT::cdStr>(inx, stype);
    }

    auto intern_int(int&& value) {
        // cannot be a ptr to value in fp_int_ptrs as it's rval,
        // so go ahead and create a new cache_ints backed Int
        cache_ints.push_back(value);
        fp_int_ptrs.push_back(&(cache_ints.back()));
        return IntInx(fp_int_ptrs.size() - 1);
    }

    auto intern_int(int& value) {
        // is there a ptr to value in fp_int_ptrs already?
        // NB such a ptr would not have cache_ints backing...
        auto iter = std::find(fp_int_ptrs.begin(), fp_int_ptrs.end(), &value);
        if (iter == fp_int_ptrs.end()) {
            // we don't know where value is stored, so copy to cache_ints
            cache_ints.push_back(value);
            fp_int_ptrs.push_back(&(cache_ints.back()));
            return IntInx(fp_int_ptrs.size() - 1);
        }
        uint32_t inx = std::distance(iter, fp_int_ptrs.begin());
        return IntInx(inx);
    }

    auto intern_float(float&& value) {
        // cannot be a ptr to value in fp_int_ptrs as it's rval,
        // so go ahead and create a new cache_ints backed Int
        cache_floats.push_back(value);
        fp_float_ptrs.push_back(&(cache_floats.back()));
        return FloatInx(fp_float_ptrs.size() - 1);
    }

    auto intern_float(float& value) {
        // is there a ptr to value in fp_float_ptrs already?
        // NB such a ptr would not have cache_ints backing...
        auto iter = std::find(fp_float_ptrs.begin(), fp_float_ptrs.end(), &value);
        if (iter == fp_float_ptrs.end()) {
            // we don't know where value is stored, so copy to cache_ints
            cache_floats.push_back(value);
            fp_float_ptrs.push_back(&(cache_floats.back()));
            return FloatInx(fp_float_ptrs.size() - 1);
        }
        uint32_t inx = std::distance(iter, fp_float_ptrs.begin());
        return FloatInx(inx);
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
    size_t widget_vec_size() { return widget_vec.size(); }
    size_t pushables_size() { return pushables.size(); }

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
            const JSON& jactions(data[Static::actions_cs]);
            StringVec action_keys;
            JKeys(jactions, action_keys);
            for (auto citer = action_keys.cbegin(); citer != action_keys.cend(); ++citer) {
                std::stringstream ss{*citer};
                std::string entity;
                if (std::getline(ss, entity, Static::period_c)) {
                    EntityInx entity_inx = intern_string<CIT::EntityID>(std::move(entity));
                    std::string event;
                    if (std::getline(ss, event, Static::period_c)) {
                        EventInx event_inx = intern_string<CIT::Event>(std::move(event));
                        // combine two inx...
                        NDLogger::cout() << method << entity_inx << ":" << event_inx << std::endl;
                    }
                    else {
                        NDLogger::cout() << method << "DATA_BAD_ACTION_KEY: "
                            << *citer << std::endl;
                    }
                }
                else {
                    NDLogger::cout() << method << "DATA_BAD_ACTION_KEY: " 
                        << *citer << std::endl;
                }
            }
        }
        else {
            NDLogger::cout() << method << "DATA_NO_ACTIONS" << std::endl;
        }

    }

    void orthogonalize_cspec(const JSON& cspec, WidgetPtr widget) {
        if (!widget->widget_id.empty()) {
            widget->widget_inx = intern_string<EntityID>(widget->widget_id, CST::WidgetID);
        }
        // parse cspec: atomics first
        const CacheSpecVec& atomics{ atomic_cspecs[widget->rname] };
        for (auto cit = atomics.cbegin(); cit != atomics.cend(); ++cit) {
            CacheSpecifier spec{ *cit };
            CacheDataType atomic_type = atomic_cspec_types[spec];
            const char* atomic_name = atomic_cspec_names[spec];
            if (JContains(cspec, atomic_name)) {
                switch (atomic_type) {
                case cdInt:
                    widget->cspec_int[spec] = intern_int(JAsInt(cspec, atomic_name));
                    break;
                case cdFloat:
                    widget->cspec_float[spec] = intern_float(JAsFloat(cspec, atomic_name));
                    break;
                case cdBool:
                    widget->cspec_int[spec] = intern_int(JAsInt(cspec, atomic_name));
                    break;
                case cdStr:
                    widget->cspec_str[spec] = intern_string<Value>(JAsString(cspec, atomic_name));
                    break;
                case cdIntVec:
                    // TODO
                    assert(false);
                    break;
                case cdStrVec:
                    assert(false);
                    break;
                }
            }
        }
    }

    void on_layout(const JSON& layout, WidgetVec* wv = nullptr) {
        // If we're not recursing, widgets go in top level vec...
        WidgetVec* wvec = (wv == nullptr) ? &widget_vec : wv;
        // NB layout as a whole is a list of widgets.
        // And so is children, hence the recursion...
        int layout_length = JSize(layout);
        for (int inx = 0; inx < layout_length; inx++) {
            const JSON& w(layout[inx]);
            const JSON& cspec(extract_cspec<JSON>(w));
            // The only NDWidget ctor invocation...
            auto wptr = std::make_shared<NDWidget>(extract_render_name(w), 
                                    extract_string(w, Static::widget_id_cs));
            wvec->push_back(wptr);
            orthogonalize_cspec(cspec, wvec->back());
            const JSON& children = extract_children(w);
            int child_count = JSize(children);
            if (child_count > 0) {
                on_layout(children, &(wptr->children));
            }
        }
        // If we've finished recursing, build the PushableMap
        if (wv != nullptr) return;

        for (auto citer = widget_vec.cbegin(); citer != widget_vec.cend(); ++citer) {
            bool valid_inx = (*citer)->widget_inx.ok();
            if (valid_inx) {
                pushables[(*citer)->widget_inx] = *citer;
            }
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

    IntInx add_stored_int(int& si) {

    }


   
};
