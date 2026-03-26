#pragma once
#include <utility>
#include "json_ops.hpp"
#include "dl_types.hpp"
#include "logger.hpp"

// Cache for data, layout and data.actions
template <typename JSON>
class DataLayCache {
protected:
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
    ActionMap                   actions;
    ActionInternMap             actions_interned;
    ActionErrorMap              actions_errors;
    StringVec                   bad_action_keys;

    // Valid cache addresses
    std::map<std::string, AddrInx>   address_map;

    // interned str for cspec names eg cname, title, title_font
    std::map<CacheSpecifier, StrInx> cspec_indices;

    std::set<EntityInx> action_key_entity_indices;
    std::set<EventInx> action_key_event_indices;


public:

    template <CIT itype>
    auto intern_string(const std::string& s, CST stype = CST::None) {
        auto iter = std::find(cache_strings.begin(), cache_strings.end(), s);
        if (iter == cache_strings.end()) {
            cache_strings.push_back(s);
            const char* ptr = cache_strings.back().c_str();
            fp_char_ptrs.push_back(ptr);
            return DataCacheIndex<itype, CDT::cdStr>(cache_strings.size() - 1, stype);
        }
        uint32_t inx = std::distance(cache_strings.begin(), iter);
        return DataCacheIndex<itype, CDT::cdStr>(inx, stype);
    }

    auto intern_int(int value) {
        // create storage for an int value, and FP ptr too
        cache_ints.push_back(value);
        fp_int_ptrs.push_back(&(cache_ints.back()));
        return IntInx(fp_int_ptrs.size() - 1);
    }

    auto intern_float(float value) {
        // cannot be a ptr to value in fp_int_ptrs as it's rval,
        // so go ahead and create new cache_floats backed storage
        cache_floats.push_back(value);
        fp_float_ptrs.push_back(&(cache_floats.back()));
        return FloatInx(fp_float_ptrs.size() - 1);
    }

    DataLayCache(int intern_capacity=256) {
        fp_float_ptrs.reserve(intern_capacity);
        fp_int_ptrs.reserve(intern_capacity);
        fp_char_ptrs.reserve(intern_capacity);
        cache_strings.reserve(intern_capacity);
        cache_ints.reserve(intern_capacity);
        cache_floats.reserve(intern_capacity);
    }

    void init() {
        const static char* method = "DataLayCache::init: ";
    }

    size_t addr_map_size() { return address_map.size(); }
    size_t widget_vec_size() { return widget_vec.size(); }
    size_t pushables_size() { return pushables.size(); }
    size_t actions_size() { return actions.size(); }



    void parse_actions(const JSON& action_seq, ActionVec& nd_action_vec, ActionInternVec& act_intern_vec, ActionErrorVec& act_error_vec) {
        const static char* method = "DataLayCache::parse_actions: ";
        int action_seq_len = JSize(action_seq);
        for (int inx = 0; inx < action_seq_len; inx++) {
            const JSON& action_defn(action_seq[inx]);
            NDAction action;
            NDActionInterned interned;
            NDActionErrors errors;
            if (JContains(action_defn, Static::ui_pop_cs)) {
                // a ui_pop must be a RenderMethod
                std::string rname = JAsString(action_defn, Static::ui_pop_cs);
                action.pop_ui = RenderMethodFromString(rname);
                if (action.pop_ui == EndRenderMethod) {
                    std::stringstream ss{ "BAD_RNAME(" };
                    ss << rname << ")";
                    errors.error_vec.push_back(ss.str());
                    errors.inx = inx;
                }
                else {
                    interned.pop_ui = (char*)render_names[action.pop_ui];
                }
            }
            if (JContains(action_defn, Static::ui_push_cs)) {
                // ui_push must be a widget_id. Since action parsing happens before
                // layout, this is the first time we may encounter encounter a
                // widget_id. NB there may be widget_ids in layout that aren't 
                // referemced in actions.
                std::string widget_id = JAsString(action_defn, Static::ui_push_cs);
                action.push_ui = intern_string<EntityID>(widget_id, CST::WidgetID);
                interned.push_ui = (char*)get_string_value(action.push_ui);
            }
            if (JContains(action_defn, Static::db_action_cs)) {
                // Command, Query & BatchRequest DB actions all require query_id
                // Command & Query need sql_cname too
                std::string db_action = JAsString(action_defn, Static::db_action_cs);
                action.db_action = DBEventTypeFromString(db_action);
                interned.db_action = (char*)db_event_types[action.db_action];
                std::string query_id = JAsString(action_defn, Static::query_id_cs);
                action.query_id = intern_string<EntityID>(query_id, CST::QueryID);
                interned.query_id = (char*)get_string_value(action.query_id);
                if (action.db_action == dbCommand || action.db_action == dbQuery) {
                    std::string sql_cache_key = JAsString(action_defn, Static::sql_cname_cs);
                    action.sql_cname = intern_string<Address>(sql_cache_key);
                    // check that the sql cname refers to a cache data entry
                    interned.sql_cname = (char*)get_addr_value(action.sql_cname);
                    if (address_map.find(sql_cache_key) == address_map.end()) {
                        std::stringstream ss{ "CACHE_KEY_NOT_FOUND(" };
                        ss << sql_cache_key << ")";
                        errors.error_vec.push_back(ss.str());
                        errors.inx = inx;
                    }
                }
            }
            if (errors.inx == -1) {
                nd_action_vec.push_back(action);
                act_intern_vec.push_back(interned);
            }
            else {
                act_error_vec.push_back(errors);
            }
        }
    }

    void print_parsed_action(const NDAction& action, const NDActionInterned& interned) {
        // TODO: debuggable output from action parsing
        bool prefix_comma = false;
        NDLogger::cout() << "Action[";
        if (action.push_ui.is_valid()) {
            NDLogger::cout() << "push_ui(" << action.push_ui << "/" << interned.push_ui << ")";
            prefix_comma = true;
        }
        if (is_render_valid(action.pop_ui)) {
            if (prefix_comma) NDLogger::cout() << ", ";
            NDLogger::cout() << "pop_ui(" << action.pop_ui << "/" << interned.pop_ui << ")";
            prefix_comma = true;
        }
        if (is_db_event_valid(action.db_action)) {
            if (prefix_comma) NDLogger::cout() << ", ";
            NDLogger::cout() << "db_action(" << action.db_action << "/" << interned.db_action << ")";
            prefix_comma = true;
        }
        if (action.query_id.is_valid()) {
            if (prefix_comma) NDLogger::cout() << ", ";
            NDLogger::cout() << "query_id(" << action.query_id << "/" << interned.query_id << ")";
            prefix_comma = true;
        }
        if (action.sql_cname.is_valid()) {
            if (prefix_comma) NDLogger::cout() << ", ";
            NDLogger::cout() << "sql_cname(" << action.sql_cname << "/" << interned.sql_cname << ")";
        }
        NDLogger::cout() << "]";
    }

    void on_data(const JSON& data) {
        const static char* method = "DataLayCache::on_data: ";

        bool we_have_actions{ false };
        StringVec data_keys;
        JKeys(data, data_keys);
        for (auto cit = data_keys.cbegin(); cit != data_keys.end(); ++cit) {
            std::string key{ *cit };
            if (key == Static::actions_cs) {
                we_have_actions = true;
                continue;
            }
            AddrInx ainx = add_address(key);
            address_map[key] = ainx;
            // We could try and parse the values here, but how do
            // we tell an int from a float? We cannot, because JS only
            // has "number". We can figure atomic vs array, and string vs
            // number, but not float vs int. However, when we parse
            // cspecs in on_layout, we can know from context what type
            // is required.
        }

        if (we_have_actions) {
            const JSON& jactions(data[Static::actions_cs]);
            StringVec action_key_vec;
            JKeys(jactions, action_key_vec);
            for (auto cak_iter = action_key_vec.cbegin(); cak_iter != action_key_vec.cend(); ++cak_iter) {
                std::string action_key_s{ *cak_iter };
                std::stringstream ss{action_key_s};
                std::string entity;
                if (!std::getline(ss, entity, Static::period_c)) {
                    bad_action_keys.push_back(action_key_s);
                    continue;
                }
                // The EntityInx and EventInx created here are only used
                // to compose the ActionKey, and are then discarded. Their
                // values will be recreated on layout parsing.
                EntityInx entity_inx = intern_string<CIT::EntityID>(entity);
                std::string event;
                if (!std::getline(ss, event, Static::period_c)) {
                    bad_action_keys.push_back(action_key_s);
                    continue;
                }
                EventInx event_inx = intern_string<CIT::Event>(event);
                // combine two inx...
                ActionKey action_key{ entity_inx, event_inx };
                ActionVec nd_action_vec{};
                ActionInternVec action_intern_vec{};
                ActionErrorVec action_error_vec{};
                JSON action_seq = JSON::array();
                action_seq = jactions[action_key_s];
                parse_actions(action_seq, nd_action_vec, action_intern_vec, action_error_vec);
                actions[action_key] = nd_action_vec;
                actions_interned[action_key] = action_intern_vec;
                actions_errors[action_key] = action_error_vec;
            }
        }
    }

    void orthogonalize_cspec(const JSON& cspec, const JSON& data, WidgetPtr widget) {
        if (!widget->widget_id.empty()) {
            widget->widget_inx = intern_string<EntityID>(widget->widget_id, CST::WidgetID);
        }
        // TODO: add exception handling to catch type mismatches...
        // 
        // parse cspec: value_cspecs first, that is fields that just
        // give a value, rather than an address
        const CacheSpecVec& values{ value_cspecs[widget->rname] };
        for (auto cit = values.cbegin(); cit != values.cend(); ++cit) {
            CacheSpecifier spec{ *cit };
            CacheDataType value_type = cspec_types[spec];
            const char* value_name = cspec_names[spec];
            if (JContains(cspec, value_name)) {
                switch (value_type) {
                case cdInt:
                    widget->cspec_int[spec] = intern_int(JAsInt(cspec, value_name));
                    break;
                case cdFloat:
                    widget->cspec_float[spec] = intern_float(JAsFloat(cspec, value_name));
                    break;
                case cdBool:
                    widget->cspec_int[spec] = intern_int(JAsBool(cspec, value_name)?1:0);
                    break;
                case cdStr:
                    widget->cspec_str[spec] = intern_string<Value>(JAsString(cspec, value_name));
                    break;
                case cdIntVec:
                case cdStrVec:
                case cdAny:
                case cdResultSet:
                    assert(false);
                    break;
                }
            }
        }
        // now we'll handle the address cspecs that ref data keys
        const CacheSpecTypeMap& cs_type_map{ addr_cspecs[widget->rname] };
        for (auto ctmit = cs_type_map.cbegin(); ctmit != cs_type_map.cend(); ++ctmit) {
            const CacheSpecifier spec{ ctmit->first };
            CacheDataType ref_type = cspec_types[spec];
            std::string ref_name = cspec_names[spec];   // cindex or cname
            if (JContains(cspec, ref_name.c_str())) {
                // addr should already be interned by on_data()
                std::string addr_s{JAsString(cspec, ref_name)};
                auto amit = address_map.find(addr_s);
                if (amit == address_map.end()) {
                    // error: cname/cindex value not in data
                }
                else {
                    DataRef data_ref{ ref_type, amit->second };
                    StringVec svec;
                    IntVec ivec;
                    JSON jvec{ JSON::array() };
                    // AddrInx ainx{ amit->second };
                    switch (ref_type) {
                    case cdAny:         // shouldn't be in addr_cspecs!
                    case cdResultSet:
                        assert(false);
                        break;
                    case cdInt: // spec:cindex, sz:1
                        data_ref.ref_inx = intern_int(JAsInt(data, ref_name))();
                        break;
                    case cdFloat:
                        assert(false);
                        break;
                    case cdBool:
                        data_ref.ref_inx = intern_int(JAsInt(data, ref_name))();
                        break;
                    case cdStr:
                    case cdIntVec:
                        jvec = data[addr_s];
                        data_ref.size = JSize(jvec);
                        if (data_ref.size > 0) {
                            // capture the "base" index; subsequent indices
                            // are implied by data_ref.size
                            data_ref.ref_inx = intern_int(jvec[0])();
                            for (int jinx = 1; jinx < data_ref.size; jinx++)
                                intern_int(jvec[jinx]);
                        }
                    case cdStrVec:
                        JAsStringVec(data, ref_name.c_str(), svec);
                        data_ref.size = svec.size();
                        if (data_ref.size > 0) {
                            auto it = svec.begin();
                            data_ref.ref_inx = intern_string<CIT::Value>(*it++)();
                            while (it != svec.end())
                                intern_string<CIT::Value>(*it++);
                        }
                        break;
                    }
                    widget->data_refs[spec] = data_ref;
                }
            }
        }
    }

    void on_layout(const JSON& layout, const JSON& data, WidgetVec* wv = nullptr) {
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
            orthogonalize_cspec(cspec, data, wvec->back());
            const JSON& children = extract_children(w);
            int child_count = JSize(children);
            if (child_count > 0) {
                on_layout(children, data, &(wptr->children));
            }
        }
        // If we've finished recursing, build the PushableMap
        if (wv != nullptr) return;

        for (auto citer = widget_vec.cbegin(); citer != widget_vec.cend(); ++citer) {
            bool valid_inx = (*citer)->widget_inx.is_valid();
            if (valid_inx) {
                pushables[(*citer)->widget_inx] = *citer;
            }
        }
    }

    void on_json(const JSON& data, const JSON& layout) {
        on_data(data);
    }

    const char* get_string_value(EntityInx inx) {
        return fp_char_ptrs[inx()];
    }

    const char* get_addr_value(AddrInx inx) {
        return fp_char_ptrs[inx()];
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


private:
    // statics that define DataLayCache data and layout geometry
    inline static std::array<const char*, EndRenderMethod> render_names{
        Static::rm_home_cs,
        Static::rm_input_int_cs,
        Static::rm_combo_cs,
        Static::rm_checkbox_cs,
        Static::rm_text_cs,
        Static::rm_button_cs,
        Static::rm_table_cs,
        Static::rm_footer_cs,
        Static::rm_date_picker_cs,
        Static::rm_duck_table_summary_modal_cs,
        Static::rm_loading_modal_cs,
        Static::rm_separator_cs,
        Static::rm_same_line_cs,
        Static::rm_new_line_cs,
        Static::rm_spacing_cs,
        Static::rm_align_text_to_frame_padding_cs,
        Static::rm_begin_child_cs,
        Static::rm_end_child_cs,
        Static::rm_begin_group_cs,
        Static::rm_end_group_cs,
        Static::rm_push_font_cs,
        Static::rm_pop_font_cs
    };

    inline static std::array<const char*, EndDBEventTypes> db_event_types{
        Static::command_cs,
        Static::command_result_cs,
        Static::query_cs,
        Static::query_result_cs,
        Static::batch_request_cs,
        Static::batch_response_cs
    };

    inline static std::array<const char*, cs_end_cache_specs> cspec_names{
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
        Static::cindex_cs,
        Static::qname_cs
    };

    inline static std::array<CacheDataType, cs_end_cache_specs> cspec_types{
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
        cdBool,     // cs_db            footer flag
        cdBool,     // cs_fps           footer flag
        cdBool,     // cs_demo          footer flag
        cdBool,     // cs_id_stack      footer flag
        cdBool,     // cs_font_scale    footer flag
        cdBool,     // cs_style         footer flag
        cdAny,      //.cs_cname
        cdAny,      // cs_cindex
        cdResultSet // cs_qname
    };

    inline static std::map<RenderMethod, CacheDataType> cname_cspec_types{
        {Combo, cdStrVec},
        {InputInt, cdInt},
        {Checkbox, cdBool},
        {DatePicker, cdIntVec},
        {LoadingModal, cdStrVec}
    };

    inline static  std::map<RenderMethod, CacheSpecVec> value_cspecs{
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

    inline static std::map<RenderMethod, CacheSpecTypeMap> addr_cspecs{
        {InputInt, {{cs_cname, cdInt}}},
        {Combo, {{cs_cindex, cdInt}, {cs_cname, cdStrVec}}},
        {Checkbox, {{cs_cname, cdBool}}},
        {DatePicker, {{cs_cname, cdIntVec}}},
        {LoadingModal, {{cs_cname, cdStrVec}}}
    };
};
