#pragma once
#include <utility>
#include "json_ops.hpp"
#include "dl_types.hpp"
#include "logger.hpp"

// Cache for data, layout and data.actions
template <typename JSON>
class DataLayCache {
protected:
    static int inline constexpr CCAP = 256;
    // RHS data values: you can only be RHS value if
    // you have an LHS CacheName. All these vecs hold
    // ptrs to mem managed by someone else...
    std::vector<float*>         fp_float_ptrs;
    std::vector<int*>           fp_int_ptrs;
    std::vector<const char*>    fp_char_ptrs;
    std::vector<bool*>          fp_bool_ptrs;

    // an LHS CacheName is an Address. All JSON sourced
    // strings from data and layout are moved into here,
    // whether LHS or RHS. Other strings, eg static_strings
    // may have a value stored elsewhere, but will also 
    // have a copy here for simplicity, so that cache_strings
    // and fp_char_ptrs can stay in a one to one.
    std::vector<std::string>    cache_strings;
    std::vector<int>            cache_ints;
    std::vector<float>          cache_floats;
    std::array<bool, CCAP>      cache_bools;

    WidgetVec                   widget_vec;
    PushableMap                 pushables;
    WidgetPtr                   noop_widget;

    ActionMap                   action_map;
    ActionInternMap             action_interned_map;
    ActionErrorMap              action_error_map;
    StringVec                   bad_action_keys;
    StringVec                   action_errors;

    // Valid cache addresses
    std::map<std::string, AddrInx>      address_map;
    std::map<AddrInx, DataRef>          data_ref_map;

    StringVec                           bad_addrs;
    StringVec                           bad_data_refs;
    StringVec                           layout_errors;

    // EntityIDs created as QueryIDs.
    std::map<std::string, EntityInx>    query_map;
    std::map<std::string, EntityInx>    widget_map;
    EntityInx                           invalid_entity;

    // Two reserved keys in data: actions and menus
    StringSet                           special_keys{
                                            Static::actions_cs, 
                                            Static::menus_cs,
                                            Static::functions_cs
    };

    // Top level menu_bar and individual menu_items are just data
    // But menus must have contiguous elements, just like menubars.
    // And we must be able to map from menu name to menu StrVec; hence menu_map
    std::map<std::string, EntityInx>    menu_entity_map;
    std::map<EntityInx, DataRef>        menu_data_ref_map;

    std::map<std::string, EntityInx>    js_func_entity_map;
    std::map<EntityInx, uint32_t>       js_func_inx_map;
    StringVec                           js_func_vec;

public:
    template <CIT itype>
    auto get_string_index(const std::string& s, CST stype = CST::None) {
        auto iter = std::find(cache_strings.begin(), cache_strings.end(), s);
        if (iter == cache_strings.end()) {
            cache_strings.push_back(s);
            const char* ptr = cache_strings.back().c_str();
            fp_char_ptrs.push_back(ptr);
            return DataCacheIndex<itype, CDT::cdStr>((uint32_t)cache_strings.size() - 1, stype);
        }
        uint32_t inx = (uint32_t)std::distance(cache_strings.begin(), iter);
        return DataCacheIndex<itype, CDT::cdStr>(inx, stype);
    }


    template <CIT itype>
    auto contiguous_string_index(const std::string& s, CST stype = CST::None) {
        cache_strings.push_back(s);
        const char* ptr = cache_strings.back().c_str();
        fp_char_ptrs.push_back(ptr);
        return DataCacheIndex<itype, CDT::cdStr>((uint32_t)cache_strings.size() - 1, stype);
    }

protected:
    void update_string(uint32_t inx, const std::string& val) {
        if (inx >= cache_strings.size())
            throw std::runtime_error("NoDOM BAD_ADDR:update_string:"
                + std::to_string(inx) + ":" + val);
        cache_strings[inx] = val;
        fp_char_ptrs[inx] = cache_strings[inx].c_str();
    }

    IntInx get_int_index(int value) {
        // create storage for an int value, and FP ptr too
        cache_ints.push_back(value);
        fp_int_ptrs.push_back(&(cache_ints.back()));
        return IntInx((uint32_t)fp_int_ptrs.size() - 1);
    }

    void update_int(uint32_t inx, int val) {
        // NB cached int may have been interned by get_int_index(),
        // or it may be an extern_int. Both cases should work here.
        if (inx >= fp_int_ptrs.size())
            throw std::runtime_error("NoDOM BAD_ADDR:update_int");
        *(fp_int_ptrs[inx]) = val;
        cache_ints[inx] = val;
    }

    BoolInx get_bool_index(bool val) {
        BoolInx binx{ (uint32_t)fp_bool_ptrs.size() };
        cache_bools[binx()] = val;
        fp_bool_ptrs.push_back(&(cache_bools[binx()]));
        return binx;
    }

    void update_bool(uint32_t inx, bool val) {
        if (inx >= fp_bool_ptrs.size())
            throw std::runtime_error("NoDOM BAD_ADDR:update_bool");
        *(fp_bool_ptrs[inx]) = val ? 1 : 0;
        cache_bools[inx] = val ? 1 : 0;
    }

    FloatInx get_float_index(float value) {
        // cannot be a ptr to value in fp_int_ptrs as it's rval,
        // so go ahead and create new cache_floats backed storage
        cache_floats.push_back(value);
        fp_float_ptrs.push_back(&(cache_floats.back()));
        return FloatInx((uint32_t)fp_float_ptrs.size() - 1);
    }

    void clear() {
        fp_float_ptrs.clear();
        fp_int_ptrs.clear();
        fp_char_ptrs.clear();
        cache_strings.clear();
        cache_ints.clear();
        cache_floats.clear();
        widget_vec.clear();
        pushables.clear();
        action_map.clear();
        action_interned_map.clear();
        action_error_map.clear();
        bad_action_keys.clear();
        address_map.clear();
        bad_addrs.clear();
        bad_data_refs.clear();
        layout_errors.clear();
    }

    void parse_actions(const JSON& data, const JSON& action_seq, ActionVec& nd_action_vec, ActionInternVec& act_intern_vec, ActionErrorVec& act_error_vec) {
        // const static char* method = "DataLayCache::parse_actions: ";
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
                    std::stringstream ss;
                    ss << "BAD_RNAME(" << rname << ")";
                    std::string error{ ss.str() };
                    errors.error_vec.push_back(error);
                    errors.inx = inx;
                    action_errors.push_back(error);
                }
                else {
                    interned.pop_ui = (char*)render_names[action.pop_ui];
                }
            }
            if (JContains(action_defn, Static::ui_push_cs)) {
                // ui_push must be a widget_id. Since action parsing happens before
                // layout, this is the first time we may encounter encounter a
                // widget_id. NB there may be widget_ids in layout that aren't 
                // referenced in actions.
                std::string widget_id = JAsString(action_defn, Static::ui_push_cs);
                action.push_ui = add_widget_id(widget_id);
                interned.push_ui = (char*)get_string_value(action.push_ui);
            }
            if (JContains(action_defn, Static::db_action_cs)) {
                // Command, Query & BatchRequest DB actions all require query_id
                // Command & Query need sql_cname too
                std::string db_action = JAsString(action_defn, Static::db_action_cs);
                action.db_action = DBEventTypeFromString(db_action);
                interned.db_action = (char*)db_event_types[action.db_action];
                // FunctionSync|FunctionAsync
                if (action.db_action == DBEventType::dbFunctionSync
                        || action.db_action == DBEventType::dbFunctionAsync) {
                    std::string func_id = JAsString(action_defn, Static::query_id_cs);
                    action.query_id = get_func_id(func_id);
                    interned.query_id = (char*)get_string_value(action.query_id);
                    if (JContains(action_defn, Static::sql_cname_cs)) {
                        std::string fn_result_key = JAsString(action_defn, Static::sql_cname_cs);
                        // This add_address should just find the addr cached by data keys
                        // parsing earlier...
                        action.sql_cname = add_address(fn_result_key);
                        // check that the sql cname refers to a cache data entry
                        interned.sql_cname = (char*)get_addr_value(action.sql_cname);
                        if (!JContains(action_defn, Static::ctype_cs)) {
                            std::stringstream ss;
                            ss << "CTYPE_NOT_FOUND(" << fn_result_key << ")";
                            std::string error{ ss.str() };
                            errors.error_vec.push_back(error);
                            errors.inx = inx;
                            action_errors.push_back(error);
                        }
                        else {
                            std::string ctype = JAsString(action_defn, Static::ctype_cs);
                            action.ctype = CDTFromString(ctype);
                            interned.ctype = (char*)CDTToString(action.ctype);
                            if (action.ctype == EndDataTypes) {
                                std::stringstream ss;
                                ss << "BAD_CTYPE(" << ctype << ")";
                                std::string error{ ss.str() };
                                errors.error_vec.push_back(error);
                                errors.inx = inx;
                                action_errors.push_back(error);
                            }
                            else {
                                DataRef data_ref = CreateDataRef(action.ctype, action.sql_cname,
                                    data, fn_result_key);
                                // we may see the same sql_cname/ctype in several actions...
                                if (data_ref_map.find(data_ref.addr_inx) == data_ref_map.end()) {
                                    data_ref_map[data_ref.addr_inx] = data_ref;
                                }
                            }
                        }
                    }

                }
                else {  // Command|Query|BatchRequest
                    std::string query_id = JAsString(action_defn, Static::query_id_cs);
                    action.query_id = add_query_id(query_id);
                    interned.query_id = (char*)get_string_value(action.query_id);

                    if (action.db_action == dbCommand || action.db_action == dbQuery) {
                        std::string sql_cache_key = JAsString(action_defn, Static::sql_cname_cs);
                        // This add_address should just find the addr cached by data keys
                        // parsing earlier...
                        action.sql_cname = add_address(sql_cache_key);
                        // check that the sql cname refers to a cache data entry
                        interned.sql_cname = (char*)get_addr_value(action.sql_cname);
                        auto amit = address_map.find(sql_cache_key);
                        if (amit == address_map.end()) {
                            std::stringstream ss;
                            ss << "CACHE_KEY_NOT_FOUND(" << sql_cache_key << ")";
                            std::string error{ ss.str() };
                            errors.error_vec.push_back(error);
                            errors.inx = inx;
                            action_errors.push_back(error);
                        }
                        else {
                            // create data_ref_map entry for the query/cmd SQL source
                            // NB we assume a cdStr. TODO: extend to support cdStrVec via
                            // ctype
                            DataRef data_ref{ cdStr, amit->second };
                            data_ref.size = 1;
                            std::string sql = JAsString(data, sql_cache_key);
                            data_ref.ref_inx = get_string_index<CIT::Value>(sql)();
                            data_ref_map[data_ref.addr_inx] = data_ref;
                        }
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
        if (render_is_valid(action.pop_ui)) {
            if (prefix_comma) NDLogger::cout() << ", ";
            NDLogger::cout() << "pop_ui(" << render_names[action.pop_ui] << "/" << interned.pop_ui << ")";
            prefix_comma = true;
        }
        if (db_event_is_valid(action.db_action)) {
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
        if (cache_data_type_is_valid(action.ctype)) {
            if (prefix_comma) NDLogger::cout() << ", ";
            NDLogger::cout() << "ctype(" << action.ctype << "/" << interned.ctype << ")";
        }
        NDLogger::cout() << "]";
        NDLogger::cout().flush();
    }

    void on_data(const JSON& data) {
        // const static char* method = "DataLayCache::on_data: ";

        StringVec data_keys;
        StringSet special_keys_found;
        JKeys(data, data_keys);
        for (auto cit = data_keys.cbegin(); cit != data_keys.end(); ++cit) {
            std::string key{ *cit };
            if (special_keys.find(key) != special_keys.end()) {
                special_keys_found.emplace(key);
                continue;
            }
            AddrInx ainx = add_address(key);
            // We could try and parse the values here, but how do
            // we tell an int from a float? We cannot, because JS only
            // has "number". We can figure atomic vs array, and string vs
            // number, but not float vs int. However, when we parse
            // cspecs in on_layout, we can know from context what type
            // is required. That's when we can create the data_ref_map
            // entry that matches the address_map entry.
        }

        // Create funcIDs before actions are parsed
        if (special_keys_found.find(Static::functions_cs) != special_keys_found.end()) {
            StringVec func_names;
            JAsStringVec(data, Static::functions_cs, func_names);
            add_func_ids(func_names);
        }
        
        // Create a StrVec DataRef for each menu. Each Str in the StrVec is a menu_item
        if (special_keys_found.find(Static::menus_cs) != special_keys_found.end()) {
            const JSON& jmenus(data[Static::menus_cs]);
            StringVec menu_name_vec;
            JKeys(jmenus, menu_name_vec);
            for (auto mit = menu_name_vec.cbegin(); mit != menu_name_vec.cend(); ++mit) {
                std::string menu_name{ *mit };
                EntityInx menu_inx = add_menu_id(menu_name);
                DataRef menu_data_ref{ CreateDataRef(cdStrVec, menu_inx(), jmenus, menu_name) };
                menu_data_ref_map[menu_inx] = menu_data_ref;

                // add post proc for menus that creates menu_item
                // entries in the menu_address_map
                StrInx mitem_inx{ menu_data_ref.ref_inx };
                for (int i = 0; i < menu_data_ref.size; i++) {
                    const char* menu_item = get_string_value(mitem_inx);
                    assert(menu_item != nullptr);
                    // create a unique index for each menu_item that can be 
                    // used as the Entity inx
                    add_menu_id(menu_item);
                    mitem_inx++;
                }
            }
        }

        if (special_keys_found.find(Static::actions_cs) != special_keys_found.end()) {
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
                EntityInx entity_inx = get_string_index<CIT::EntityID>(entity);
                std::string event;
                if (!std::getline(ss, event, Static::period_c)) {
                    bad_action_keys.push_back(action_key_s);
                    continue;
                }
                EventInx event_inx = get_string_index<CIT::Event>(event);
                // combine two inx...
                ActionKey action_key{ entity_inx, event_inx };
                ActionVec nd_action_vec{};
                ActionInternVec action_intern_vec{};
                ActionErrorVec action_error_vec{};
                JSON action_seq = JSON::array();
                action_seq = jactions[action_key_s];
                parse_actions(data, action_seq, nd_action_vec, action_intern_vec, action_error_vec);
                action_map[action_key] = nd_action_vec;
                action_interned_map[action_key] = action_intern_vec;
                action_error_map[action_key] = action_error_vec;
            }
        }
    }

    void orthogonalize_cspec(const JSON& cspec, const JSON& data, WidgetPtr widget) {
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
                    widget->cspec_int[spec] = get_int_index(JAsInt(cspec, value_name));
                    break;
                case cdFloat:
                    widget->cspec_float[spec] = get_float_index(JAsFloat(cspec, value_name));
                    break;
                case cdBool:
                    widget->cspec_bool[spec] = get_bool_index(JAsBool(cspec, value_name) ? 1 : 0);
                    break;
                case cdStr:
                    widget->cspec_str[spec] = get_string_index<Value>(JAsString(cspec, value_name));
                    break;
                case cdIntVec:
                case cdStrVec:
                case cdAny:
                case cdResultSet:
                case EndDataTypes:
                    assert(false);
                    break;
                }
            }
        }
        // now we'll handle the address cspecs that ref data keys
        const CacheSpecTypeMap& cs_type_map{ addr_cspecs[widget->rname] };
        for (auto ctmit = cs_type_map.cbegin(); ctmit != cs_type_map.cend(); ++ctmit) {
            const CacheSpecifier spec{ ctmit->first };
            // if we did ref_type = cspec_types[spec] we'd get cdAny
            // for cname. Instead we get it from the CacheSpecTypeMap 
            CacheDataType ref_type{ ctmit->second };
            std::string ref_name = cspec_names[spec];   // [cindex|cname|query_id|menubar]
            if (!JContains(cspec, ref_name.c_str())) {
                // menubar is optional in the Home and Window cspec
                if (spec == cs_menu_bar)
                    continue;
                bad_data_refs.push_back(ref_name);
                std::stringstream ss;
                ss << "BAD_DATA_REF(" << ref_name << ") in cspec:" << cspec;
                layout_errors.push_back(ss.str());
                continue;
            }
            // cname|cindex|menubar: ref_name will be a data addr
            // query_id: ref_name will be an EntityInx
            // NB query_id may now be a FuncID EntityInx
            std::string addr_or_qid{ JAsString(cspec, ref_name) };
            // NB amit->second is AddrInx. Either amit->second
            // will be good, or query_inx
            auto amit = address_map.find(addr_or_qid);
            EntityInx query_inx = get_query_id(addr_or_qid);

            // cindex|cname|menubar data_refs must have an address_map entry.
            // query_id data_refs do not have an address_map entry,
            //      but must have a valid EntityInx, which should have
            //      been created by on_data() when it parses ActionKeys
            // menu data_ref fulfilled in menus
            // menu_item data_ref to enabled bool is optional
            if (amit == address_map.end()) {
                if (ref_name == Static::cname_cs || 
                    ref_name == Static::cindex_cs ||
                    ref_name == Static::menu_bar_cs) {
                    bad_data_refs.push_back(ref_name);
                    std::stringstream ss;
                    ss << "BAD_DATA_REF(" << ref_name << "/" << addr_or_qid << ") not address mapped in cspec:";
                    ss << cspec;
                    layout_errors.push_back(ss.str());
                    continue;
                }
                if (ref_name == Static::query_id_cs && !query_inx.is_valid()) {
                    bad_data_refs.push_back(ref_name);
                    std::stringstream ss;
                    ss << "BAD_DATA_REF(" << ref_name << "/" << addr_or_qid << ") not used by any data.actions ActionKey occurs in cspec:";
                    ss << cspec;
                    layout_errors.push_back(ss.str());
                    continue;
                }
            }
            DataRef data_ref = CreateDataRef(ref_type, 
                query_inx.is_valid() ? query_inx() : amit->second(), 
                data, addr_or_qid);
            // sanity check the DataRef
            if (data_ref.size == 0 && 
                (data_ref.tipe == cdStrVec || data_ref.tipe == cdIntVec)) {
                bad_data_refs.push_back(ref_name);
                std::stringstream ss;
                ss << "PROTO_VEC_SIZE_0(" << ref_name << "/" << addr_or_qid << ") is empty";
                layout_errors.push_back(ss.str());
                continue;
            }
            else {
                widget->data_refs[spec] = data_ref;
                data_ref_map[data_ref.addr_inx] = data_ref;
            }
        }
    }

    DataRef CreateDataRef(CDT ref_type, AddrInx inx, const JSON& data, const std::string& addr) {
        DataRef data_ref{ ref_type, inx };
        StringVec svec;
        IntVec ivec;
        JSON jvec{ JSON::array() };

        switch (ref_type) {
        case cdAny:         // shouldn't be in addr_cspecs!
        case EndDataTypes:
            assert(false);
            break;

        case cdFloat:       // not required by any widget yet
            assert(false);
            break;
        case cdResultSet:
            // no need to set data_ref.ref_inx as the result set
            // is not in the data cache
            assert(true);
            break;
        case cdStr: // spec:[xname|yname], sz:1. TODO: extend to strvec for multi Yval plots
            data_ref.ref_inx = get_string_index<CIT::Value>(JAsString(data, addr))();
            break;
        case cdInt: // spec:cindex, sz:1
            data_ref.ref_inx = get_int_index(JAsInt(data, addr))();
            break;
        case cdBool:
            data_ref.ref_inx = get_bool_index(JAsInt(data, addr))();
            break;
        case cdIntVec:
            jvec = data[addr];
            data_ref.size = JSize(jvec);
            if (data_ref.size > 0) {
                // capture the "base" index; subsequent indices
                // are implied by data_ref.size
                data_ref.ref_inx = get_int_index(JAsInt(jvec[0]))();
                for (uint32_t jinx = 1; jinx < data_ref.size; jinx++)
                    get_int_index(JAsInt(jvec[jinx]));
            }
            break;
        case cdStrVec:
            JAsStringVec(data, addr.c_str(), svec);
            data_ref.size = (uint32_t)svec.size();
            if (data_ref.size > 0) {
                auto it = svec.begin();
                // only record addr of 1st vec element
                data_ref.ref_inx = contiguous_string_index<CIT::Value>(*it++)();
                while (it != svec.end()) {
                    contiguous_string_index<CIT::Value>(*it++);
                }
            }
            break;
        }
        return data_ref;
    }

    void on_layout(const JSON& data, const JSON& layout, WidgetVec* wv = nullptr) {
        // If we're not recursing, widgets go in top level vec...
        WidgetVec* wvec = (wv == nullptr) ? &widget_vec : wv;
        // NB layout as a whole is a list of widgets.
        // And so is children, hence the recursion...
        int layout_length = JSize(layout);
        if (layout_length == -1) {
            layout_errors.push_back(std::string("BAD_DATA_REF(layout) not an array"));
            return;
        }
        for (int inx = 0; inx < layout_length; inx++) {
            const JSON& w(layout[inx]);
            const JSON cspec(extract_cspec<JSON>(w));
            // The only NDWidget ctor invocation...
            EntityInx winx; // invalid on init
            std::string widget_id_s{ extract_string(w, Static::widget_id_cs) };
            if (!widget_id_s.empty()) winx = get_string_index<EntityID>(widget_id_s, WidgetID);
            auto wptr = std::make_shared<NDWidget>(extract_render_name(w), winx);
            wvec->push_back(wptr);
            orthogonalize_cspec(cspec, data, wvec->back());
            const JSON children = extract_children(w);
            int child_count = JSize(children);
            if (child_count > 0) {
                on_layout(data, children, &(wptr->children));
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

public:
    DataLayCache(int capacity = 256) {
        fp_float_ptrs.reserve(capacity);
        fp_int_ptrs.reserve(capacity);
        fp_char_ptrs.reserve(capacity);
        cache_strings.reserve(capacity);
        cache_ints.reserve(capacity);
        cache_floats.reserve(capacity);

        EntityInx winx = get_string_index<EntityID>(Static::i_am_noop_cs, WidgetID);
        noop_widget = std::make_shared<NDWidget>(RenderMethod::Noop, winx);
    }

    size_t addr_map_size() { return address_map.size(); }
    size_t menu_entity_map_size() { return menu_entity_map.size(); }
    size_t widget_vec_size() { return widget_vec.size(); }
    size_t pushables_size() { return pushables.size(); }
    size_t action_map_size() { return action_map.size(); }
    size_t data_ref_map_size() { return data_ref_map.size(); }
    size_t menu_data_ref_map_size() { return menu_data_ref_map.size(); }
    size_t error_count() { return action_errors.size() + layout_errors.size(); }

    void on_json(const JSON& data, const JSON& layout, VVFunc on_init) {
        clear();
        on_data(data);
        on_layout(data, layout);
        if (on_init) on_init();
    }

    void on_data_change(const std::string& addr, const JSON& dc) {
        AddrInx ainx{ get_addr_inx(addr) };
        if (!ainx.is_valid())
            throw std::runtime_error("NoDOM BAD_ADDR:on_data_change:"+addr);
        auto it = data_ref_map.find(ainx);
        if (it == data_ref_map.end()) {
            // Not in data_ref_map means there are no cspec cindex or cname
            // refs to this addr in data, so assume it's a string...
            std::string new_val = JAsString(dc, Static::new_value_cs);
            update_string(ainx(), new_val);
            return;
        }
            
        DataRef& data_ref{it->second};
        assert(data_ref.addr_inx() == ainx());

        StringVec svec;
        IntVec ivec;
        JSON jvec{ JSON::array() };

        switch (data_ref.tipe) {
        case cdAny:         // shouldn't be in addr_cspecs!
        case cdResultSet:
        case EndDataTypes:
            assert(false);
            break;
        case cdFloat:       // not required by any widget yet
            assert(false);
            break;
        case cdStr:         // NDAction::sql_cname SQL data_ref
            update_string(data_ref.ref_inx, JAsString(dc, Static::new_value_cs));
            break;
        case cdInt: // spec:cindex, sz:1
            update_int(data_ref.ref_inx, JAsInt(dc, Static::new_value_cs));
            break;
        case cdBool:
            update_bool(data_ref.ref_inx, JAsBool(dc, Static::new_value_cs));
            break;
        case cdIntVec:
            jvec = dc[Static::new_value_cs];
            assert(data_ref.size = JSize(jvec));
            for (uint32_t jinx = 0; jinx < data_ref.size; jinx++) {
                update_int(data_ref.ref_inx + jinx, JAsInt(jvec[jinx]));
            }
            break;
        case cdStrVec:
            JAsStringVec(dc, Static::new_value_cs, svec);
            assert(data_ref.size = (uint32_t)svec.size());
            for (size_t jinx = 0; jinx < data_ref.size; jinx++) {
                update_string(data_ref.ref_inx + (uint32_t)jinx, svec[jinx]);
            }
            break;
        }
    }

    ActionVec* get_action_vec(ActionKey ak) {
        auto it = action_map.find(ak);
        if (it != action_map.end()) {
            return &(it->second);
        }
        return nullptr;
    }

    DataRef* get_data_ref(AddrInx ainx) {
        auto it = data_ref_map.find(ainx);
        if (it != data_ref_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    DataRef* get_menu_data_ref(EntityInx einx) {
        auto it = menu_data_ref_map.find(einx);
        if (it != menu_data_ref_map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    WidgetPtr get_pushable(EntityInx widget_id) {
        auto pit = pushables.find(widget_id);
        if (pit != pushables.end()) return pit->second;
        throw std::runtime_error("NoDOM BAD_WIDGET_ID:"+std::to_string(widget_id()));
    }

    WidgetPtr get_home() {
        assert(widget_vec.size() > 0);
        return widget_vec[0];
    }

    template <typename INX>
    const char* get_string_value(INX inx) {
        return fp_char_ptrs[inx()];
    }

    const char* get_addr_value(AddrInx inx) {
        return fp_char_ptrs[inx()];
    }

    AddrInx get_addr_inx(const std::string& addr) {
        auto it = address_map.find(addr);
        return it == address_map.end() ? 0 : it->second;
    }

    int get_func_inx(EntityInx finx) {
        auto it = js_func_inx_map.find(finx);
        if (it == js_func_inx_map.end())
            return -1;
        return it->second;
    }

    const char* get_func_name(uint32_t raw_inx) {
        if (raw_inx < js_func_vec.size()) {
            return js_func_vec[raw_inx].c_str();
        }
        return nullptr;
    }

    EntityInx get_func_id(const std::string& func_name) {
        auto it = js_func_entity_map.find(func_name);
        return it == js_func_entity_map.end() ? 0 : it->second;
    }

    EntityInx get_menu_id(const std::string& menu) {
        auto it = menu_entity_map.find(menu);
        return it == menu_entity_map.end() ? 0 : it->second;
    }

    int* get_int_value(IntInx inx) {
        return fp_int_ptrs[inx()];
    }

    bool* get_bool_value(BoolInx inx) {
        return fp_bool_ptrs[inx()];
    }

    AddrInx add_address(const std::string& addr) {
        AddrInx ainx = get_string_index<CIT::Address>(addr);
        address_map[addr] = ainx;
        return ainx;
    }

    EntityInx add_menu_id(const std::string& addr) {
        EntityInx einx{ get_string_index<CIT::EntityID>(addr, CST::QueryID) };
        menu_entity_map[addr] = einx;
        return einx;
    }

    EntityInx add_widget_id(const std::string& wid) {
        EntityInx inx{ get_string_index<CIT::EntityID>(wid, CST::WidgetID) };
        widget_map[wid] = inx;
        return inx;
    }

    EntityInx add_query_id(const std::string& qid) {
        EntityInx inx{ get_string_index<CIT::EntityID>(qid, CST::QueryID) };
        query_map[qid] = inx;
        return inx;
    }

    void add_func_ids(const StringVec& func_list) {
        if (func_list.size() == 0)
            return;
        int base_inx = -1;
        for (auto it = func_list.cbegin(); it != func_list.cend(); ++it) {
            EntityInx func_inx{ contiguous_string_index<CIT::EntityID>(*it, CST::JSFuncID) };
            if (base_inx == -1)
                base_inx = func_inx();
            js_func_entity_map[*it] = func_inx;
            js_func_inx_map[func_inx] = func_inx() - base_inx;
            js_func_vec.push_back(*it);
        }
    }

    EntityInx get_query_id(const std::string& qid) {
        auto qmit = query_map.find(qid);
        if (qmit != query_map.end())
            return qmit->second;
        return invalid_entity;
    }

    IntInx extern_int(int* v) {
        cache_ints.push_back(*v);
        fp_int_ptrs.push_back(v);
        return IntInx{ (uint32_t)fp_int_ptrs.size() - 1 };
    }

    FloatInx extern_float(float* v) {
        cache_floats.push_back(*v);
        fp_float_ptrs.push_back(v);
        return FloatInx{ (uint32_t)fp_float_ptrs.size() - 1 };
    }

    BoolInx extern_bool(bool* v) {
        fp_bool_ptrs.push_back(v);
        BoolInx binx{ (uint32_t)fp_bool_ptrs.size() - 1 };
        cache_bools[binx()] = *v;
        return binx;
    }

    const char* get_render_name(RenderMethod rm) {
        return render_names[rm];
    }

    const char* get_cspec_name(CacheSpecifier cs) {
        if (cs < CacheSpecifier::cs_end_cache_specs)
            return cspec_names[cs];
        return nullptr;
    }

private:
    // statics that define DataLayCache data and layout geometry
    inline static std::array<const char*, EndRenderMethod> render_names{
        Static::rm_noop_cs,
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
        Static::rm_pop_font_cs,
        Static::rm_window_cs,
        Static::rm_shaded_plot_cs
    };

    inline static std::array<const char*, EndDBEventTypes> db_event_types{
        Static::command_cs,
        Static::command_result_cs,
        Static::query_cs,
        Static::query_result_cs,
        Static::batch_request_cs,
        Static::batch_response_cs,
        Static::function_async_cs,
        Static::function_result_cs
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
        Static::column_flags_cs,      // cs_column_flags
        Static::show_lines_cs,
        Static::show_fills_cs,
        Static::shaded_plot_flags_cs,
        Static::menu_bar_cs,
        Static::menu_cs,
        Static::menu_item_cs,
        Static::db_cs,     // cs_db,
        Static::fps_cs,     // cs_fps,
        Static::demo_cs,     // cs_demo,
        Static::id_stack_cs,     // cs_id_stack,
        Static::font_scale_cs,    // cs_font_scale,
        Static::style_cs,        // cs_style
        Static::dlc_cs,
        Static::cname_cs,
        Static::cindex_cs,
        Static::query_id_cs,
        Static::xname_cs,
        Static::yname_cs
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
        cdInt,      // cs_column_flags
        cdBool,     // cs_show_lines
        cdBool,     // cs_show_fills
        cdInt,      // cs_shaded_plot_flags
        cdStr,      // cs_menu_bar
        cdStr,      // cs_menu
        cdStr,      // cs_menu_item
        cdBool,     // cs_show_footer_db
        cdBool,     // cs_show_footer_fps
        cdBool,     // cs_show_footer_demo
        cdBool,     // cs_show_footer_id_stack
        cdBool,     // cs_show_footer_font_scale
        cdBool,     // cs_show_footer_style
        cdBool,     // cs_show_footer_dlc
        cdAny,      // cs_cname
        cdAny,      // cs_cindex
        cdResultSet,// cs_query_id
        cdStr,      // cs_xname
        cdStr       // cs_yname
    };

    inline static  std::map<RenderMethod, CacheSpecVec> value_cspecs{
        {Home, {cs_title, cs_title_font, cs_title_font_size, cs_window_flags}},
        {InputInt, {cs_label, cs_step, cs_step_fast, cs_flags}},
        {Combo, {cs_label, cs_step}},
        {Checkbox, {cs_label}},
        {Text, {cs_text}},
        {Button, {cs_text}},
        {Table, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_table_flags, cs_window_flags, cs_column_flags}},
        {Footer, {cs_show_footer_db, cs_show_footer_fps, cs_show_footer_demo, 
                    cs_show_footer_id_stack, cs_show_footer_font_scale, 
                        cs_show_footer_style, cs_show_footer_dlc}},
        {DatePicker, {cs_label, cs_year_month_font, cs_year_month_font_size,
                        cs_day_date_font, cs_day_date_font_size,
                        cs_table_flags, cs_combo_flags}},
        {DuckTableSummaryModal, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_table_flags, cs_window_flags, cs_column_flags,
                    cs_button_font, cs_button_font_size}},
        {LoadingModal, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_spinner_thickness, cs_spinner_radius,
                    cs_window_flags}},
        {Window, {cs_title, cs_title_font, cs_title_font_size,
                    cs_window_flags}},
        {ShadedPlot, {cs_title, cs_show_lines, cs_show_fills, cs_shaded_plot_flags}},
        {PushFont, {cs_font, cs_font_size}},
        {BeginChild, {cs_title}}
    };

    inline static std::map<RenderMethod, CacheSpecTypeMap> addr_cspecs{
        {Home, {
            {cs_menu_bar, cdStrVec}
        }},
        {InputInt, {{cs_cname, cdInt}}},
        {Combo, {
            {cs_cindex, cdInt},
            {cs_cname, cdStrVec}
        }},
        {Checkbox, {{cs_cname, cdBool}}},
        {DatePicker, {{cs_cname, cdIntVec}}},
        {LoadingModal, {{cs_cname, cdStrVec}}},
        {DuckTableSummaryModal, {{cs_query_id, cdResultSet}}},
        {Table, {{cs_query_id, cdResultSet}}},
        {ShadedPlot, {
            {cs_query_id, cdResultSet},
            {cs_xname, cdStr},
            {cs_yname, cdStr}
        }},
        {Window, {
            {cs_menu_bar, cdStrVec}
        }}

    };

public:
    void report_sanity_check() {
        std::cout << "== report_sanity_check" << std::endl;
        std::cout << "sizeof(int):" << sizeof(int) << std::endl;
        std::cout << "sizeof(float):" << sizeof(float) << std::endl;
        std::cout << "sizeof(double):" << sizeof(double) << std::endl;
        std::cout << "sizeof(bool):" << sizeof(bool) << std::endl;
        std::cout << "sizeof(uint32_t):" << sizeof(uint32_t) << std::endl;
        std::cout << "sizeof(uint64_t):" << sizeof(uint64_t) << std::endl;
        std::cout << "sizeof(size_t):" << sizeof(size_t) << std::endl;
        std::cout << "sizeof(char*):" << sizeof(char*) << std::endl;
        std::cout << std::endl;
    }

    int report_cache_strings(int& externals) {
        size_t fp_len = fp_char_ptrs.size();
        size_t cs_len = cache_strings.size();
        // sizeof(char8)==8 on x64, 4 on wasm
        // 4 bytes is 8 hex digits, 8 bytes is 16
        constexpr size_t cs_sz = 2 * sizeof(char*);
        std::cout << "== report_cache_strings ptrs:"
            << std::dec << fp_len << ", cached:" << std::dec << cs_len << std::endl;
        std::cout << "inx:val:cptr:fptr" << std::endl;
        externals = 0;
        for (int inx = 0; inx < cs_len; inx++) {
            const char* cache_ptr = cache_strings[inx].c_str();
            const char* fast_ptr = fp_char_ptrs[inx];
            size_t cp_val = (size_t)cache_ptr;
            size_t fp_val = (size_t)fast_ptr;

            std::cout << std::hex << std::setfill('0');
            std::cout << std::setw(3) << inx << ":" << cache_strings[inx] << ":";
            std::cout << "0x" << std::setw(cs_sz) << cp_val << ":";
            if (cp_val != fp_val) {
                std::cout << "0x" << std::setw(cs_sz) << fp_val << std::endl;
                externals++;
            }
            else {
                std::cout << "==" << std::string(cs_sz, '=') << std::endl;
            }
        }
        std::cout << "count:" << cs_len << ", externals:" << externals << std::endl;
        std::cout << std::endl;
        return (int)cs_len;
    }

    int report_cache_ints(int& externals) {
        size_t fp_len = fp_int_ptrs.size();
        size_t cs_len = cache_ints.size();
        std::cout << "== report_cache_ints ptrs:"
            << std::dec << fp_len << ", cached:" << std::dec << cs_len << std::endl;
        std::cout << "inx:val:cptr:fptr" << std::endl;
        externals = 0;
        for (int inx = 0; inx < cs_len; inx++) {
            int* cache_ptr = &(cache_ints[inx]);
            int* fast_ptr = fp_int_ptrs[inx];
            size_t cp_val = (size_t)cache_ptr;
            size_t fp_val = (size_t)fast_ptr;

            std::cout << std::hex << std::setfill('0');
            std::cout << std::setw(3) << inx << ":" << cache_ints[inx] << ":";
            std::cout << cp_val << ":";
            if (cp_val != fp_val) {
                std::cout << fp_val << std::endl;
                externals++;
            }
            else {
                std::cout << "====" << std::endl;
            }
        }
        std::cout << "count:" << cs_len << ", externals:" << externals << std::endl;
        std::cout << std::endl;
        return (int)cs_len;
    }

    int report_cache_floats(int& externals) {
        size_t fp_len = fp_float_ptrs.size();
        size_t cs_len = cache_floats.size();
        std::cout << "== report_cache_floats ptrs:"
            << std::dec << fp_len << ", cached:" << std::dec << cs_len << std::endl;
        std::cout << "inx:val:cptr:fptr" << std::endl;
        externals = 0;
        for (int inx = 0; inx < cs_len; inx++) {
            float* cache_ptr = &(cache_floats[inx]);
            float* fast_ptr = fp_float_ptrs[inx];
            size_t cp_val = (size_t)cache_ptr;
            size_t fp_val = (size_t)fast_ptr;

            std::cout << std::hex << std::setfill('0');
            std::cout << std::setw(3) << inx << ":" << cache_floats[inx] << ":";
            std::cout << cp_val << ":";
            if (cp_val != fp_val) {
                std::cout << fp_val << std::endl;
                externals++;
            }
            else {
                std::cout << "====" << std::endl;
            }
        }
        std::cout << "count:" << cs_len << ", externals:" << externals << std::endl;
        std::cout << std::endl;
        return (int)cs_len;
    }

    void report_address_map() {
        size_t len = address_map.size();
        int inx{ 0 };
        std::cout << "== report_address_map len:" << std::dec << len << std::endl;
        std::cout << "inx:addr:AddrInx{0x0104,inx}:AddrVal" << std::endl;
        for (auto cit = address_map.cbegin(); cit != address_map.cend(); ++cit) {
            std::cout << std::setfill('0') << std::setw(3) << std::hex << inx++ << ":";
            std::cout << cit->first << ":" << cit->second << ":" << get_addr_value(cit->second) << std::endl;
        }
        std::cout << std::dec << std::endl;
    }

    void report_menu_entity_map() {
        size_t len = menu_entity_map.size();
        int inx{ 0 };
        std::cout << "== report_menu_entity_map len:" << std::dec << len << std::endl;
        std::cout << "inx:addr:EntityInx{0x0324,inx}" << std::endl;
        for (auto cit = menu_entity_map.cbegin(); cit != menu_entity_map.cend(); ++cit) {
            std::cout << std::setfill('0') << std::setw(3) << std::hex << inx++ << ":";
            std::cout << cit->first << ":" << cit->second << std::endl;
        }
        std::cout << std::dec << std::endl;
    }

    void report_data_refs() {
        size_t len = data_ref_map.size();
        int inx{ 0 };
        std::cout << "== report_data_ref_map len:" << std::dec << len << std::endl;
        std::cout << "inx:AddrInx:DataRef{tipe,addr,ref,sz}" << std::endl;
        for (auto drmit = data_ref_map.begin(); drmit != data_ref_map.end(); ++drmit) {
            std::cout << std::setfill('0') << std::setw(3) << std::hex << inx++ << ":";
            if (!drmit->first.is_valid()) {
                std::cout << "INVALID:"
                    << CDTToString(drmit->second.tipe) << ","
                    << get_addr_value(drmit->second.addr_inx) << ","
                    << drmit->second.ref_inx << ","
                    << drmit->second.size << std::endl;
            }
            else {
                AddrInx ainx{ drmit->first };
                std::cout << ainx
                    << ":"
                    << CDTToString(drmit->second.tipe) << ","
                    << get_addr_value(drmit->second.addr_inx) << ","
                    << std::hex << drmit->second.ref_inx << ","
                    << std::dec << drmit->second.size << std::endl;
            }
        }
        std::cout << std::dec << std::endl;
    }

    void report_menu_data_refs() {
        size_t len = menu_data_ref_map.size();
        int inx{ 0 };
        std::cout << "== report_menu_data_ref_map len:" << std::dec << len << std::endl;
        std::cout << "inx:AddrInx:DataRef{tipe,addr,ref,sz}" << std::endl;
        for (auto drmit = menu_data_ref_map.cbegin(); drmit != menu_data_ref_map.cend(); ++drmit) {
            std::cout << std::setfill('0') << std::setw(3) << std::hex << inx++ << ":";
            std::cout << drmit->first << ":"
                << drmit->second.tipe << ","
                << drmit->second.addr_inx << ","
                << drmit->second.ref_inx << ","
                << drmit->second.size << std::endl;
        }
        std::cout << std::dec << std::endl;
    }

    void report_func_maps() {
        // std::map<std::string, EntityInx>    js_func_entity_map;
        // std::map<EntityInx, uint32_t>       js_func_inx_map;

        size_t len = js_func_entity_map.size();
        int inx{ 0 };
        std::cout << "== report_js_func_entity_map len:" << std::dec << len << std::endl;
        std::cout << "inx:func:EntityInx{0x03a4,inx}" << std::endl;
        for (auto cit = js_func_entity_map.cbegin(); cit != js_func_entity_map.cend(); ++cit) {
            std::cout << std::setfill('0') << std::setw(3) << std::hex << inx++ << ":";
            std::cout << cit->first << ":" << cit->second << std::endl;
        }
        std::cout << "== report_js_func_inx_map len:" << std::dec << len << std::endl;
        std::cout << "inx:EntityInx{0x03a4,inx}:raw_inx" << std::endl;
        inx = 0;
        for (auto cit = js_func_inx_map.cbegin(); cit != js_func_inx_map.cend(); ++cit) {
            std::cout << std::setfill('0') << std::setw(3) << std::hex << inx++ << ":";
            std::cout << cit->first << ":" << cit->second << std::endl;
        }

        std::cout << std::dec << std::endl;
    }

    void report_actions() {
        int key_inx{ 0 };
        size_t actions_len = action_map.size();
        std::cout << "== report_action_map len:" << std::dec << actions_len << std::endl;
        std::cout << "inx:ActnKey:EntityInx{0x0304,inx}:EventInx{0x0404,inx}:str(action)" << std::endl;

        for (auto cit = action_map.cbegin(); cit != action_map.cend(); ++cit) {
            const ActionKey& key{ cit->first };
            const ActionVec& action_vec{ cit->second };
            // action_intern_vec should be same len as action_vec (see 
            // DLC::parse_actions()). So the resize() below should be a 
            // null op. But if not we'll dflt ctor NDActionInterned
            // to match lengths. The dflt constructed NDActionInterned will
            // all show as nullptr populated anyway...
            ActionInternVec& action_intern_vec{ action_interned_map[key] };
            action_intern_vec.resize(action_vec.size());
            std::cout << std::setfill('0') << std::setw(3) << std::hex << key_inx++ << ":";
            std::cout << key << ":EntityInx(" << key.entity_inx()
                << "):EventInx(" << key.event_inx() << "):";
            for (int act_inx = 0; act_inx < action_vec.size(); act_inx++) {
                print_parsed_action(action_vec[act_inx], action_intern_vec[act_inx]);
                std::cout << std::endl;
            }
        }
    }

    void report_cache_state() {
        int esc, eic, efc;
        report_cache_errors();
        report_cache_strings(esc);
        report_cache_ints(eic);
        report_cache_floats(efc);
        report_address_map();
        report_menu_entity_map();
        report_data_refs();
        report_func_maps();
        report_actions();
        std::cout << std::endl;
    }

    void report_cache_errors() {
        std::cout << "== layout errors" << std::endl;
        for (const auto& error : layout_errors) {
            std::cout << error << std::endl;
        }
        // NB action_errors is a single container
        // for all reported in action_map_errors
        std::cout << "== action errors" << std::endl;
        for (const auto& error : action_errors) {
            std::cout << error << std::endl;
        }
    }
};
