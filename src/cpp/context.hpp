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
static constexpr int act_ev_key_buf_len{ 256 };
struct GLFWwindow;

// NDContext helper NextEvent: what comes next in a 
// DB action_dispatch sequence?
const char* NextEvent(const char* nd_event) {
    if (strcmp(nd_event, Static::command_cs) == 0) {
        return Static::command_result_cs;
    }
    if (strcmp(nd_event, Static::query_cs) == 0) {
        return Static::query_result_cs;
    }
    if (strcmp(nd_event, Static::batch_request_cs) == 0) {
        return Static::batch_response_cs;
    }
    return nullptr;
}

// NDContext helper AtomicCacheValue: present a common internal API 
// for fastpath wasm vars and slowpath JSON vars.
template <typename A, typename JSON>
class AtomicCacheValue {
public:
    AtomicCacheValue() = delete;
    AtomicCacheValue(JSON& d, const char* k, std::vector<A>& fcache, const StringIntMap& indices)
        :data(d), key(k)
    {
        if (key[0] == Static::underscore_c && indices.find(key) != indices.end()) {
            // fastpath to wasm vars
            const int& inx = indices.at(key);
            address = fcache[inx];
        }
        else {
            // slowpath to json vars
            if constexpr (std::is_same_v<A, int>) {
                value = init_val = JAsInt(data, key);
            }
            if constexpr (std::is_same_v<A, float>) {
                value = init_val = JAsFloat(data, key);
            }
        }
    }

    ~AtomicCacheValue() {
        // If this is a slowpath JSON var, we copy value
        // back to data if it has changed
        if (address == nullptr && value != init_val) {
            JSet(data, key, value);
        }
    }

    A* value_addr() {
        if (address != nullptr) return address;
        return &value;
    }

    bool is_changed() {
        // fast path vars don't exist on the server, so no
        // need for a notify_server() invocation
        if (address != nullptr) return false;
        // for slow path vars we do need to check for val change
        return value != init_val;
    }

    std::pair<A, A> get_change() {
        return std::make_pair<A, A>(init_val, value);
    }

private:
    A init_val;
    A value;
    A* address{ nullptr };
    const char* key;
    JSON& data;
};

// NDContext helper: SetStyleColoring
void SetStyleColoring(int col) {
    switch (col) {
    case Dark:
        ImGui::StyleColorsDark();
        break;
    case Light:
        ImGui::StyleColorsLight();
        break;
    case Classic:
        ImGui::StyleColorsClassic();
        break;
    }
}

void ErrorModal(const std::string& error_message) {

    static ImVec2 position = { 0.5, 0.5 };

    ImGui::OpenPopup(Static::error_modal_cs);

    // Always center this window when appearing
    ImGuiViewport* vp = ImGui::GetMainViewport();
    auto center = vp->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, position);

    if (ImGui::BeginPopupModal(Static::error_modal_cs, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(error_message.c_str());
        ImGui::EndPopup();
    }
}

template <typename JSON, typename DB>
class NDContext {
private:
    // ref to "server process"; just an abstraction of EMS vs win32
    NDProxy<DB>&        proxy;  // DB proxy decouples NDContext from DB detail
    JSON                layout; // layout and data are fetched by 
    JSON                data;   // sync c++py calls not HTTP gets
    std::deque<JSON>    stack;  // render stack

    int                 style_coloring{ StyleColor::Dark };   // match im_start StyleColorsDark()
    float*              fast_path_float[FloatIndices::EndFloats];
    int*                fast_path_int[IntIndices::EndInts];
    StringIntMap        fpf_indices;
    StringIntMap        fpi_indices;
    bool                data_loaded{ false };
    bool                layout_loaded{ false };

    // map layout render func names to the actual C++ impls
    std::unordered_map<std::string, std::function<void(const JSON& w)>> rfmap;
    // top level layout widgets with widget_id eg modals are in pushables
    std::unordered_map<std::string, JSON> pushable;

    // manage down logging by tracking misconfiged queries that throw
    // BAD_HANDLE_FAIL, especially for browser console log
    std::map<std::string, uint32_t>   bad_handle_map;

    // action_dispatch is called while rendering, and changes
    // the size of the render stack. JS will let us do that in the root
    // render() method. But in C++ we use an STL iterator in the root render
    // method, and that segfaults. So in C++ we have pending pushes done
    // outside the render stack walk. JOS 2025-01-31
    std::deque<JSON>        pending_pushes;
    std::deque<std::string> pending_pops;
    // In flight action sequences. Key is query_id, not query_id.event
    struct InFlight {
        InFlight() {}
        InFlight(JSON& s, int i, const char* n, const std::string& qid)
            :sequence(s), inx(i), next(n), query_id(qid) {}
        JSON        sequence{JSON::array()};
        int         inx{ 0 };
        const char* next{ nullptr };
        std::string query_id;
    };
    std::list<InFlight> in_flight_list;

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
    // std::uint32_t bad_handle_count = 0;
    std::deque<std::string> bad_font_pushes;

    WebSockSenderFunc ws_send = nullptr;  // ref to NDWebSockClient::send
    MessagePumpFunc msg_pump = nullptr;
    GLFWwindow* glfw_window = nullptr;

    // Buffer for action_event compound keys
    char act_ev_key_buf[act_ev_key_buf_len];

public:
    NDContext(NDProxy<DB>& s)
        :proxy(s), red{ 255, 51, 0 },
        green{ 102, 153, 0 },
        amber{ 255, 153, 0 } {
        // init status is not connected
        db_status_color = red;

        rfmap.emplace(std::string("Home"), [this](const JSON& w) { render_home(w); });

        // native imgui input widgets
        rfmap.emplace(std::string("InputInt"), [this](const JSON& w) { render_input_int(w); });
        rfmap.emplace(std::string("Combo"), [this](const JSON& w) { render_combo(w); });
        rfmap.emplace(std::string("Checkbox"), [this](const JSON& w) { render_checkbox(w); });
        rfmap.emplace(std::string("Text"), [this](const JSON& w) { render_text(w); });
        rfmap.emplace(std::string("Button"), [this](const JSON& w) { render_button(w); });
        rfmap.emplace(std::string("Table"), [this](const JSON& w) { render_table(w); });

        // nodom compound widgets
        rfmap.emplace(std::string("Footer"), [this](const JSON& w) { render_footer(w); });
        rfmap.emplace(std::string("DatePicker"), [this](const JSON& w) { render_date_picker(w); });
        rfmap.emplace(std::string("DuckTableSummaryModal"), [this](const JSON& w) { render_duck_table_summary_modal(w); });
        rfmap.emplace(std::string("DuckParquetLoadingModal"), [this](const JSON& w) { render_duck_parquet_loading_modal(w); });

        // layout widgets
        rfmap.emplace(std::string("Separator"), [this](const JSON& w) { render_separator(w); });
        rfmap.emplace(std::string("SameLine"), [this](const JSON& w) { render_same_line(w); });
        rfmap.emplace(std::string("NewLine"), [this](const JSON& w) { render_new_line(w); });
        rfmap.emplace(std::string("Spacing"), [this](const JSON& w) { render_spacing(w); });
        rfmap.emplace(std::string("AlignTextToFramePadding"), [this](const JSON& w) { render_align_text_to_frame_padding(w); });

        // grouping
        rfmap.emplace(std::string("BeginChild"), [this](const JSON& w) { render_begin_child(w); });
        rfmap.emplace(std::string("EndChild"), [this](const JSON& w) { render_end_child(w); });
        rfmap.emplace(std::string("BeginGroup"), [this](const JSON& w) { render_begin_group(w); });
        rfmap.emplace(std::string("EndGroup"), [this](const JSON& w) { render_end_group(w); });

        // fonts
        rfmap.emplace(std::string("PushFont"), [this](const JSON& w) { render_push_font(w); });
        rfmap.emplace(std::string("PopFont"), [this](const JSON& w) { render_pop_font(w); });

    }

    void initialize() {
        // NDContext ctor is fired before im_start initializes imgui. Some
        // init tasks require imgui to be initialized; we do those here and
        // are invoked on the 1st render cycle
        // 
        // glfwSwapInterval invokes _emscripten_set_main_loop_timing, which
        // throws if main the main loop hasn't been started by emscripten_set_main_loop[_arg]
        // Not an issue for Breadboard, but it is a nodom break
        glfwSwapInterval(1);

        // init the fast path vars from the settings established in im_render.hpp:im_start()
        ImGuiStyle& style = ImGui::GetStyle();
        // FontScale vals are set in im_start; we recover then here
        fast_path_float[FloatIndices::FontScaleDpi] = &style.FontScaleDpi;
        fast_path_float[FloatIndices::FontScaleMain] = &style.FontScaleMain;
        fpf_indices[Static::_font_scale_dpi_cs] = FloatIndices::FontScaleDpi;
        fpf_indices[Static::_font_scale_main_cs] = FloatIndices::FontScaleMain;
        // No single var for style in imgui; one method for each style
        // im_start sets Dark, and nodom uses enum StyleColor
        fast_path_int[IntIndices::StyleColoring] = &style_coloring;
        fpi_indices[Static::_style_coloring] = IntIndices::StyleColoring;
    }

    GLFWwindow* get_glfw_window() { return glfw_window; }
    void        set_glfw_window(GLFWwindow* w) { glfw_window = w; }

    bool        cache_loaded() { return data_loaded && layout_loaded; }

    // invoked by main loop
    void render() {
        pix_begin_render(render_count);
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
        pix_end_event();
    }

    void start_render_cycle() {
        const static char* method = "NDContext::start_render_cycle: ";

        if (render_count == 0) initialize();


        // Zero the font push/pop counts before rendering. This
        // enables us to detect lopsided push/pop sequences after
        // all widgets have rendered.
        font_push_count = 0;
        font_pop_count = 0;
        bad_font_pushes.clear();

        if (pending_pops.size() || pending_pushes.size()) {
            NDLogger::cout() << method << "PPOPS(" << pending_pops.size() << ") PPUSHS(" << pending_pushes.size()
                << ")" << std::endl;
        }
    }

    void end_render_cycle() {
        const static char* method = "NDContext::end_render_cycle: ";

        if (font_push_count != font_pop_count) {
            NDLogger::cerr() << method << "font_push_count: " << font_push_count
                << ", font_pop_count: " << font_pop_count << std::endl;
        }
        render_count++;

        pix_report(RenderPushPC, static_cast<float>(font_push_count));
        pix_report(RenderPopPC, static_cast<float>(font_pop_count));
        pix_report(RenderFPS, ImGui::GetIO().Framerate);
        int bad_handle_sum = 0;
        for (auto citer = bad_handle_map.cbegin(); citer != bad_handle_map.end(); ++citer) {
            bad_handle_sum += citer->second;
        }
        pix_report(RenderBadHandlePC, bad_handle_sum);

        if (render_count % 60 == 0) {   // every 60 renders eg ~1 sec
            for (auto citer = bad_handle_map.cbegin(); citer != bad_handle_map.cend(); ++citer) {
                NDLogger::cout() << method << citer->first << " BHC: " << citer->second << std::endl;
            }
        }
    }

    void server_request(const std::string& key) {
        std::stringstream msgbuf;
        msgbuf << "{ \"" << Static::nd_type_cs << "\":\"" << Static::cache_request_cs << "\",\""
            << Static::cache_key_cs << "\":\"" << key << "\"}";
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
        msgbuf << "{ \"" << Static::nd_type_cs << "\":\"" << Static::data_change_cs << "\",\""
            << Static::cache_key_cs << "\":\"" << caddr << "\",\""
            << Static::new_value_cs << "\":" << new_val << ",\""
            << Static::old_value_cs << "\":" << old_val << "}";
        ws_send(msgbuf.str());
    }

    void dispatch_server_responses(std::queue<JSON>& responses) {
        const static char* method = "NDContext::dispatch_server_responses: ";

        // server_changes will be a list of json obj copied out of a pybind11
        // list of py dicts. So use C++11 auto range...
        while (!responses.empty()) {
            const JSON& resp = responses.front();
            NDLogger::cout() << method << JPrettyPrint(resp) << std::endl;
            std::string nd_type(JAsString(resp, Static::nd_type_cs));
            // polymorphic as types are hidden inside change
            // Is this a CacheResponse for layout or data?
            if (nd_type == Static::cache_response_cs) {
                if (JAsString(resp, Static::cache_key_cs) == Static::data_cs) {
                    data = resp[Static::value_cs];
                    on_data();
                    data_loaded = true;
                }
                else if (JAsString(resp, Static::cache_key_cs) == Static::layout_cs) {
                    layout = resp[Static::value_cs];
                    on_layout();
                    layout_loaded = true;
                }
                if (cache_loaded())
                    NDLogger::cout() << method << "CACHE_LOADED" << std::endl;
            }
            // Is this a DataChange?
            else if (nd_type == Static::data_change_cs) {
                std::string ckey = JAsString(resp, Static::cache_key_cs);
                JSet(data, ckey.c_str(), resp[Static::new_value_cs]);
            }
            else if (nd_type == Static::data_change_confirmed_cs) {
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

        if (!JContains(db_msg, Static::nd_type_cs)) {
            NDLogger::cerr() << method << "no nd_type in " << db_msg << std::endl;
            return;
        }
        std::string nd_type = JAsString(db_msg, Static::nd_type_cs);
        std::string qid = JAsString(db_msg, Static::query_id_cs);
        NDLogger::cout() << method << nd_type << ", QID: " << qid << std::endl;

        if (nd_type == Static::command_cs) {
            db_status_color = amber;
        }
        else if (nd_type == Static::query_cs) {
            db_status_color = amber;
        }
        else if (nd_type == Static::command_result_cs) {
            db_status_color = green;
            action_dispatch(qid, nd_type);
        }
        else if (nd_type == Static::query_result_cs) {
            db_status_color = green;
            // Typically, a QueryResult is followed by dispatch
            // of a BatchRequest. 
            action_dispatch(qid, nd_type);
        }
        else if (nd_type == Static::batch_response_cs) {
            db_status_color = green;
            action_dispatch(qid, nd_type);
        }
        else if (nd_type == Static::duck_instance_cs) {
            // TODO: q processing order means this doesn't happen so early in cpp
            // main.ts:on_duck_event invokes check_duck_module.
            // However, we don't need all the check_duck_module JS module stuff,
            // so we can just flip status button color here
            db_status_color = amber;
            // trigger any DuckInstance action
            action_dispatch(Static::db_online_cs, Static::duck_instance_cs);
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
            NDLogger::cout() << method << "LAYOUT_CHILD: " << JPrettyPrint(w) << std::endl;
            if (JContains(w, Static::widget_id_cs)) {
                std::string widget_id(JAsString(w, Static::widget_id_cs));
                if (!widget_id.empty()) {
                    NDLogger::cout() << method << "LAYOUT_PUSHABLE: " << widget_id << ":" << JPrettyPrint(w) << std::endl;
                    pushable[widget_id] = w;
                }
                else {
                    NDLogger::cerr() << method << "empty widget_id in: " << JPrettyPrint(w) << std::endl;
                }
            }
        }
        // Home on the render stack
        stack.push_back(layout[0]);
    }

    void on_data() {
        const static char* method = "NDContext::on_data: ";

        NDLogger::cout() << method << "CACHE_DATA: " << JPrettyPrint(data) << std::endl;
        if (!JContains(data, Static::actions_cs)) {
            NDLogger::cout() << method << "NO_ACTIONS in data!" << std::endl;
            return;
        }
    }

    void get_config(JSON& cfg) { proxy.get_config(cfg); }
    void register_font(const std::string& name, ImFont* f) { font_map[name] = f; }
    void register_ws_sender(WebSockSenderFunc send) { ws_send = send; }
    void register_msg_pump(MessagePumpFunc mpf) { msg_pump = mpf; }
    void pump_messages() { msg_pump(); }

protected:
    // w["rname"] resolve & invoke
    void dispatch_render(const JSON& w) {
        const static char* method = "NDContext::dispatch_render: ";

        if (!JContains(w, Static::rname_cs)) {
            NDLogger::cerr() << method << "missing rname in " << w << std::endl;
            return;
        }
        std::string rname = JAsString(w, Static::rname_cs);
        const auto it = rfmap.find(rname);
        if (it == rfmap.end()) {
            NDLogger::cerr() << method << "unknown rname in " << w << std::endl;
            return;
        }
        it->second(w);
    }

    void compound_action_key(char* buf, int buflen, const std::string& action_id,
                                                    const std::string& nd_event) {
        // First, compose the compound action sequence key from action_id and nd_event
        // NB action_id will be a widget_id or query_id
        char* dest = buf;
        int space = buflen;
        memset(dest, 0, space);
        strncpy(dest, action_id.c_str(), space);
        dest += action_id.size();
        space -= action_id.size();
        strncpy(dest, Static::period_cs, space);
        dest += strlen(Static::period_cs);
        space -= strlen(Static::period_cs);
        strncpy(dest, nd_event.c_str(), space);
    }

    void action_dispatch(const std::string& action_id, const std::string& nd_event) {
        const static char* method = "NDContext::action_dispatch: ";

        compound_action_key(act_ev_key_buf, act_ev_key_buf_len, action_id, nd_event);

        NDLogger::cout() << method << "ACT_EV(" << act_ev_key_buf << ")" << std::endl;

        if (!JContains(data, Static::actions_cs)) {
            NDLogger::cerr() << method << "no actions in data!" << std::endl;
            return;
        }
        // Get hold of "actions" in data: do we a matching action?
        // NB if there isn't a matching action.event compound key in data.actions,
        // then we have to check the in flight sequences...
        const JSON& actions(data[Static::actions_cs]);
        JSON action_seq = JSON::array();
        std::list<InFlight> new_in_flight_list;
        if (JContains(actions, act_ev_key_buf)) {
            InFlight resume;
            // we have a match in data.actions to kick off a sequence
            action_seq = actions[act_ev_key_buf];
            action_execute(action_seq, 0, resume);
            // If there's a continuation, add to new list
            if (resume.next != nullptr)
                new_in_flight_list.emplace_back(resume);
        }
        // If there's an action specified in data.actions, we've executed it,
        // and captured the resumption in InFlight resume. So now we check if
        // there were any in flight sequences waiting for an action.event
        auto if_iter = in_flight_list.begin();
        while (if_iter != in_flight_list.end()) {
            if (if_iter->query_id == action_id && nd_event == if_iter->next) {
                // we have an in flight match with action.event so execute
                // then add any resumption to new list
                InFlight resume;
                action_execute(if_iter->sequence, if_iter->inx, resume);
                if (resume.next != nullptr)
                    new_in_flight_list.emplace_back(resume);
            }
            else {
                // this in flight didn't match, so just copy to new list
                new_in_flight_list.emplace_back(*if_iter);
            }
            if_iter++;
        }
        // Do we have any continuations in hand?
        // If so swap the in flight lists
        if (new_in_flight_list.size() > 0) {
            in_flight_list.swap(new_in_flight_list);
        }
    }

    void action_execute(JSON& action_seq, int action_inx, InFlight& resume) {
        const static char* method = "NDContext::action_execute: ";
        int seq_len = JSize(action_seq);
        if (action_inx >= seq_len) {
            NDLogger::cerr() << method << "ACT_EXEC: inx(" << action_inx
                << ") >= len(" << seq_len << ")" << std::endl;
            return;
        }
        const JSON& action_defn(action_seq[action_inx]);
        // Now we have a matched action definition in hand we can look
        // for UI push/pop and DB scan/query. If there's both push and pop,
        // pop goes first naturally!
        if (JContains(action_defn, Static::ui_pop_cs)) {
            // for pops we supply the rname, not the pushable name so
            // the context can check the widget type on pops
            std::string rname(JAsString(action_defn, Static::ui_pop_cs));
            NDLogger::cout() << method << "ui_pop(" << rname << ")" << std::endl;
            pending_pops.push_back(rname);
        }
        if (JContains(action_defn, Static::ui_push_cs)) {
            // for pushes we supply widget_id, not the rname
            std::string widget_id(JAsString(action_defn, Static::ui_push_cs));
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
        // TODO: rejig!
        std::string db_action;
        std::string query_id;
        std::string sql_cache_key;
        if (JContains(action_defn, Static::db_action_cs)) {
            db_action = JAsString(action_defn, Static::db_action_cs);
            if (!JContains(action_defn, Static::query_id_cs)) {
                NDLogger::cerr() << method << "ACT_EXEC_FAIL db_action(" << db_action << ") missing query_id" << std::endl;
                return;
            }
            query_id = JAsString(action_defn, Static::query_id_cs);
            if (db_action == Static::batch_request_cs) {
                db_dispatch(db_action, query_id);
            }
            else {
                if (!JContains(action_defn, Static::sql_cname_cs)) {
                    NDLogger::cerr() << method << "ACT_EXEC_FAIL db_action(" << db_action << ") missing sql_cname" << std::endl;
                    return;
                }
                sql_cache_key = JAsString(action_defn, Static::sql_cname_cs);
                if (!JContains(data, sql_cache_key.c_str())) {
                    NDLogger::cerr() << method << "ACT_EXEC_FAIL db_action(" << db_action << ") sql_cname(" << sql_cache_key << ") does not resolve!" << std::endl;
                    return;
                }
                else {
                    std::string sql(JAsString(data, sql_cache_key.c_str()));
                    db_dispatch(db_action, query_id, sql);
                }
            }
            // We've dispatched a DB op from a sequence, so there's a 
            // continuation if this is not the last action.
            if (++action_inx < seq_len) {
                resume.sequence = action_seq;
                resume.inx = action_inx;
                resume.query_id = query_id;
                resume.next = NextEvent(db_action.c_str());
            }
        }
    }

    void db_dispatch(const std::string& nd_type, const std::string& qid, 
                    const std::string& sql) {
        auto db_request = JNewObject();
        JSet(db_request, Static::nd_type_cs, nd_type.c_str());
        JSet(db_request, Static::sql_cs, sql.c_str());
        JSet(db_request, Static::query_id_cs, qid.c_str());
        proxy.db_dispatch(db_request);
    }

    void db_dispatch(const std::string& nd_type, const std::string& qid) {
        auto db_request = JNewObject();
        JSet(db_request, Static::nd_type_cs, nd_type.c_str());
        JSet(db_request, Static::query_id_cs, qid.c_str());
        proxy.db_dispatch(db_request);
    }

    // Render functions
    void render_home(const JSON& w) {
        const static char* method = "NDContext::render_home: ";

        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec( w[Static::cspec_cs]);
        std::string title(Static::nodom_cs);
        if (JContains(cspec, Static::title_cs)) {
            title = JAsString(cspec, Static::title_cs);
        }

        bool fpop = false;
        if (JContains(cspec, Static::title_font_cs))
            fpop = push_font(w, Static::title_font_cs, Static::title_font_size_cs);

        ImGui::Begin(title.c_str());
        const JSON& children(w[Static::children_cs]);
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
        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << JPrettyPrint(w) << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string label;
        if (JContains(cspec, Static::text_cs)) label = JAsString(cspec, Static::text_cs);
        // params by value
        int step = 1;
        if (JContains(cspec, Static::step_cs)) step = JAsInt(cspec, Static::step_cs);
        int step_fast = 1;
        if (JContains(cspec, Static::step_fast_cs)) step_fast = JAsInt(cspec, Static::step_fast_cs);
        int flags = 0;
        if (JContains(cspec, Static::flags_cs)) flags = JAsInt(cspec, Static::flags_cs);
        // one param by ref: the int itself
        std::string cname_cache_addr = JAsString(cspec, Static::cname_cs);
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
        static StringVec combo_list;
        static const char* cs_combo_list[ND_MAX_COMBO_LIST];
        static int combo_selection;
        memset(cs_combo_list, 0, ND_MAX_COMBO_LIST * sizeof(char*));
        combo_selection = 0;
        combo_list.clear();
        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << JPrettyPrint(w) << ")" << std::endl;
            return;
        }
        const JSON& cspec(w["cspec"]);
        std::string label(Static::nodom_cs);
        int step = 1;
        if (JContains(cspec, Static::text_cs)) label = JAsString(cspec, Static::text_cs);
        if (JContains(cspec, Static::step_cs)) step = JAsInt(cspec, Static::step_cs);
        // no value params in layout here; all combo layout is data cache refs
        // /cspec/cname should give us a data cache addr for the combo list
        std::string combo_list_cache_addr(JAsString(cspec, Static::cname_cs));
        std::string combo_index_cache_addr(JAsString(cspec, Static::index_cs));
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

        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string cname_cache_addr(JAsString(cspec, Static::cname_cs));
        std::string text(cname_cache_addr);
        if (JContains(cspec, Static::text_cs))
            text = JAsString(cspec, Static::text_cs);

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

    void render_footer(const JSON& w) {
        static const char* method = "NDContext::render_footer: ";

        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << JPrettyPrint(w) << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        bool db = JAsBool(cspec, Static::db_cs);
        bool fps = JAsBool(cspec, Static::fps_cs);
        // TODO: config demo mode so it can be switched on/off in prod
        bool demo = JAsBool(cspec, Static::demo_cs);
        bool id_stack = JAsBool(cspec, Static::id_stack_cs);
        bool font_scale = JAsBool(cspec, Static::font_scale_cs);
        bool style = JAsBool(cspec, Static::style_cs);

        // TODO: understand ems mem anlytics and restore in footer
        // bool memory = w.value(nlohmann::json::json_pointer("/cspec/memory"), true);

        ImGui::BeginGroup();
        if (db) {
            // Push colour styling for the DB button
            ImGui::PushStyleColor(ImGuiCol_Button, (ImU32)db_status_color);
            if (ImGui::Button("DB")) {
                action_dispatch(Static::footer_db_button_cs, Static::click_cs);
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
        if (font_scale) {
            ImGuiStyle& style = ImGui::GetStyle();
            ImGui::SameLine();
            ImGui::Text("FontScale");
            ImGui::SameLine();
            if (ImGui::ArrowButton("##left", ImGuiDir_Left)) { style.FontScaleMain -= 0.25; }
            ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
            if (ImGui::ArrowButton("##right", ImGuiDir_Right)) { style.FontScaleMain += 0.25; }
            ImGui::SameLine();
            ImGui::Text("%.2f", style.FontScaleMain);
        }
        if (style) {
            static const char* cs_combo_list[3] = { Static::dark_cs, Static::light_cs, Static::classic_cs };
            int old_val = style_coloring;
            ImGui::Combo(Static::style_label_cs, &style_coloring, cs_combo_list, 3, 3);
            if (style_coloring != old_val) SetStyleColoring(style_coloring);
        }
        ImGui::EndGroup();
    }

    // layout "jiggler": see imgui.h "Other layout functions"
    void render_separator(const JSON&) {
        ImGui::Separator();
    }

    void render_same_line(const JSON&) {
        ImGui::SameLine();
    }

    void render_new_line(const JSON&) {
        ImGui::NewLine();
    }

    void render_spacing(const JSON&) {
        ImGui::Spacing();
    }

    void render_align_text_to_frame_padding(const JSON&) {
        ImGui::AlignTextToFramePadding();
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

            if (!JContains(w, Static::cspec_cs)) {
                NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
                return;
            }
            const JSON& cspec(w[Static::cspec_cs]);
            if (JContains(cspec, Static::year_month_font_cs)) {
                std::string ym_font(JAsString(cspec, Static::year_month_font_cs));
                auto font_it = font_map.find(ym_font);
                if (font_it != font_map.end()) {
                    year_month_font = font_it->second;
                    if (JContains(cspec, Static::year_month_font_size_cs))
                        year_month_font_size_base = JAsInt(cspec, Static::year_month_font_size_cs);
                }
                else {
                    // Use Default font installed by im_start instead
                    year_month_font = font_map[Static::default_cs];
                }
            }
            if (JContains(cspec, Static::day_date_font_cs)) {
                std::string dd_font(JAsString(cspec, Static::day_date_font_cs));
                auto font_it = font_map.find(dd_font);
                if (font_it != font_map.end()) {
                    day_date_font = font_it->second;
                    if (JContains(cspec, Static::day_date_font_size_cs))
                        day_date_font_size_base = JAsInt(cspec, Static::day_date_font_size_cs);
                }
                else {
                    // Use Default font installed by im_start instead
                    day_date_font = font_map[Static::default_cs];
                }
            }
            int table_flags = default_table_flags;
            if (JContains(cspec, Static::table_flags_cs))
                table_flags = JAsInt(cspec, Static::table_flags_cs);
            int combo_flags = default_combo_flags;
            if (JContains(cspec, Static::combo_flags_cs))
                combo_flags = JAsInt(cspec, Static::combo_flags_cs);
            std::string ckey = JAsString(cspec, Static::cname_cs);
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

        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string rtext = JAsString(cspec, Static::text_cs);
        ImGui::Text(rtext.c_str());
    }

    void render_button(const JSON& w) {
        const static char* method = "NDContext::render_button: ";

        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        if (!JContains(cspec, Static::text_cs)) {
            NDLogger::cerr() << method << "no text in cspec(" << cspec << ")" << std::endl;
            return;
        }
        std::string button_text = JAsString(cspec, Static::text_cs);
        std::string widget_id(button_text);
        if (JContains(cspec, Static::widget_id_cs)) {
            widget_id = JAsString(cspec, Static::widget_id_cs);
        }
        if (ImGui::Button(button_text.c_str())) {
            action_dispatch(widget_id, Static::click_cs);
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

        if (!JContains(w, Static::cspec_cs) || 
            !JContains(w[Static::cspec_cs], Static::qname_cs) || 
            !JContains(w[Static::cspec_cs], Static::title_cs)) {
            NDLogger::cerr() << method << Static::BAD_CSPEC_cs << JPrettyPrint(w) << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        const std::string qname(JAsString(cspec, Static::qname_cs));
        std::string title(qname);
        if (JContains(cspec, Static::title_cs))
            title = JAsString(cspec, Static::title_cs);

        int table_flags = default_summary_table_flags;
        if (JContains(cspec, Static::table_flags_cs)) {
            table_flags = JAsInt(cspec, Static::table_flags_cs);
        }

        int window_flags = default_window_flags;
        if (JContains(cspec, Static::window_flags_cs)) {
            window_flags = JAsInt(cspec, Static::window_flags_cs);
        }

        bool title_pop = false;
        if (JContains(cspec, Static::title_font_cs))
            title_pop = push_font(w, Static::title_font_cs, Static::title_font_size_cs);

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
            if (JContains(cspec, Static::body_font_cs))
                body_pop = push_font(w, Static::body_font_cs, Static::body_font_size_cs);

            std::uint64_t result_handle = proxy.get_handle(qname);
            if (!result_handle) {
                auto [iter, inserted] = bad_handle_map.insert(std::make_pair(std::move(qname), 1));
                if (!inserted) iter->second++;
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
        if (JContains(cspec, Static::button_font_cs))
            button_pop = push_font(w, Static::button_font_cs, Static::button_font_size_cs);

        // Note we do not invoke pop_widget() here as we're
        // rendering, and that would change the stack while
        // the topmost render method is iterating over it.
        // Instead we add it to the list of pending_pops
        // so top level render will invoke pop_widget for us.
        if (ImGui::Button(Static::ok_cs)) {
            ImGui::CloseCurrentPopup();
            pending_pops.push_back(Static::duck_table_summary_modal_cs);
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button(Static::cancel_cs)) {
            ImGui::CloseCurrentPopup();
            pending_pops.push_back(Static::duck_table_summary_modal_cs);
        }

        if (button_pop) pop_font();

        ImGui::EndPopup();
    }

    void render_duck_parquet_loading_modal(const JSON& w) {
        const static char* method = "NDContext::render_duck_parquet_loading_modal: ";

        static ImVec2 position = { 0.5, 0.5 };

        if (!JContains(w, Static::cspec_cs) || 
            !JContains(w[Static::cspec_cs], Static::cname_cs) || 
            !JContains(w[Static::cspec_cs], Static::title_cs)) {
            NDLogger::cerr() << method << "BAD_CSPEC" << w << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string cname_cache_addr(JAsString(cspec, Static::cname_cs));
        std::string title(cname_cache_addr);
        if (JContains(cspec, Static::title_cs))
            title = JAsString(cspec, Static::title_cs);

        bool tpop = false;
        if (JContains(cspec, Static::title_font_cs))
            tpop = push_font(w, Static::title_font_cs, Static::title_font_size_cs);

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
        StringVec pq_url_vec;
        JAsStringVec(data, cname_cache_addr.c_str(), pq_url_vec);

        if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            bool bpop = false;
            if (JContains(cspec, Static::body_font_cs))
                bpop = push_font(w, Static::body_font_cs, Static::body_font_size_cs);
            for (auto pq_iter = pq_url_vec.begin(); pq_iter != pq_url_vec.end(); ++pq_iter) {
                ImGui::Text((*pq_iter).c_str());
            }
            if (bpop) pop_font();
            int spinner_radius = 5;
            int spinner_thickness = 2;
            if (JContains(cspec, Static::spinner_radius_cs))
                spinner_radius = JAsInt(cspec, Static::spinner_radius_cs);
            if (JContains(cspec, Static::spinner_thickness_cs))
                spinner_thickness = JAsInt(cspec, Static::spinner_thickness_cs);
            if (!ImGui::Spinner("parquet_loading_spinner", (float)spinner_radius, spinner_thickness, 0)) {
                // TODO: spinner always fails IsClippedEx on first render
                NDLogger::cout() << method << "spinner fail" << std::endl;
            }
            ImGui::EndPopup();
        }
        if (tpop) pop_font();
    }

    void render_table(const JSON& w) {
        const static char* method = "NDContext::render_table: ";
        static int default_summary_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
        static int default_window_flags = ImGuiWindowFlags_AlwaysAutoResize;

        if (!JContains(w, Static::cspec_cs) || !JContains(w[Static::cspec_cs], Static::qname_cs)) {
            NDLogger::cerr() << method << "BAD_CSPEC" << w << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        const std::string qname(JAsString(cspec, Static::qname_cs));

        int table_flags = default_summary_table_flags;
        if (JContains(cspec, Static::table_flags_cs)) {
            table_flags = JAsInt(cspec, Static::table_flags_cs);
        }

        int window_flags = default_window_flags;
        if (JContains(cspec, Static::window_flags_cs)) {
            window_flags = JAsInt(cspec, Static::window_flags_cs);
        }

        // TODO: recode proxy.get_meta_data() to lazy load 
        // and report geom
        std::uint64_t colm_count = 0;
        std::uint64_t row_count = 0;
        int colm_index = 0;
        bool body_pop = false;

        if (JContains(cspec, Static::body_font_cs))
            body_pop = push_font(w, Static::body_font_cs, Static::body_font_size_cs);

        std::uint64_t result_handle = proxy.get_handle(qname);
        if (!result_handle) {
            auto [iter, inserted] = bad_handle_map.insert(std::make_pair(std::move(qname), 1));
            if (!inserted) iter->second++;
            return;
        }
        if (!proxy.get_meta_data(result_handle, colm_count, row_count)) {
            NDLogger::cout() << method << "GET_META_DATA_FAIL for QID: " << qname << std::endl;
            return;
        }
        StringVec& colm_names = proxy.get_col_names(result_handle);
        if (ImGui::BeginTable(qname.c_str(), (int)colm_count, table_flags)) {
            ImGui::TableSetupScrollFreeze(1, 1);
            for (colm_index = 0; colm_index < colm_count; colm_index++) {
                ImGui::TableSetupColumn(colm_names[colm_index].c_str());
            }
            ImGui::TableHeadersRow();
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
    }

    void render_push_font(const JSON& w) {
        push_font(w, Static::font_cs, Static::font_size_cs);
    }

    void render_pop_font(const JSON&) {
        pop_font();
    }

    void render_begin_child(const JSON& w) {
        const static char* method = "NDContext::render_begin_child: ";

        // NB BeginChild is a grouping mechanism, so there's no single
        // cache datum to which we refer, so no cname. However, we do look
        // require a title (for imgui ID purposes) and we can have styling
        // attributes like ImGuiChildFlags
        if (!JContains(w, Static::cspec_cs) || !JContains(w[Static::cspec_cs], Static::title_cs)) {
            NDLogger::cerr() << method << Static::BAD_CSPEC_cs << JPrettyPrint(w) << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string title(JAsString(cspec, Static::title_cs));

        int child_flags = 0;
        if (JContains(cspec, Static::child_flags_cs)) {
            child_flags = JAsInt(cspec, Static::child_flags_cs);
        }
        ImVec2 size{ 0, 0 };
        if (JContains(cspec, Static::size_cs)) {
            JSON size_tup2 = JSON::array();
            size_tup2 = cspec[Static::size_cs];
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

    void pop_widget(const std::string& widget_id_or_render_name = "") {
        const static char* method = "NDContext::pop_widget: ";
        // if rname specifies a class we check the
        //      popped widget rname
        if (!widget_id_or_render_name.empty()) {
            const JSON& w(stack.back());
            std::string render_name = JAsString(w, Static::rname_cs);
            if (JContains(w, Static::widget_id_cs)) {
                std::string widget_id = JAsString(w, Static::widget_id_cs);
                if (render_name != widget_id_or_render_name &&
                    widget_id != widget_id_or_render_name) {
                    NDLogger::cerr() << method << "POP_FAIL mismatch rname("
                        << render_name << ") widget_id("
                        << widget_id << ") widget_id_or_render_name("
                        << widget_id_or_render_name << ")" << std::endl;
                }
                else {
                    stack.pop_back();
                }
            }
            else {
                if (render_name != widget_id_or_render_name) {
                    NDLogger::cerr() << method << "POP_FAIL mismatch rname("
                        << render_name << ") widget_id_or_render_name("
                        << widget_id_or_render_name << ")" << std::endl;
                }
                else {
                    stack.pop_back();
                }
            }
        }
        else {
            NDLogger::cerr() << method << "POP_FAIL empty widget_id_or_render_name(" << widget_id_or_render_name << ")" << std::endl;
        }
    }

    bool push_font(const JSON& w, const char* font_attr,
        const char* font_size_base_attr) {
        const static char* method = "NDContext::push_font: ";

        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "BAD_CSPEC" << w << std::endl;
            return false;
        }
        const JSON& cspec(w[Static::cspec_cs]);
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
            ImFont* default_font = font_map[Static::default_cs];
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


