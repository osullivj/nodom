#pragma once
#include <string>
#include <map>
#include <queue>
#include <filesystem>
#include <functional>
#include "imgui.h"
#include "ImGuiDatePicker.hpp"
#include "proxy.hpp"
#include "static_strings.hpp"

// NoDOM emulation: debugging ND impls in JS is tricky. Code compiled from C++ to clang .o
// is not available. So when we port to EM, we have to resort to printf debugging. Not good
// when we want to understand what the "correct" imgui behaviour should be. So here we have
// just enough impl to emulate ND server and client scaffolding that allows us to debug
// imgui logic. So we don't bother with HTTP get, just the websockets, with enough
// C++ code to maintain when we just want to focus on the impl that is opaque in the browser.
// JOS 2025-01-22

#define ND_MAX_COMBO_LIST 16

typedef std::function<void(const std::string&)> ws_sender;

template <typename JSON>
class NDContext {
private:
    // ref to "server process"; just an abstraction of EMV vs win32
    NDProxy& proxy;
    // NSServer invokes will be replaced with EM_JS

    JSON                      layout; // layout and data are fetched by 
    JSON                      data;   // sync c++py calls not HTTP gets
    std::deque<JSON>          stack;  // render stack

    // map layout render func names to the actual C++ impls
    std::unordered_map<std::string, std::function<void(JSON& w)>> rfmap;

    // top level layout widgets with widget_id eg modals are in pushables
    std::unordered_map<std::string, JSON> pushable;
    // main.ts:action_dispatch is called while rendering, and changes
    // the size of the render stack. JS will let us do that in the root
    // render() method. But in C++ we use an STL iterator in the root render
    // method, and that segfaults. So in C++ we have pending pushes done
    // outside the render stack walk. JOS 2025-01-31
    std::deque<JSON>        pending_pushes;
    std::deque<std::string> pending_pops;
    bool    show_demo = false;
    bool    show_id_stack = false;
    bool    show_memory = false;

    // colours: https://www.w3schools.com/colors/colors_picker.asp
    ImColor red;    // ImGui.COL32(255, 51, 0);
    ImColor green;  // ImGui.COL32(102, 153, 0);
    ImColor amber;  // ImGui.COL32(255, 153, 0);
    ImColor db_status_color;

    std::map<std::string, ImFont*>  font_map;

    // default value for invoking std::find on nlohmann JSON iterators
    std::string null_value = "null_value";

    // ref to NDWebSockClient::send
    ws_sender ws_send;

public:
    NDContext(NDProxy& s) : proxy(s), red(255, 51, 0),
        green(102, 153, 0), amber(255, 153, 0) {
        // init status is not connected
        db_status_color = red;

        rfmap.emplace(std::string("Home"), [this](JSON& w) { render_home(w); });
        rfmap.emplace(std::string("InputInt"), [this](JSON& w) { render_input_int(w); });
        rfmap.emplace(std::string("Combo"), [this](JSON& w) { render_combo(w); });
        rfmap.emplace(std::string("Checkbox"), [this](JSON& w) { render_checkbox(w); });
        rfmap.emplace(std::string("Separator"), [this](JSON& w) { render_separator(w); });
        rfmap.emplace(std::string("Footer"), [this](JSON& w) { render_footer(w); });
        rfmap.emplace(std::string("SameLine"), [this](JSON& w) { render_same_line(w); });
        rfmap.emplace(std::string("DatePicker"), [this](JSON& w) { render_date_picker(w); });
        rfmap.emplace(std::string("Text"), [this](JSON& w) { render_text(w); });
        rfmap.emplace(std::string("Button"), [this](JSON& w) { render_button(w); });
        rfmap.emplace(std::string("DuckTableSummaryModal"), [this](JSON& w) { render_duck_table_summary_modal(w); });
        rfmap.emplace(std::string("DuckParquetLoadingModal"), [this](JSON& w) { render_duck_parquet_loading_modal(w); });
        rfmap.emplace(std::string("Table"), [this](JSON& w) { render_table(w); });
        rfmap.emplace(std::string("PushFont"), [this](JSON& w) { render_push_font(w); });
        rfmap.emplace(std::string("PopFont"), [this](JSON& w) { render_pop_font(w); });
        rfmap.emplace(std::string("BeginChild"), [this](JSON& w) { render_begin_child(w); });
        rfmap.emplace(std::string("EndChild"), [this](JSON& w) { render_end_child(w); });
        rfmap.emplace(std::string("BeginGroup"), [this](JSON& w) { render_begin_group(w); });
        rfmap.emplace(std::string("EndGroup"), [this](JSON& w) { render_end_group(w); });
    }

    // invoked by main loop
    void render() {
        const static char* method = "NDContext::render: ";

        if (pending_pops.size() || pending_pushes.size()) {
            std::cerr << method << pending_pops.size() << " pending pops, " << pending_pushes.size()
                << " pending pushes" << std::endl;
        }
        // address pending pops first: maintaining ordering by working from front to back
        // as that is the order they would land on the stack if not pushed during rendering
        while (!pending_pops.empty()) {
            pop_widget(pending_pops.front());
            pending_pops.pop_front();
        }
        // drain pending_pushes onto the render stack, maintaining
        // the stack order we would have had if the push had
        // happended intra-render. JOS 2025-01-31
        while (!pending_pushes.empty()) {
            push_widget(pending_pushes.front());
            pending_pushes.pop_front();
        }
        // This loop used to break if we raised a modal as changing stack state
        // while this iter is live segfaults. JS lets us get away with that in 
        // main.ts:render (imgui-js based Typescript NDContext).
        // So here we push modals into pending_pushes so they can
        // push/pop outside the context of the loop below. JOS 2025-01-31
        for (std::deque<nlohmann::json>::iterator it = stack.begin(); it != stack.end(); ++it) {
            // deref it for clarity and to rename as widget for
            // cross ref with main.ts logic
            JSON& widget{ *it };
            dispatch_render(widget);
        }
    }

    void server_request(const std::string& key) {
        JSON msg = { {nd_type_cs, cache_request_cs}, {cache_key_cs, key} };
#ifndef __EMSCRIPTEN__  // breadboard websockpp impl
        ws_send(msg.dump());
#else
        // TODO: EM_JS code to socket send

#endif  // __EMSCRIPTEN__
    }

    void notify_server(const std::string& caddr, JSON& old_val, JSON& new_val) {
        const static char* method = "NDContext::notify_server: ";

        std::cout << method << caddr << ", old: " << old_val << ", new: " << new_val << std::endl;

        // build a JSON msg for the to_python Q
        JSON msg = { {nd_type_cs, data_change_cs}, {cache_key_cs, caddr}, {new_value_cs, new_val}, {old_value_cs, old_val} };
        try {
            // TODO: websockpp send in breadboard, EM_JS fetch for EMS
#ifndef __EMSCRIPTEN__
            ws_send(msg.dump());
#else   // 
        // TODO: EM_JS fetch
#endif  // __EMSCRIPTEN__
        }
        catch (...) {
            std::cerr << "notify_server EXCEPTION!" << std::endl;
        }
    }

    void dispatch_server_responses(std::queue<JSON>& responses) {
        const static char* method = "NDContext::dispatch_server_responses: ";

        // server_changes will be a list of json obj copied out of a pybind11
        // list of py dicts. So use C++11 auto range...
        while (!responses.empty()) {
            JSON& resp = responses.front();
            std::cout << method << resp << std::endl;
            // polymorphic as types are hidden inside change
            // Is this a CacheResponse for layout or data?
            if (resp[nd_type_cs] == cache_response_cs) {
                if (resp[cache_key_cs] == data_cs) {
                    data = resp[value_cs];
                }
                else if (resp[cache_key_cs] == layout_cs) {
                    layout = resp[value_cs];
                    on_layout();
                }
            }
            // Is this a DataChange?
            else if (resp[nd_type_cs] == data_change_cs) {
                data[resp[cache_key_cs]] = resp[new_value_cs];
            }
            // Duck or Parquet event...
            else {
                on_db_event(resp);
            }
            responses.pop();
        }
    }

    bool duck_app() { return proxy.duck_app(); }
    void set_done(bool d) {
        // TODO: add code to do proxy.set_done() so the
        // duck_loop exits
    }

    void on_ws_open() {
        server_request("data");
        server_request("layout");
    }

    void on_db_event(JSON& duck_msg) {
        const static char* method = "NDContext::on_db_event: ";

        if (!duck_msg.contains("nd_type")) {
            std::cerr << method << "no nd_type in " << duck_msg << std::endl;
        }
        std::cout << method << duck_msg << std::endl;
        const std::string& nd_type(duck_msg[nd_type_cs]);
        if (nd_type == "ParquetScan") {
            db_status_color = amber;
        }
        else if (nd_type == "Query") {
            db_status_color = amber;
        }
        else if (nd_type == "ParquetScanResult") {
            db_status_color = green;
            action_dispatch(duck_msg["query_id"], nd_type);
        }
        else if (nd_type == "QueryResult") {
            db_status_color = green;
            const std::string& qid(duck_msg[query_id_cs]);
            JSON chunk_lo_hi = JSON::array();
            chunk_lo_hi = duck_msg[chunk_cs];
            std::cout << method << "chunk_ptr: " << std::hex << chunk_lo_hi << std::endl;
            std::string cname = qid + "_result";
            data[cname] = chunk_lo_hi;
        }
        else if (nd_type == "DuckInstance") {
            // TODO: q processing order means this doesn't happen so early in cpp
            // main.ts:on_duck_event invokes check_duck_module.
            // However, we don't need all the check_duck_module JS module stuff,
            // so we can just flip status button color here
            db_status_color = amber;
            // send a test query
            duck_dispatch("Query", "select 1729;", "ramanujan");
        }
        else {
            std::cerr << "NDContext::on_duck_event: unexpected nd_type in " << duck_msg << std::endl;
        }
    }

    void on_layout() {
        const static char* method = "NDContext::on_layout: ";
        // layout is a list of widgets; and all may have children
        // however, not all widgets are children. For instance modals
        // like parquet_loading_modal have to be explicitly pushed
        // on to the render stack by an event. JOS 2025-01-31
        // Fonts appear in layout now. JOS 2025-07-29
        for (typename JSON::iterator it = layout.begin(); it != layout.end(); ++it) {
            std::cout << method << *it << std::endl;
            // NB we used it->value("wiget_id", "") in nlohmann::json
            // to extract child value. emscripten::val doesn't implement
            // operator->, so we lean on JSON::iterator::operator*, which
            // both support.
            const JSON& w(*it);
            if (w.contains("widget_id")) {
                const std::string& widget_id = w["widget_id"].template get<std::string>();
                if (!widget_id.empty()) {
                    std::cout << method << "pushable: " << widget_id << ":" << *it << std::endl;
                    pushable[widget_id] = *it;
                }
                else {
                    std::cerr << method << "empty widget_id in: " << *it << std::endl;
                }
            }
        }
        // Home on the render stack
        stack.push_back(layout[0]);
    }

    JSON  get_breadboard_config() { return proxy.get_breadboard_config(); }

    void register_ws_callback(ws_sender send) { ws_send = send; proxy.register_ws_callback(send); }

    void register_font(const std::string& name, ImFont* f) { font_map[name] = f; }

protected:
    // w["rname"] resolve & invoke
    void dispatch_render(JSON& w) {
        const static char* method = "NDContext::dispatch_render: ";

        if (!w.contains("rname")) {
            std::cerr << method << "missing rname in " << w << std::endl;
            return;
        }
        const std::string& rname(w["rname"]);
        auto it = rfmap.find(rname);
        if (it == rfmap.end()) {
            std::cerr << method << "unknown rname in " << w << std::endl;
            return;
        }
        it->second(w);
    }

    void action_dispatch(const std::string& action, const std::string& nd_event) {
        const static char* method = "NDContext::action_dispatch: ";

        std::cout << method << "action(" << action << ")" << std::endl;

        if (action.empty()) {
            std::cerr << method << "no action specified!" << std::endl;
            return;
        }
        // Is it a pushable widget? NB this is only for self popping modals like DuckTableSummaryModal.
        // In this case we expect nd_event to be empty as we're not driven directly by the event, but
        // by ui_push and ui_pop action qualifiers associated with the nd_event list. JOS 2025-02-21
        // JOS 2025-02-22
        auto it = pushable.find(action);
        if (it != pushable.end() && nd_event.empty()) {
            std::cout << method << "pushable(" << action << ")" << std::endl;
            stack.push_back(pushable[action]);
        }
        else {
            if (!data.contains("actions")) {
                std::cerr << method << "no actions in data!" << std::endl;
                return;
            }
            // get hold of "actions" in data: do we have one matching action?
            JSON& actions = data["actions"];
            if (!actions.contains(action)) {
                std::cerr << method << "no actions." << action << " in data!" << std::endl;
                return;
            }
            JSON& action_defn = actions[action];
            if (!action_defn.contains("nd_events")) {
                std::cerr << method << "no nd_events in actions." << action << " in data!" << std::endl;
                return;
            }
            JSON nd_events = nlohmann::json::array();
            nd_events = action_defn["nd_events"];
            auto event_iter = nd_events.begin();
            bool event_match = false;
            while (event_iter != nd_events.end()) {
                if (*event_iter++ == nd_event) {
                    event_match = true;
                    break;
                }
            }
            if (!event_match) {
                std::cerr << method << "no match for nd_event(" << nd_event << ") in defn(" << action_defn << ") in data!" << std::endl;
                return;
            }
            // Now we have a matched action definition in hand we can look
            // for UI push/pop and DB scan/query. If there's both push and pop,
            // pop goes first naturally!
            if (action_defn.contains("ui_pop")) {
                // for pops we supply the rname, not the pushable name so
                // the context can check the widget type on pops
                const std::string& rname(action_defn["ui_pop"]);
                std::cout << method << "ui_pop(" << rname << ")" << std::endl;
                pending_pops.push_back(rname);
            }
            if (action_defn.contains("ui_push")) {
                // for pushes we supply widget_id, not the rname
                const std::string& widget_id(action_defn["ui_push"]);
                // salt'n'pepa in da house!
                auto push_it = pushable.find(widget_id);
                if (push_it != pushable.end()) {
                    std::cout << method << "ui_push(" << widget_id << ")" << std::endl;
                    // NB action_dispatch is called by eg render_button, which ultimately is called
                    // by render(), which iterates over stack. So we cannot change stack here...
                    pending_pushes.push_back(push_it->second);
                }
                else {
                    std::cerr << method << "ui_push(" << widget_id << ") no such pushable" << std::endl;
                }
            }
            // Finally, do we have a DB op to handle?
            if (action_defn.contains("db")) {
                JSON& db_op(action_defn["db"]);
                if (!db_op.contains("sql_cname") || !db_op.contains("query_id") || !db_op.contains("action")) {
                    std::cerr << method << "db(" << db_op << ") missing sql_cname|query_id|action" << std::endl;
                }
                else {
                    const std::string& sql_cache_key(db_op["sql_cname"]);
                    if (!data.contains(sql_cache_key)) {
                        std::cerr << method << "db(" << db_op << ") sql_cname(" << sql_cache_key << ") does not resolve" << std::endl;
                    }
                    else {
                        const std::string& sql(data[sql_cache_key]);
                        duck_dispatch(db_op["action"], sql, db_op["query_id"]);
                    }
                }
            }
        }
    }

    void duck_dispatch(const std::string& nd_type, const std::string& sql, const std::string& qid) {
        const static char* method = "NDContext::duck_dispatch: ";
        JSON duck_request = { {nd_type_cs, nd_type}, {sql_cs, sql}, {query_id_cs, qid} };
        proxy.duck_dispatch(duck_request);

    }

    // Render functions
    void render_home(JSON& w) {
        const static char* method = "NDContext::render_home: ";

        if (!w.contains("cspec")) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        JSON& cspec = w["cspec"];
        std::string title(nodom_cs);
        if (cspec.contains("title")) {
            title = cspec["title"];
        }

        bool fpop = false;
        if (cspec.contains(title_font_cs))
            fpop = push_font(w, title_font_cs, title_font_size_base_cs);

        ImGui::Begin(title.c_str());
        JSON& children = w["children"];
        for (typename JSON::iterator it = children.begin(); it != children.end(); ++it) {
            dispatch_render(*it);
        }
        if (fpop) {
            pop_font();
        }
        ImGui::End();
    }

    void render_input_int(JSON& w) {
        const static char* method = "NDContext::render_input_int: ";
        // static storage: imgui wants int (int32), nlohmann::json uses int64_t
        static int input_integer;
        input_integer = 0;
        if (!w.contains("cspec")) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        JSON& cspec = w["cspec"];
        std::string label(nodom_cs);
        if (cspec.contains("label")) label = cspec["label"];
        // params by value
        int step = 1;
        if (cspec.contains("step")) step = cspec["step"];
        int step_fast = 1;
        if (cspec.contains("step_fast")) step_fast = cspec["step_fast"];
        int flags = 0;
        if (cspec.contains("flags")) flags = cspec["flags"];
        // one param by ref: the int itself
        std::string& cname_cache_addr = cspec["cname"].template get<std::string>();
        // if no label use cache addr
        if (!label.size()) label = cname_cache_addr;
        // local static copy of cache val
        int old_val = input_integer = data[cname_cache_addr];
        // imgui has ptr to copy of cache val
        ImGui::InputInt(label.c_str(), &input_integer, step, step_fast, flags);
        // copy local copy back into cache
        if (input_integer != old_val) {
            data[cname_cache_addr] = input_integer;
            // TODO: mv semantics so notify_server can own the params
            notify_server(cname_cache_addr, JSON(old_val), JSON(input_integer));
        }
    }

    void render_combo(JSON& w) {
        const static char* method = "NDContext::render_combo: ";
        // Static storage for the combo list
        // NB single GUI thread!
        // No malloc at runtime, but we will clear the array with a memset
        // on each visit. JOS 2025-01-26
        static std::vector<std::string> combo_list;
        static const char* cs_combo_list[ND_MAX_COMBO_LIST];
        static int combo_selection;
        memset(cs_combo_list, 0, ND_MAX_COMBO_LIST * sizeof(char*));
        combo_selection = 0;
        combo_list.clear();
        if (!w.contains("cspec")) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        JSON& cspec = w["cspec"];
        std::string label(nodom_cs);
        if (cspec.contains("text")) label = cspec["text"];
        // params by value
        int step = 1;
        if (cspec.contains("step")) step = cspec["step"];
        // no value params in layout here; all combo layout is data cache refs
        // /cspec/cname should give us a data cache addr for the combo list
        std::string& combo_list_cache_addr(cspec["cname"].template get<std::string>());
        std::string& combo_index_cache_addr(cspec["index"].template get<std::string>());
        // if no label use cache addr
        if (!label.size()) label = combo_list_cache_addr;
        int combo_count = 0;
        combo_list = data[combo_list_cache_addr];
        for (auto it = combo_list.begin(); it != combo_list.end(); ++it) {
            cs_combo_list[combo_count++] = it->c_str();
            if (combo_count == ND_MAX_COMBO_LIST) break;
        }
        int old_val = combo_selection = data[combo_index_cache_addr];
        ImGui::Combo(label.c_str(), &combo_selection, cs_combo_list, combo_count, combo_count);
        if (combo_selection != old_val) {
            data[combo_index_cache_addr] = combo_selection;
            notify_server(combo_index_cache_addr, JSON(old_val), JSON(combo_selection));
        }
    }

    void render_checkbox(JSON& w) {
        const static char* method = "NDContext::render_checkbox: ";

        if (!w.contains(cspec_cs)) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        const std::string& cname_cache_addr(cspec[cname_cs]);
        const std::string& text(cspec[text_cs].empty() ? cname_cache_addr : cspec[title_cs]);

        bool checked_value = data[cname_cache_addr];
        bool old_checked_value = checked_value;

        ImGui::Checkbox(text.c_str(), &checked_value);

        if (checked_value != old_checked_value) {
            data[cname_cache_addr] = checked_value;
            notify_server(cname_cache_addr, nlohmann::json(old_checked_value), nlohmann::json(checked_value));
        }

    }

    void render_separator(JSON& w) {
        ImGui::Separator();
    }

    void render_footer(JSON& w) {
        static const char* method = "NDContext::render_footer: ";

        if (!w.contains(cspec_cs)) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        // TODO: optimise local vars: these cspec are not cache refs so could
// bound at startup time...
        bool db = cspec["db"];
        bool fps = cspec["fps"];
        // TODO: config demo mode so it can be switched on/off in prod
        bool demo = cspec["demo"];
        bool id_stack = cspec["id_stack"];
        // TODO: understand ems mem anlytics and restore in footer
        // bool memory = w.value(nlohmann::json::json_pointer("/cspec/memory"), true);

        if (db) {
            // Push colour styling for the DB button
            ImGui::PushStyleColor(ImGuiCol_Button, (ImU32)db_status_color);
            if (ImGui::Button("DB")) {
                // TODO: main.ts raises a new browser tab here...
            }
            ImGui::PopStyleColor(1);
        }
        if (fps) {
            ImGui::SameLine();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        if (demo) {
            ImGui::Checkbox("Demo", &show_demo);
            if (show_demo)  ImGui::ShowDemoWindow();
        }
        if (id_stack) {
            ImGui::SameLine();
            ImGui::Checkbox("IDStack", &show_id_stack);
            if (show_id_stack)  ImGui::ShowStackToolWindow();
        }
        /* TODO
        if (memory) {

        } */
    }

    void render_same_line(JSON& w) {
        ImGui::SameLine();
    }

    void render_date_picker(JSON& w) {
        const static char* method = "NDContext::render_date_picker: ";

        static int default_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedSame;
        static int default_combo_flags = ImGuiComboFlags_HeightRegular;
        //| ImGuiTableFlags_SizingFixedSame |
        //    ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_NoHostExtendY;
        static int ymd_i[3] = { 0, 0, 0 };
        ImFont* year_month_font = nullptr;
        ImFont* day_date_font = nullptr;
        uint32_t year_month_font_size_base = 0;
        uint32_t day_date_font_size_base = 0;

            if (!w.contains("cspec")) {
                std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
                return;
            }
            JSON& cspec = w["cspec"];
            if (cspec.contains(year_month_font_cs)) {
                const std::string& ym_font(cspec[year_month_font_cs]);
                auto font_it = font_map.find(ym_font);
                if (font_it != font_map.end()) {
                    year_month_font = font_it->second;
                    if (cspec.contains(year_month_font_size_base_cs))
                        year_month_font_size_base = cspec[year_month_font_size_base_cs];
                }
                else {
                    std::cerr << method << ym_font << " not in font_map" << std::endl;
                }
            }
            if (cspec.contains(day_date_font_cs)) {
                const std::string& dd_font(cspec[day_date_font_cs]);
                auto font_it = font_map.find(dd_font);
                if (font_it != font_map.end()) {
                    day_date_font = font_it->second;
                    if (cspec.contains(day_date_font_size_base_cs))
                        day_date_font_size_base = cspec[day_date_font_size_base_cs];
                }
                else {
                    std::cerr << method << dd_font << " not in font_map" << std::endl;
                }
            }
            int table_flags = default_table_flags;
            if (cspec.contains(table_flags_cs))
                table_flags = cspec[table_flags_cs];
            int combo_flags = default_combo_flags;
            if (cspec.contains(combo_flags_cs))
                combo_flags = cspec[combo_flags_cs];
            std::string& ckey(cspec["cname"].template get<std::string>());
            JSON ymd_old_j = JSON::array();
            ymd_old_j = data[ckey];
            ymd_i[0] = ymd_old_j.at(0);
            ymd_i[1] = ymd_old_j.at(1);
            ymd_i[2] = ymd_old_j.at(2);
            if (ImGui::DatePicker(ckey.c_str(), ymd_i, combo_flags, table_flags, year_month_font, day_date_font, year_month_font_size_base, day_date_font_size_base)) {
                JSON ymd_new_j = JSON::array();
                ymd_new_j.push_back(ymd_i[0]);
                ymd_new_j.push_back(ymd_i[1]);
                ymd_new_j.push_back(ymd_i[2]);
                data[ckey] = ymd_new_j;
                notify_server(ckey, ymd_old_j, ymd_new_j);
            }
    }

    void render_text(JSON& w) {
        const static char* method = "NDContext::render_text: ";

        if (!w.contains(cspec_cs)) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        std::string& rtext = cspec["text"].template get<std::string>();
        ImGui::Text(rtext.c_str());
    }

    void render_button(JSON& w) {
        const static char* method = "NDContext::render_button: ";

        if (!w.contains("cspec")) {
            std::cerr << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        JSON& cspec = w["cspec"];
        if (!cspec.contains("text")) {
            std::cerr << method << "no text in cspec(" << cspec << ")" << std::endl;
            return;
        }
        const std::string& button_text = cspec["text"];
        if (ImGui::Button(button_text.c_str())) {
            action_dispatch(button_text, "Button");
        }
    }

#define SMRY_COLM_CNT 12

    void render_duck_table_summary_modal(JSON& w) {
        const static char* method = "NDContext::render_duck_table_summary_modal: ";
        static int default_summary_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
        static int default_window_flags = ImGuiWindowFlags_AlwaysAutoResize;

        const static char* colm_names[SMRY_COLM_CNT] = {
            "name", "type", // DUCKDB_TYPE_VARCHAR, DUCKDB_TYPE_VARCHAR, 
            "min", "max",   // DUCKDB_TYPE_VARCHAR, DUCKDB_TYPE_VARCHAR,
            "apxu", "avg",  // DUCKDB_TYPE_BIGINT, DUCKDB_TYPE_DOUBLE,
            "std", "q25",   // DUCKDB_TYPE_DOUBLE, DUCKDB_TYPE_VARCHAR,
            "q50", "q75",   // DUCKDB_TYPE_VARCHAR, DUCKDB_TYPE_VARCHAR,
            "cnt", "null"   // DUCKDB_TYPE_BIGINT, DUCKDB_TYPE_DECIMAL
        };

        if (!w.contains(cspec_cs) || !w[cspec_cs].contains(cname_cs) || !w[cspec_cs].contains(title_cs)) {
            std::cerr << method << "bad cspec in: " << w << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        const std::string& cname(cspec[cname_cs]);
        const std::string& title(cspec[title_cs].empty() ? cname : cspec[title_cs]);

        int table_flags = default_summary_table_flags;
        if (cspec.contains(table_flags_cs)) {
            table_flags = cspec[table_flags_cs];
        }

        int window_flags = default_window_flags;
        if (cspec.contains(window_flags_cs)) {
            window_flags = cspec[window_flags_cs];
        }

        bool title_pop = false;
        if (cspec.contains(title_font_cs))
            title_pop = push_font(w, title_font_cs, title_font_size_base_cs);

        ImGui::OpenPopup(title.c_str());
        // Always center this window when appearing
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (!vp) {
            std::cerr << method << cname << ": null viewport ptr!" << std::endl;
            return;
        }
        auto center = vp->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5, 0.5 });

        int colm_count = SMRY_COLM_CNT;
        int colm_index = 0;
        duckdb_type colm_type = DUCKDB_TYPE_INVALID;
        duckdb_logical_type colm_type_l;
        uint64_t* colm_validity = nullptr;
        char buf[32];
        char fmtbuf[16];
        bool body_pop = false;
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, window_flags)) {
            if (title_pop) pop_font();
            if (cspec.contains(body_font_cs))
                body_pop = push_font(w, body_font_cs, body_font_size_base_cs);

            JSON chunk_lo_hi = JSON::array();
            chunk_lo_hi = data[cname];
            std::cout << method << "chunk_ptr: " << chunk_lo_hi << std::endl;
            std::uint64_t chunk_lo = chunk_lo_hi[0];
            std::uint64_t chunk_hi = static_cast<uint64_t>(chunk_lo_hi[1]) << 32;
            std::uint64_t chunk_ptr = chunk_hi + chunk_lo;
            std::cout << method << "chunk_ptr: " << std::hex << chunk_hi << "," << chunk_lo << std::endl;
            std::cout << method << "chunk_ptr: " << chunk_ptr << std::endl;
            duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(chunk_ptr);
            if (!chunk) {
                std::cerr << method << cname << ": null chunk!" << std::endl;
                return;
            }
            if (ImGui::BeginTable(cname.c_str(), colm_count, table_flags)) {
                for (colm_index = 0; colm_index < colm_count; colm_index++) {
                    ImGui::TableSetupColumn(colm_names[colm_index]);
                }
                ImGui::TableHeadersRow();
                auto row_count = duckdb_data_chunk_get_size(chunk);
                int16_t* sidata = nullptr;
                int32_t* idata = nullptr;
                int64_t* bidata = nullptr;
                duckdb_hugeint* hidata = nullptr;
                double* dbldata = nullptr;
                duckdb_string_t* vcdata = nullptr;
                duckdb_type decimal_type;
                uint8_t decimal_width = 0;
                uint8_t decimal_scale = 0;
                double  decimal_divisor = 1.0;
                double  decimal_value = 0.0;
                for (int row_index = 0; row_index < row_count; row_index++) {
                    ImGui::TableNextRow();
                    for (colm_index = 0; colm_index < colm_count; colm_index++) {
                        ImGui::TableSetColumnIndex(colm_index);
                        duckdb_vector colm = duckdb_data_chunk_get_vector(chunk, colm_index);
                        colm_validity = duckdb_vector_get_validity(colm);
                        duckdb_logical_type colm_type_l = duckdb_vector_get_column_type(colm);
                        duckdb_type colm_type = duckdb_get_type_id(colm_type_l);

                        if (duckdb_validity_row_is_valid(colm_validity, row_index)) {
                            switch (colm_type) {
                            case DUCKDB_TYPE_VARCHAR:
                                vcdata = (duckdb_string_t*)duckdb_vector_get_data(colm);
                                if (duckdb_string_is_inlined(vcdata[row_index])) {
                                    // if inlined is 12 chars, there will be no zero terminator
                                    memcpy(buf, vcdata[row_index].value.inlined.inlined, vcdata[row_index].value.inlined.length);
                                    buf[vcdata[row_index].value.inlined.length] = 0;
                                    ImGui::TextUnformatted(buf);
                                }
                                else {
                                    // NB ptr arithmetic using length to calc the end
                                    // ptr as these strings may be packed without 0 terminators
                                    ImGui::TextUnformatted(vcdata[row_index].value.pointer.ptr,
                                        vcdata[row_index].value.pointer.ptr + vcdata[row_index].value.pointer.length);
                                }
                                break;
                            case DUCKDB_TYPE_BIGINT:
                                bidata = (int64_t*)duckdb_vector_get_data(colm);
                                sprintf(buf, "%I64d", bidata[row_index]);
                                ImGui::TextUnformatted(buf);
                                break;
                            case DUCKDB_TYPE_DOUBLE:
                                dbldata = (double_t*)duckdb_vector_get_data(colm);
                                sprintf(buf, "%f", dbldata[row_index]);
                                ImGui::TextUnformatted(buf);
                                break;
                            case DUCKDB_TYPE_DECIMAL:
                                decimal_width = duckdb_decimal_width(colm_type_l);
                                decimal_scale = duckdb_decimal_scale(colm_type_l);
                                decimal_type = duckdb_decimal_internal_type(colm_type_l);
                                decimal_divisor = pow(10, decimal_scale);

                                switch (decimal_type) {
                                case DUCKDB_TYPE_SMALLINT:  // int16_t
                                    sidata = (int16_t*)duckdb_vector_get_data(colm);
                                    decimal_value = (double)sidata[row_index] / decimal_divisor;
                                    break;
                                case DUCKDB_TYPE_INTEGER:   // int32_t
                                    idata = (int32_t*)duckdb_vector_get_data(colm);
                                    decimal_value = (double)idata[row_index] / decimal_divisor;
                                    break;
                                case DUCKDB_TYPE_BIGINT:    // int64_t
                                    bidata = (int64_t*)duckdb_vector_get_data(colm);
                                    decimal_value = (double)bidata[row_index] / decimal_divisor;
                                    break;
                                case DUCKDB_TYPE_HUGEINT:   // duckdb_hugeint: 128
                                    hidata = (duckdb_hugeint*)duckdb_vector_get_data(colm);
                                    decimal_value = duckdb_hugeint_to_double(hidata[row_index]) / decimal_divisor;
                                    break;
                                }
                                // decimal scale 2 means we want "%.2f"
                                // NB %% is the "escape sequence" for %
                                // in a printf format, not \%.
                                sprintf(fmtbuf, "%%.%df", decimal_scale);
                                sprintf(buf, fmtbuf, decimal_value);
                                ImGui::TextUnformatted(buf);
                                break;
                            }
                        }
                        else {
                            ImGui::TextUnformatted(null_cs);
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
        }
        if (body_pop) pop_font();

        bool button_pop = false;
        if (cspec.contains(button_font_cs))
            button_pop = push_font(w, button_font_cs, button_font_size_base_cs);

        // Note we do not invoke pop_widget() here as we're
        // rendering, and that would change the stack while
        // the topmost render method is iterating over it.
        // Instead we add it to the list of pending_pops
        // so top level render will invoke pop_widget for us.
        if (ImGui::Button(ok_cs)) {
            ImGui::CloseCurrentPopup();
            pending_pops.push_back(duck_table_summary_modal_cs);
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button(cancel_cs)) {
            ImGui::CloseCurrentPopup();
            pending_pops.push_back(duck_table_summary_modal_cs);
        }

        if (button_pop) pop_font();

        ImGui::EndPopup();
    }

    void render_duck_parquet_loading_modal(JSON& w) {
        const static char* method = "NDContext::render_duck_parquet_loading_modal: ";

        static ImVec2 position = { 0.5, 0.5 };

        if (!w.contains(cspec_cs) || !w[cspec_cs].contains(cname_cs) || !w[cspec_cs].contains(title_cs)) {
            std::cerr << method << "bad cspec in: " << w << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        const std::string& cname_cache_addr(cspec[cname_cs]);
        const std::string& title(cspec[title_cs].empty() ? cname_cache_addr : cspec[title_cs]);

        bool tpop = false;
        if (cspec.contains(title_font_cs))
            tpop = push_font(w, title_font_cs, title_font_size_base_cs);

        ImGui::OpenPopup(title.c_str());

        // Always center this window when appearing
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (!vp) {
            std::cerr << method << "cname: " << cname_cache_addr
                << ", title: " << title << ", null viewport ptr!";
        }
        auto center = vp->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, position);

        // Get the parquet url list
        auto pq_urls = data[cname_cache_addr];
        // std::cout << "render_duck_parquet_loading_modal: urls: " << pq_urls << std::endl;

        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            bool bpop = false;
            if (cspec.contains(body_font_cs))
                bpop = push_font(w, body_font_cs, body_font_size_base_cs);
            for (int i = 0; i < pq_urls.size(); i++) ImGui::Text(pq_urls[i].get<std::string>().c_str());
            if (bpop) pop_font();
            int spinner_radius = 5;
            int spinner_thickness = 2;
            if (cspec.contains(spinner_radius_cs))
                spinner_radius = cspec[spinner_radius_cs];
            if (cspec.contains(spinner_thickness_cs))
                spinner_thickness = cspec[spinner_thickness_cs];
            if (!ImGui::Spinner("parquet_loading_spinner", spinner_radius, spinner_thickness, 0)) {
                // TODO: spinner always fails IsClippedEx on first render
                std::cerr << "render_duck_parquet_loading_modal: spinner fail" << std::endl;
            }
            ImGui::EndPopup();
        }
        if (tpop) pop_font();
    }

    void render_table(JSON& w) {}

    void render_push_font(JSON& w) {
        push_font(w, font_cs, font_size_base_cs);
    }

    void render_pop_font(JSON& w) {
        pop_font();
    }

    void render_begin_child(JSON& w) {
        const static char* method = "NDContext::render_begin_child: ";

        // NB BeginChild is a grouping mechahism, so there's no single
        // cache datum to which we refer, so no cname. However, we do look
        // require a title (for imgui ID purposes) and we can have styling
        // attributes like ImGuiChildFlags
        if (!w.contains(cspec_cs) || !w[cspec_cs].contains(title_cs)) {
            std::cerr << method << "bad cspec in: " << w << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        const std::string& title(cspec[title_cs]);

        int child_flags = 0;
        if (cspec.contains(child_flags_cs)) {
            child_flags = cspec[child_flags_cs];
        }
        ImVec2 size{ 0, 0 };
        if (cspec.contains(size_cs)) {
            nlohmann::json size_tup2 = nlohmann::json::array();
            size_tup2 = cspec[size_cs];
            size[0] = size_tup2.at(0);
            size[1] = size_tup2.at(1);
        }
        ImGui::BeginChild(title.c_str(), size, child_flags);
    }

    void render_end_child(JSON& w) {
        ImGui::EndChild();
    }

    void render_begin_group(JSON& w) {
        ImGui::BeginGroup();
    }

    void render_end_group(JSON& w) {
        ImGui::EndGroup();
    }

    void push_widget(JSON& w) {
        stack.push_back(w);
    }

    void pop_widget(const std::string& rname = "") {
        // if rname is empty we pop without checks
        // if rname specifies a class we check the
        //      popped widget rname
        if (!rname.empty()) {
            JSON& w(stack.back());
            if (w["rname"] != rname) {
                std::cerr << "pop mismatch w.rname(" << w["rname"] << ") rname("
                    << rname << ")" << std::endl;
            }
            stack.pop_back();
        }
    }

    bool push_font(JSON& w, const char* font_attr,
        const char* font_size_base_attr) {
        const static char* method = "NDContext::push_font: ";

        if (!w.contains(cspec_cs)) {
            std::cerr << method << "bad cspec in: " << w << std::endl;
            return false;
        }
        const JSON& cspec(w[cspec_cs]);
        if (!cspec.contains(font_attr)) {
            std::cerr << method << "no " << font_attr << " in cspec : " << w << std::endl;
            return false;
        }
        bool pop_font = false;
        float font_size_base = 0.0;
        const std::string& font_name = cspec[font_attr];
        auto font_it = font_map.find(font_name);
        if (font_it != font_map.end()) {
            if (cspec.contains(font_size_base_attr)) {
                font_size_base = cspec[font_size_base_attr];
            }
            ImGui::PushFont(font_it->second, font_size_base);
        }
        else {
            std::cerr << method << "bad font name in cspec in w(" << w << ")" << std::endl;
            return false;
        }
        return true;
    }

    void pop_font() {
        ImGui::PopFont();
    }

};



