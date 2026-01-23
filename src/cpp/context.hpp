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
#include "db_cache.hpp"
#include "json_ops.hpp"
#include "logger.hpp"

// NDContext: the NoDOM render engine, built on top of Dear ImGui and Emscripten.
// NDContext is coded as portable C++ with two targets: Emscripten (ems) running in 
// Chrome and Win32. Because the two platforms differ some key abstractions
// are in play...
// json_cache.hpp: JSON is the format for data and layout. On win32 we use 
// nlohmann::json, and on ems emscripten::val. json_cache.hpp uses template
// functions to bridge the difference, keeping the code here identical.
// websock.hpp: on win32 we use websocketpp, and on ems we use the ems 
// wesock API. See emscripten/websocket.h and 
// https://gist.github.com/nus/564e9e57e4c107faa1a45b8332c265b9

static constexpr int ND_MAX_COMBO_LIST{ 16 };

struct GLFWwindow;

template <typename JSON, typename DB>
class NDContext {
private:
    // ref to "server process"; just an abstraction of EMS vs win32
    NDProxy<DB>&        proxy;  // DB proxy decouples NDContext from DB detail
    JSON                layout; // layout and data are fetched by 
    JSON                data;   // sync c++py calls not HTTP gets
    std::deque<JSON>    stack;  // render stack

    // map layout render func names to the actual C++ impls
    std::unordered_map<std::string, std::function<void(const JSON& w)>> rfmap;

    // top level layout widgets with widget_id eg modals are in pushables
    std::unordered_map<std::string, JSON> pushable;
    // action_dispatch is called while rendering, and changes
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
    std::uint32_t font_push_count = 0;
    std::uint32_t font_pop_count = 0;
    std::uint32_t render_count = 0;
    std::deque<std::string> bad_font_pushes;

    WebSockSenderFunc ws_send = nullptr;  // ref to NDWebSockClient::send
    MessagePumpFunc msg_pump = nullptr;
    GLFWwindow* glfw_window = nullptr;

public:
    NDContext(NDProxy<DB>& s)
        :proxy(s), red{ 255, 51, 0 },
            green{ 102, 153, 0 }, 
                amber{ 255, 153, 0 } {
        // init status is not connected
        db_status_color = red;

        rfmap.emplace(std::string("Home"), [this](const JSON& w) { render_home(w); });
        rfmap.emplace(std::string("InputInt"), [this](const JSON& w) { render_input_int(w); });
        rfmap.emplace(std::string("Combo"), [this](const JSON& w) { render_combo(w); });
        rfmap.emplace(std::string("Checkbox"), [this](const JSON& w) { render_checkbox(w); });
        rfmap.emplace(std::string("Separator"), [this](const JSON& w) { render_separator(w); });
        rfmap.emplace(std::string("Footer"), [this](const JSON& w) { render_footer(w); });
        rfmap.emplace(std::string("SameLine"), [this](const JSON& w) { render_same_line(w); });
        rfmap.emplace(std::string("DatePicker"), [this](const JSON& w) { render_date_picker(w); });
        rfmap.emplace(std::string("Text"), [this](const JSON& w) { render_text(w); });
        rfmap.emplace(std::string("Button"), [this](const JSON& w) { render_button(w); });
        rfmap.emplace(std::string("DuckTableSummaryModal"), [this](const JSON& w) { render_duck_table_summary_modal(w); });
        rfmap.emplace(std::string("DuckParquetLoadingModal"), [this](const JSON& w) { render_duck_parquet_loading_modal(w); });
        rfmap.emplace(std::string("Table"), [this](const JSON& w) { render_table(w); });
        rfmap.emplace(std::string("PushFont"), [this](const JSON& w) { render_push_font(w); });
        rfmap.emplace(std::string("PopFont"), [this](const JSON& w) { render_pop_font(w); });
        rfmap.emplace(std::string("BeginChild"), [this](const JSON& w) { render_begin_child(w); });
        rfmap.emplace(std::string("EndChild"), [this](const JSON& w) { render_end_child(w); });
        rfmap.emplace(std::string("BeginGroup"), [this](const JSON& w) { render_begin_group(w); });
        rfmap.emplace(std::string("EndGroup"), [this](const JSON& w) { render_end_group(w); });
    }

    GLFWwindow* get_glfw_window() { return glfw_window; }
    void        set_glfw_window(GLFWwindow* w) { glfw_window = w; }

    // invoked by main loop
    void render() {
        start_render_cycle();

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
        // That's why modals are dealt with by pending_pushes/pops so they can
        // push/pop outside the context of the loop below. JOS 2025-01-31
        // And we avoid live iterators so far down the stack too...
        int stack_length = (int)stack.size();
        for (int inx = 0; inx < stack_length; inx++) {
            // deref it for clarity and to rename as widget for
            // cross ref with main.ts logic
            const JSON& widget(stack[inx]);    // not {} to avoid list inits
            dispatch_render(widget);
        }
        end_render_cycle();
    }

    void start_render_cycle() {
        const static char* method = "NDContext::start_render_cycle: ";
        // Zero the font push/pop counts before rendering. This
        // enables us to detect lopsided push/pop sequences after
        // all widgets have rendered.
        font_push_count = 0;
        font_pop_count = 0;
        bad_font_pushes.clear();

        if (pending_pops.size() || pending_pushes.size()) {
            NDLogger::cerr() << method << pending_pops.size() << " pending pops, " << pending_pushes.size()
                << " pending pushes" << std::endl;
        }
    }

    void end_render_cycle() {
        const static char* method = "NDContext::end_render_cycle: ";
        if (font_push_count != font_pop_count) {
            NDLogger::cerr() << method << "font_push_count: " << font_push_count
                << ", font_pop_count: " << font_pop_count << std::endl;
        }
        bad_font_pushes.clear();
        render_count++;
    }

    void server_request(const std::string& key) {
        std::stringstream msgbuf;
        msgbuf << "{ \"" << nd_type_cs << "\":\"" << cache_request_cs << "\",\""
            << cache_key_cs << "\":\"" << key << "\"}";
        ws_send(msgbuf.str());
    }

    void notify_server(const std::string& caddr, JSON& old_val, JSON& new_val) {
        const static char* method = "NDContext::notify_server: ";

        std::cout << method << caddr << ", old: " << old_val << ", new: " << new_val << std::endl;

        std::stringstream msgbuf;
        // We supply wrapping "" for data_change_cs and caddr
        // as we know they're strings. For new_val and old_val
        // we rely on operator<< which is supplied by
        // nlohmann::json. For emscripten::val we use our own
        // operator<< from db_cache.hpp.
        msgbuf << "{ \"" << nd_type_cs << "\":\"" << data_change_cs << "\",\""
            << cache_key_cs << "\":\"" << caddr << "\",\""
            // 
            << new_value_cs << "\":" << new_val << ",\""
            << old_value_cs << "\":" << old_val << "}";
        ws_send(msgbuf.str());
    }

    void dispatch_server_responses(std::queue<JSON>& responses) {
        const static char* method = "NDContext::dispatch_server_responses: ";

        // server_changes will be a list of json obj copied out of a pybind11
        // list of py dicts. So use C++11 auto range...
        while (!responses.empty()) {
            JSON& resp = responses.front();
            std::string nd_type(JAsString(resp, nd_type_cs));
            NDLogger::cout() << method << nd_type << std::endl;
            // polymorphic as types are hidden inside change
            // Is this a CacheResponse for layout or data?
            if (nd_type == cache_response_cs) {
                if (JAsString(resp, cache_key_cs) == data_cs) {
                    data = resp[value_cs];
                }
                else if (JAsString(resp, cache_key_cs) == layout_cs) {
                    layout = resp[value_cs];
                    on_layout();
                }
            }
            // Is this a DataChange?
            else if (nd_type == data_change_cs) {
                std::string ckey = JAsString(resp, cache_key_cs);
                JSet(data, ckey.c_str(), resp[new_value_cs]);
            }
            else if (nd_type == data_change_confirmed_cs) {
                // TODO: add check that type has not mutated
            }
            // Duck or Parquet event...
            else {
                on_db_event(resp);
            }
            responses.pop();
        }
    }

    bool db_app() { return proxy.db_app(); }
    void set_done(bool d) { proxy.set_done(d); }

    void on_ws_open() {
        server_request("data");
        server_request("layout");
    }

    void on_db_event(const JSON& db_msg) {
        const static char* method = "NDContext::on_db_event: ";

        if (!JContains(db_msg, nd_type_cs)) {
            NDLogger::cerr() << method << "no nd_type in " << db_msg << std::endl;
            return;
        }
        std::string nd_type = JAsString(db_msg, nd_type_cs);
        std::string qid = JAsString(db_msg, query_id_cs);
        NDLogger::cout() << method << nd_type << ", QID: " << qid << std::endl;

        if (nd_type == parquet_scan_cs) {
            db_status_color = amber;
        }
        else if (nd_type == query_cs) {
            db_status_color = amber;
        }
        else if (nd_type == parquet_scan_result_cs) {
            db_status_color = green;
            action_dispatch(qid, nd_type);
        }
        else if (nd_type == query_result_cs) {
            db_status_color = green;
            // Typically, a QueryResult is followed by dispatch
            // of a BatchRequest
            action_dispatch(qid, nd_type);
        }
        else if (nd_type == batch_response_cs) {
            db_status_color = green;
        }
        else if (nd_type == "DuckInstance") {
            // TODO: q processing order means this doesn't happen so early in cpp
            // main.ts:on_duck_event invokes check_duck_module.
            // However, we don't need all the check_duck_module JS module stuff,
            // so we can just flip status button color here
            db_status_color = amber;
            // send a test query
            db_dispatch("Query", "ramanujan", "select 1729;");
        }
        else {
            NDLogger::cerr() << "NDContext::on_db_event: unexpected nd_type in " << db_msg << std::endl;
        }
    }

    void on_layout() {
        const static char* method = "NDContext::on_layout: ";
        // layout is a list of widgets; and all may have children
        // however, not all widgets are children. For instance modals
        // like parquet_loading_modal have to be explicitly pushed
        // on to the render stack by an event. JOS 2025-01-31
        // Fonts appear in layout now. JOS 2025-07-29
        int layout_length = JSize(layout);
        for (int inx=0; inx < layout_length; inx++) {
            const JSON& w(layout[inx]);
            NDLogger::cout() << method << "CHILD: " << w << std::endl;
            if (JContains(w, widget_id_cs)) {
                std::string widget_id = JAsString(w, widget_id_cs);
                if (!widget_id.empty()) {
                    NDLogger::cout() << method << "PUSHABLE: " << widget_id << ":" << w << std::endl;
                    pushable[widget_id] = w;
                }
                else {
                    NDLogger::cerr() << method << "empty widget_id in: " << w << std::endl;
                }
            }
        }
        // Home on the render stack
        stack.push_back(layout[0]);
    }

#ifndef __EMSCRIPTEN__
    JSON  get_breadboard_config() { return proxy.get_breadboard_config(); }
#endif
    void register_font(const std::string& name, ImFont* f) { font_map[name] = f; }
    void register_ws_sender(WebSockSenderFunc send) { ws_send = send; }
    void register_msg_pump(MessagePumpFunc mpf) { msg_pump = mpf; }
    void pump_messages() { msg_pump(); }

protected:
    // w["rname"] resolve & invoke
    void dispatch_render(const JSON& w) {
        const static char* method = "NDContext::dispatch_render: ";

        if (!JContains(w, rname_cs)) {
            NDLogger::cerr() << method << "missing rname in " << w << std::endl;
            return;
        }
        std::string rname = JAsString(w, rname_cs);
        const auto it = rfmap.find(rname);
        if (it == rfmap.end()) {
            NDLogger::cerr() << method << "unknown rname in " << w << std::endl;
            return;
        }
        it->second(w);
    }

    void action_dispatch(const std::string& action, const std::string& nd_event) {
        const static char* method = "NDContext::action_dispatch: ";

        NDLogger::cout() << method << "action(" << action << ")" << std::endl;

        if (action.empty()) {
            NDLogger::cerr() << method << "no action specified!" << std::endl;
            return;
        }
        // Is it a pushable widget? NB this is only for self popping modals like DuckTableSummaryModal.
        // In this case we expect nd_event to be empty as we're not driven directly by the event, but
        // by ui_push and ui_pop action qualifiers associated with the nd_event list. JOS 2025-02-21
        // JOS 2025-02-22
        auto it = pushable.find(action);
        if (it != pushable.end() && nd_event.empty()) {
            NDLogger::cout() << method << "pushable(" << action << ")" << std::endl;
            stack.push_back(pushable[action]);
        }
        else {
            if (!JContains(data, actions_cs)) {
                NDLogger::cerr() << method << "no actions in data!" << std::endl;
                return;
            }
            // get hold of "actions" in data: do we have one matching action?
            const JSON& actions(data[actions_cs]);
            if (!JContains(actions, action.c_str())) {
                NDLogger::cerr() << method << "no actions." << action << " in data!" << std::endl;
                return;
            }
            const JSON& action_defn(actions[action]);
            if (!JContains(action_defn, nd_events_cs)) {
                NDLogger::cerr() << method << "no nd_events in actions." << action << " in data!" << std::endl;
                return;
            }
            std::vector<std::string> nd_events_vec;
            JAsStringVec(action_defn, nd_events_cs, nd_events_vec);
            auto nd_events_iter = std::find(nd_events_vec.begin(), nd_events_vec.end(), nd_event);
            bool event_match = nd_events_iter != nd_events_vec.end();
            if (!event_match) {
                NDLogger::cerr() << method << "no match for nd_event(" << nd_event << ") in defn(" << action_defn << ") in data!" << std::endl;
                return;
            }
            // Now we have a matched action definition in hand we can look
            // for UI push/pop and DB scan/query. If there's both push and pop,
            // pop goes first naturally!
            if (JContains(action_defn, ui_pop_cs)) {
                // for pops we supply the rname, not the pushable name so
                // the context can check the widget type on pops
                std::string rname(JAsString(action_defn, ui_pop_cs));
                NDLogger::cout() << method << "ui_pop(" << rname << ")" << std::endl;
                pending_pops.push_back(rname);
            }
            if (JContains(action_defn, ui_push_cs)) {
                // for pushes we supply widget_id, not the rname
                std::string widget_id(JAsString(action_defn, ui_push_cs));
                // salt'n'pepa in da house!
                auto push_it = pushable.find(widget_id);
                if (push_it != pushable.end()) {
                    NDLogger::cout() << method << "ui_push(" << widget_id << ")" << std::endl;
                    // NB action_dispatch is called by eg render_button, which ultimately is called
                    // by render(), which iterates over stack. So we cannot change stack here...
                    pending_pushes.push_back(push_it->second);
                }
                else {
                    NDLogger::cerr() << method << "ui_push(" << widget_id << ") no such pushable" << std::endl;
                }
            }
            // Finally, do we have a DB op to handle?
            if (JContains(action_defn, db_cs)) {
                const JSON& db_op(action_defn[db_cs]);
                if (!JContains(db_op, query_id_cs) || !JContains(db_op, action_cs)) {
                    NDLogger::cerr() << method << "db(" << db_op << ") missing query_id|action!" << std::endl;
                    return;
                }
                std::string db_action(JAsString(db_op, action_cs));
                std::string query_id(JAsString(db_op, query_id_cs));
                if (db_action == batch_request_cs) {
                    db_dispatch(db_action, query_id);
                }
                else {
                    if (!JContains(db_op, sql_cname_cs)) {
                        NDLogger::cerr() << method << "db(" << db_op << ") missing sql_cname" << std::endl;
                        return;
                    }
                    std::string sql_cache_key(JAsString(db_op, sql_cname_cs));
                    if (!JContains(data, sql_cache_key.c_str())) {
                        NDLogger::cerr() << method << "db(" << db_op << ") sql_cname(" << sql_cache_key << ") does not resolve!" << std::endl;
                        return;
                    }
                    else {
                        std::string sql(JAsString(data, sql_cache_key.c_str()));
                        db_dispatch(db_action, query_id, sql);
                    }
                }
            }
        }
    }

    void db_dispatch(const std::string& nd_type, const std::string& qid, 
                    const std::string& sql) {
        auto db_request = JNewObject();
        JSet(db_request, nd_type_cs, nd_type.c_str());
        JSet(db_request, sql_cs, sql.c_str());
        JSet(db_request, query_id_cs, qid.c_str());
        proxy.db_dispatch(db_request);
    }

    void db_dispatch(const std::string& nd_type, const std::string& qid) {
        auto db_request = JNewObject();
        JSet(db_request, nd_type_cs, nd_type.c_str());
        JSet(db_request, query_id_cs, qid.c_str());
        proxy.db_dispatch(db_request);
    }

    // Render functions
    void render_home(const JSON& w) {
        const static char* method = "NDContext::render_home: ";

        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec = w[cspec_cs];
        std::string title(nodom_cs);
        if (JContains(cspec, title_cs)) {
            title = JAsString(cspec, title_cs);
        }

        bool fpop = false;
        if (JContains(cspec, title_font_cs))
            fpop = push_font(w, title_font_cs, title_font_size_base_cs);

        ImGui::Begin(title.c_str());
        const JSON& children = w[children_cs];
        int child_count = JSize(children);
        for (int inx=0; inx < child_count; inx++) {
            const JSON& child(children[inx]);
            dispatch_render(child);
        }
        if (fpop) {
            pop_font();
        }
        ImGui::End();
    }

    void render_input_int(const JSON& w) {
        const static char* method = "NDContext::render_input_int: ";
        // static storage: imgui wants int (int32), nlohmann::json uses int64_t
        static int input_integer;
        input_integer = 0;
        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        std::string label;
        if (JContains(cspec, text_cs)) label = JAsString(cspec, text_cs);
        // params by value
        int step = 1;
        if (JContains(cspec, step_cs)) step = JAsInt(cspec, step_cs);
        int step_fast = 1;
        if (JContains(cspec, step_fast_cs)) step_fast = JAsInt(cspec, step_fast_cs);
        int flags = 0;
        if (JContains(cspec, flags_cs)) flags = JAsInt(cspec, flags_cs);
        // one param by ref: the int itself
        std::string cname_cache_addr = JAsString(cspec, cname_cs);
        // if no label use cache addr
        if (!label.size()) label = cname_cache_addr;
        // local static copy of cache val
        int old_val = input_integer = JAsInt(data, cname_cache_addr.c_str());
        // imgui has ptr to copy of cache val
        ImGui::InputInt(label.c_str(), &input_integer, step, step_fast, flags);
        // copy local copy back into cache
        if (input_integer != old_val) {
            JSet(data, cname_cache_addr.c_str(), input_integer);
            JSON j_old_val(old_val);
            JSON j_input_int(input_integer);
            notify_server(cname_cache_addr, j_old_val, j_input_int);
        }
    }

    void render_combo(const JSON& w) {
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
        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec = w["cspec"];
        std::string label(nodom_cs);
        int step = 1;
        if (JContains(cspec, text_cs)) label = JAsString(cspec, text_cs);
        if (JContains(cspec, step_cs)) step = JAsInt(cspec, step_cs);
        // no value params in layout here; all combo layout is data cache refs
        // /cspec/cname should give us a data cache addr for the combo list
        std::string combo_list_cache_addr(JAsString(cspec, cname_cs));
        std::string combo_index_cache_addr(JAsString(cspec, index_cs));
        // if no label use cache addr
        if (!label.size()) label = combo_list_cache_addr;
        int combo_count = 0;
        JAsStringVec(data, combo_list_cache_addr.c_str(), combo_list);
        for (auto it = combo_list.begin(); it != combo_list.end(); ++it) {
            cs_combo_list[combo_count++] = it->c_str();
            if (combo_count == ND_MAX_COMBO_LIST) break;
        }
        int old_val = combo_selection = JAsInt(data, combo_index_cache_addr.c_str());
        ImGui::Combo(label.c_str(), &combo_selection, cs_combo_list, combo_count, combo_count);
        if (combo_selection != old_val) {
            JSet(data, combo_index_cache_addr.c_str(), combo_selection);
            // Do not use list_init style ctors with nlohmann::json, 
            // they'll serialize as [old_val], [combo_selection],
            // and we want them to serialize as atomic vals, not lists
            JSON j_old_val(old_val);
            JSON j_combo_selection(combo_selection);
            notify_server(combo_index_cache_addr, j_old_val, j_combo_selection);
        }
    }

    void render_checkbox(const JSON& w) {
        const static char* method = "NDContext::render_checkbox: ";

        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        std::string cname_cache_addr(JAsString(cspec, cname_cs));
        std::string text(cname_cache_addr);
        if (JContains(cspec, text_cs))
            text = JAsString(cspec, text_cs);

        bool checked_value = JAsBool(data, cname_cache_addr.c_str());
        bool old_checked_value = checked_value;

        ImGui::Checkbox(text.c_str(), &checked_value);

        if (checked_value != old_checked_value) {
            JSet(data, cname_cache_addr.c_str(), checked_value);
            JSON j_old_checked_value{ old_checked_value };
            JSON j_checked_value{ checked_value };
            notify_server(cname_cache_addr, j_old_checked_value, j_checked_value);
        }

    }

    void render_separator(const JSON&) {
        ImGui::Separator();
    }

    void render_footer(const JSON& w) {
        static const char* method = "NDContext::render_footer: ";

        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        bool db = JAsBool(cspec, "db");
        bool fps = JAsBool(cspec, "fps");
        // TODO: config demo mode so it can be switched on/off in prod
        bool demo = JAsBool(cspec, "demo");
        bool id_stack = JAsBool(cspec, "id_stack");
        // TODO: understand ems mem anlytics and restore in footer
        // bool memory = w.value(nlohmann::json::json_pointer("/cspec/memory"), true);

        if (db) {
            // Push colour styling for the DB button
            ImGui::PushStyleColor(ImGuiCol_Button, (ImU32)db_status_color);
            if (ImGui::Button("DB")) {
                // TODO: browser tab for DuckDB shell
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

    void render_same_line(const JSON&) {
        ImGui::SameLine();
    }

    void render_date_picker(const JSON& w) {
        const static char* method = "NDContext::render_date_picker: ";

        static int default_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedSame;
        static int default_combo_flags = ImGuiComboFlags_HeightRegular;
        static std::vector<int> ymd_i = { 0, 0, 0 };
        ImFont* year_month_font = nullptr;
        ImFont* day_date_font = nullptr;
        uint32_t year_month_font_size_base = 0;
        uint32_t day_date_font_size_base = 0;

            if (!JContains(w, cspec_cs)) {
                NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
                return;
            }
            const JSON& cspec(w[cspec_cs]);
            if (JContains(cspec, year_month_font_cs)) {
                std::string ym_font(JAsString(cspec, year_month_font_cs));
                auto font_it = font_map.find(ym_font);
                if (font_it != font_map.end()) {
                    year_month_font = font_it->second;
                    if (JContains(cspec, year_month_font_size_base_cs))
                        year_month_font_size_base = JAsInt(cspec, year_month_font_size_base_cs);
                }
                else {
                    // Use Default font installed by im_start instead
                    year_month_font = font_map[default_cs];
                }
            }
            if (JContains(cspec, day_date_font_cs)) {
                std::string dd_font(JAsString(cspec, day_date_font_cs));
                auto font_it = font_map.find(dd_font);
                if (font_it != font_map.end()) {
                    day_date_font = font_it->second;
                    if (JContains(cspec, day_date_font_size_base_cs))
                        day_date_font_size_base = JAsInt(cspec, day_date_font_size_base_cs);
                }
                else {
                    // Use Default font installed by im_start instead
                    day_date_font = font_map[default_cs];
                }
            }
            int table_flags = default_table_flags;
            if (JContains(cspec, table_flags_cs))
                table_flags = JAsInt(cspec, table_flags_cs);
            int combo_flags = default_combo_flags;
            if (JContains(cspec, combo_flags_cs))
                combo_flags = JAsInt(cspec, combo_flags_cs);
            std::string ckey = JAsString(cspec, cname_cs);
            JSON ymd_old_j = JSON::array();
            ymd_old_j = data[ckey];
            ymd_i[0] = JAsInt(ymd_old_j, 0);
            ymd_i[1] = JAsInt(ymd_old_j, 1);
            ymd_i[2] = JAsInt(ymd_old_j, 2);
            if (ImGui::DatePicker(ckey.c_str(), ymd_i.data(), combo_flags, table_flags, year_month_font, day_date_font, year_month_font_size_base, day_date_font_size_base)) {
                auto ymd_new_j = JArray(ymd_i);
                JSet(data, ckey.c_str(), ymd_new_j);
                notify_server(ckey, ymd_old_j, ymd_new_j);
            }
    }

    void render_text(const JSON& w) {
        const static char* method = "NDContext::render_text: ";

        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        std::string rtext = JAsString(cspec, text_cs);
        ImGui::Text(rtext.c_str());
    }

    void render_button(const JSON& w) {
        const static char* method = "NDContext::render_button: ";

        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        if (!JContains(cspec, text_cs)) {
            NDLogger::cerr() << method << "no text in cspec(" << cspec << ")" << std::endl;
            return;
        }
        std::string button_text = JAsString(cspec, text_cs);
        if (ImGui::Button(button_text.c_str())) {
            action_dispatch(button_text, "Button");
        }
    }

    constexpr static int SMRY_COLM_CNT = 12;

    void render_duck_table_summary_modal(const JSON& w) {
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

        if (!JContains(w, cspec_cs) || !JContains(w[cspec_cs], qname_cs) || !JContains(w[cspec_cs], title_cs)) {
            NDLogger::cerr() << method << "bad cspec in: " << w << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        const std::string qname(JAsString(cspec, qname_cs));
        std::string title(qname);
        if (JContains(cspec, title_cs))
            title = JAsString(cspec, title_cs);

        int table_flags = default_summary_table_flags;
        if (JContains(cspec, table_flags_cs)) {
            table_flags = JAsInt(cspec, table_flags_cs);
        }

        int window_flags = default_window_flags;
        if (JContains(cspec, window_flags_cs)) {
            window_flags = JAsInt(cspec, window_flags_cs);
        }

        bool title_pop = false;
        if (JContains(cspec, title_font_cs))
            title_pop = push_font(w, title_font_cs, title_font_size_base_cs);

        ImGui::OpenPopup(title.c_str());
        // Always center this window when appearing
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (!vp) {
            NDLogger::cerr() << method << qname << ": null viewport ptr!" << std::endl;
            return;
        }
        auto center = vp->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5, 0.5 });

        std::uint64_t colm_count = SMRY_COLM_CNT;
        int colm_index = 0;
        bool body_pop = false;
        if (ImGui::BeginPopupModal(title.c_str(), nullptr, window_flags)) {
            if (title_pop) pop_font();
            if (JContains(cspec, (body_font_cs)))
                body_pop = push_font(w, body_font_cs, body_font_size_base_cs);

            std::uint64_t result_handle = proxy.get_handle(qname);
            // NDLogger::cout() << method << "result_handle: " << std::hex << result_handle << std::endl;
            if (!result_handle) {
                NDLogger::cerr() << method << qname << ": null result_handle!" << std::endl;
                return;
            }
            if (ImGui::BeginTable(qname.c_str(), (int)colm_count, table_flags)) {
                for (colm_index = 0; colm_index < colm_count; colm_index++) {
                    ImGui::TableSetupColumn(colm_names[colm_index]);
                }
                ImGui::TableHeadersRow();
                std::uint64_t row_count = proxy.get_row_count(result_handle);
                proxy.get_meta_data(result_handle, colm_count, row_count);
                for (int row_index = 0; row_index < row_count; row_index++) {
                    ImGui::TableNextRow();
                    for (colm_index = 0; colm_index < colm_count; colm_index++) {
                        ImGui::TableSetColumnIndex(colm_index);
                        const char* endchar = proxy.get_datum(result_handle, colm_index, row_index);
                        if (endchar) {
                            ImGui::TextUnformatted(proxy.buffer, endchar);
                        }
                        else {
                            ImGui::TextUnformatted(proxy.buffer);
                        }
                    }
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
        }
        if (body_pop) pop_font();

        bool button_pop = false;
        if (JContains(cspec, button_font_cs))
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

    void render_duck_parquet_loading_modal(const JSON& w) {
        const static char* method = "NDContext::render_duck_parquet_loading_modal: ";

        static ImVec2 position = { 0.5, 0.5 };

        if (!JContains(w, cspec_cs) || !JContains(w[cspec_cs], cname_cs) || !JContains(w[cspec_cs], title_cs)) {
            NDLogger::cerr() << method << "bad cspec in: " << w << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        std::string cname_cache_addr(JAsString(cspec, cname_cs));
        std::string title(cname_cache_addr);
        if (JContains(cspec, title_cs))
            title = JAsString(cspec, title_cs);

        bool tpop = false;
        if (JContains(cspec, title_font_cs))
            tpop = push_font(w, title_font_cs, title_font_size_base_cs);

        ImGui::OpenPopup(title.c_str());

        // Always center this window when appearing
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (!vp) {
            NDLogger::cerr() << method << "cname: " << cname_cache_addr
                << ", title: " << title << ", null viewport ptr!";
        }
        auto center = vp->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, position);

        // Get the parquet url list
        std::vector<std::string> pq_url_vec;
        JAsStringVec(data, cname_cache_addr.c_str(), pq_url_vec);

        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            bool bpop = false;
            if (JContains(cspec, body_font_cs))
                bpop = push_font(w, body_font_cs, body_font_size_base_cs);
            for (auto pq_iter = pq_url_vec.begin(); pq_iter != pq_url_vec.end(); ++pq_iter) {
                ImGui::Text((*pq_iter).c_str());
            }
            if (bpop) pop_font();
            int spinner_radius = 5;
            int spinner_thickness = 2;
            if (JContains(cspec, spinner_radius_cs))
                spinner_radius = JAsInt(cspec, spinner_radius_cs);
            if (JContains(cspec, spinner_thickness_cs))
                spinner_thickness = JAsInt(cspec, spinner_thickness_cs);
            if (!ImGui::Spinner("parquet_loading_spinner", (float)spinner_radius, spinner_thickness, 0)) {
                // TODO: spinner always fails IsClippedEx on first render
                NDLogger::cerr() << "render_duck_parquet_loading_modal: spinner fail" << std::endl;
            }
            ImGui::EndPopup();
        }
        if (tpop) pop_font();
    }

    void render_table(const JSON&) {}

    void render_push_font(const JSON& w) {
        push_font(w, font_cs, font_size_base_cs);
    }

    void render_pop_font(const JSON&) {
        pop_font();
    }

    void render_begin_child(const JSON& w) {
        const static char* method = "NDContext::render_begin_child: ";

        // NB BeginChild is a grouping mechahism, so there's no single
        // cache datum to which we refer, so no cname. However, we do look
        // require a title (for imgui ID purposes) and we can have styling
        // attributes like ImGuiChildFlags
        if (!JContains(w, cspec_cs) || !JContains(w[cspec_cs], title_cs)) {
            NDLogger::cerr() << method << "bad cspec in: " << w << std::endl;
            return;
        }
        const JSON& cspec(w[cspec_cs]);
        std::string title(JAsString(cspec, title_cs));

        int child_flags = 0;
        if (JContains(cspec, child_flags_cs)) {
            child_flags = JAsInt(cspec, child_flags_cs);
        }
        ImVec2 size{ 0, 0 };
        if (JContains(cspec, size_cs)) {
            JSON size_tup2 = JSON::array();
            size_tup2 = cspec[size_cs];
            size[0] = (float)JAsInt(size_tup2, 0);
            size[1] = (float)JAsInt(size_tup2, 1);
        }
        ImGui::BeginChild(title.c_str(), size, child_flags);
    }

    void render_end_child(const JSON&) {
        ImGui::EndChild();
    }

    void render_begin_group(const JSON&) {
        ImGui::BeginGroup();
    }

    void render_end_group(const JSON&) {
        ImGui::EndGroup();
    }

    void push_widget(const JSON& w) {
        stack.push_back(w);
    }

    void pop_widget(const std::string& rname = "") {
        // if rname is empty we pop without checks
        // if rname specifies a class we check the
        //      popped widget rname
        if (!rname.empty()) {
            const JSON& w(stack.back());
            std::string wrname = JAsString(w, rname_cs);
            if (wrname != rname) {
                NDLogger::cerr() << "pop mismatch w.rname(" << wrname << ") rname("
                    << rname << ")" << std::endl;
            }
            stack.pop_back();
        }
    }

    bool push_font(const JSON& w, const char* font_attr,
        const char* font_size_base_attr) {
        const static char* method = "NDContext::push_font: ";

        if (!JContains(w, cspec_cs)) {
            NDLogger::cerr() << method << "bad cspec in: " << w << std::endl;
            return false;
        }
        const JSON& cspec(w[cspec_cs]);
        if (!JContains(cspec, font_attr)) {
            NDLogger::cerr() << method << "no " << font_attr << " in cspec : " << w << std::endl;
            return false;
        }
        float font_size_base = 0.0;
        std::string font_name = JAsString(cspec, font_attr);
        if (JContains(cspec, font_size_base_attr)) {
            font_size_base = JAsFloat(cspec, font_size_base_attr);
        }
        auto font_it = font_map.find(font_name);
        if (font_it != font_map.end()) {
            ImGui::PushFont(font_it->second, font_size_base);
        }
        else {
            // Gotta push something as matching PopFont
            // renders will trigger imgui asserts
            ImFont* default_font = font_map[default_cs];
            ImGui::PushFont(default_font, font_size_base);
            bad_font_pushes.push_back(font_name);
        }
        font_push_count++;
        return true;
    }

    void pop_font() {
        ImGui::PopFont();
        font_pop_count++;
    }

};
