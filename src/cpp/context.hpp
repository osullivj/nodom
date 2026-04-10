#pragma once
#include <string>
#include <map>
#include <queue>
// #include <filesystem>
#include <functional>
#include "imgui.h"
#include "ImGuiDatePicker.hpp"
#include "proxy.hpp"
#include "static_strings.hpp"
#include "db_cache.hpp"
#include "dl_cache.hpp"
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
static constexpr int MAX_ID_COUNT{ 256 };
static constexpr int ND_MAX_COMBO_LIST{ 16 };
static constexpr int act_ev_key_buf_len{ 256 };

struct GLFWwindow;


// NDContext: stack based rendering
// Utility funcs above if they're too specific to rendering
// to go in nd_utils, NDContext itself below, with all inline
// templated impl. 

// array of each possible show stopping critical error
// critical defn: not so bad it causes exit() or abort()
// bad enough to make rendering impossible
using CritArray = std::array<std::string, Critical::EndCritical>;

template <typename JSON, typename DB>
class NDContext {
private:

    // ref to "server process"; just an abstraction of EMS vs win32
    NDProxy<DB>&            proxy;  // DB proxy decouples NDContext from DB detail
    JSON                    layout; // layout and data are fetched by 
    JSON                    data;   // sync c++py calls not HTTP gets
    std::deque<WidgetPtr>   stack;  // render stack
    DataLayCache<JSON>      data_lay_cache;

    // latest critical error state raised, and array of crit err
    // msgs to show in Home
    Critical            show_stopper{ Clear };  
    CritArray           critical_messages;  // dflt init, dflt ctor for elems

    // initial data and layout possibly provided via argc, argv
    // we keep the websock downloaded data & layout in real_data
    // and real_layout so we can so the parse and write to 
    // JSON data & layout in one shot. We do not want JSON
    // data & layout to be a mix of real and init.
    std::string         init_data_s;
    std::string         init_layout_s;
    JSON                real_data;
    JSON                real_layout;
    int                 style_coloring{ StyleColor::Dark };   // match im_start StyleColorsDark()
    bool                data_loaded{ true };
    bool                layout_loaded{ true };

    // map layout render func names to the actual C++ impls
    // std::unordered_map<std::string, std::function<void(const JSON& w)>> rfmap;
    // top level layout widgets with widget_id eg modals are in pushables
    // std::unordered_map<std::string, JSON> pushable;
    // std::unordered_map<uint32_t, WidgetPtr> pushable;

    // manage down logging by tracking misconfiged queries that throw
    // BAD_HANDLE_FAIL, especially for browser console log
    std::map<std::string, uint32_t>   bad_handle_map;

    // action_dispatch is called while rendering, and changes
    // the size of the render stack. JS will let us do that in the root
    // render() method. But in C++ we use an STL iterator in the root render
    // method, and that segfaults. So in C++ we have pending pushes done
    // outside the render stack walk. JOS 2025-01-31
    // std::deque<JSON>        pending_pushes;
    std::deque<EntityInx>       pending_pushes;
    std::deque<RenderMethod>    pending_pops;
    // In flight action sequences. Key is query_id, not query_id.event
    // TODO: rewrite InFlight to operate on ActionVec/NDAction
    struct InFlight {
        InFlight() {}
        // InFlight(ActionVec& s, int i, const char* n, const std::string& qid)
        InFlight(ActionVec* avec, int i, EventInx n, EntityInx qid)
            :sequence(s), inx(i), next(n), query_id(qid) {}
        ActionVec*  sequence;
        int         inx{ 0 };
        // const char* next{ nullptr };
        EventInx next;
        // std::string query_id;
        EntityInx query_id;
    };
    std::list<InFlight> in_flight_list;

    bool    show_demo = false;
    bool    show_id_stack = false;
    bool    show_memory = false;

    EntityInx   ninx_GUI;
    EntityInx   ninx_Websock;
    EntityInx   ninx_DuckDB;
    EntityInx   ninx_FooterDBButton;

    EventInx    einx_WebSockConnectionFailed;   // CST::SubSysEvent
    EventInx    einx_Online;                    // CST::SubSysEvent
    EventInx    einx_CacheLoaded;               // CST::SubSysEvent
    EventInx    einx_Click;                     // CST::WidgetEvent
    EventInx    einx_Command;                   // CST::DBEvent
    EventInx    einx_CommandResult;             // CST::DBEvent
    EventInx    einx_Query;                     // CST::DBEvent
    EventInx    einx_QueryResult;               // CST::DBEvent
    EventInx    einx_BatchRequest;              // CST::DBEvent
    EventInx    einx_BatchResponse;             // CST::DBEvent
    EventInx    einx_Invalid;                   // !init->OH_FECK

    /* hmm: these are all in the footer cspec, 
        so shouldn't be cache vars...
    IntInx      binx__footer_show_db;
    int         _footer_show_db;
    IntInx      binx__footer_show_fps;
    int         _footer_show_fps;
    IntInx      binx__footer_show_demo;
    IntInx      binx__footer_show_id_stack;
    IntInx      binx__footer_show_font_scale;
    IntInx      binx__footer_show_style;
    */
    bool  footer_show_db{ false };
    bool  footer_show_fps{ false };
    bool  footer_show_demo{ false };
    bool  footer_show_id_stack{ false };
    bool  footer_show_font_scale{ false };
    bool  footer_show_style{ false };

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
    std::deque<StrInx> bad_font_pushes;

    VSFunc ws_send = nullptr;  // ref to NDWebSockClient::send
    VVFunc msg_pump = nullptr;
    GLFWwindow* glfw_window = nullptr;

    // Buffer for action_event compound keys
    char act_ev_key_buf[act_ev_key_buf_len];
    // Buffer for err msg fmt
    char string_buffer[STR_BUF_LEN];

    struct LocalFont {
        inline static VVFunc pop_func{nullptr};
        inline static WCSCSFunc push_func{nullptr};
        LocalFont() = delete;
        LocalFont(const LocalFont&) = delete;
        LocalFont(WidgetPtr w, CacheSpecifier cs_font_name, CacheSpecifier cs_font_size) {
            push_func(w, cs_font_name, cs_font_size);
        }
        ~LocalFont() { pop_func(); }
    };
public:
    NDContext(NDProxy<DB>& s, const char* idata = nullptr, const char* ilayout = nullptr)
        :proxy(s), init_data_s(idata), init_layout_s(ilayout), red{ 255, 51, 0 },
        green{ 102, 153, 0 },
        amber{ 255, 153, 0 }
    {
        // init status is not connected
        db_status_color = red;
        LocalFont::push_func = [&](WidgetPtr w, CacheSpecifier nm, CacheSpecifier sz) {push_font(w, nm, sz); };
        LocalFont::pop_func = [&]() {pop_font(); };
    }

    void initialize() {
        const char* method = "NDContext::initialize: ";
        // NDContext ctor is fired before im_start initializes imgui. Some
        // init tasks require imgui to be initialized; we do those here and
        // are invoked on the 1st render cycle
        // 
        // glfwSwapInterval invokes _emscripten_set_main_loop_timing, which
        // throws if main the main loop hasn't been started by emscripten_set_main_loop[_arg]
        // Not an issue for Breadboard, but it is a nodom break
        glfwSwapInterval(1);

        // websock download of data and layout won't have happened yet,
        // so check if ctor was given init_data/layout splash screen
        if (!init_data_s.empty() && !init_layout_s.empty()) {   // both or neither
            data = JParse<JSON>(init_data_s);
            layout = JParse<JSON>(init_layout_s);
            NDLogger::cout() << method << "ARGV..." << std::endl;
        }
        else {
            data = JParse<JSON>(Static::init_data_cs);
            layout = JParse<JSON>(Static::init_layout_cs);
            NDLogger::cout() << method << "HARDWIRED..." << std::endl;
        }
        NDLogger::cout() << method << "init_data: " << JPrettyPrint(data) << std::endl;
        NDLogger::cout() << method << "init_layout: " << JPrettyPrint(layout) << std::endl;


        // fire post processing of init layout and data
        // NB this will be repeated when real layout and data arrive via websock
        data_lay_cache.on_json(data, layout, [&](){ on_dlc_init(); });
    }

    void on_dlc_init() {
        // DataLayCache on_json() has processed data and layout. Now we add
        // Entity and Event indices for our action_dispatch() impl.

        // First EntityIDs for subsystem events from GUI or Websock
        ninx_GUI = data_lay_cache.add_string<CIT::EntityID>(Static::gui_cs, CST::SubSysID);
        ninx_Websock = data_lay_cache.add_string<CIT::EntityID>(Static::websock_cs, CST::SubSysID);
        ninx_DuckDB = data_lay_cache.add_string<CIT::EntityID>(Static::duck_db_cs, CST::SubSysID);

        // EntityIDs for predefined widgets like LoadingModal
        ninx_FooterDBButton = data_lay_cache.add_string<CIT::EntityID>(Static::i_am_footer_db_button_cs, CST::WidgetID);

        // Events: subsys
        einx_WebSockConnectionFailed = data_lay_cache.add_string<CIT::Event>(
                                        CriticalToString(WebSockConnectionFailed), CST::SubSysEvent);
        einx_Online = data_lay_cache.add_string<CIT::Event>(Static::online_cs, CST::SubSysEvent);
        einx_CacheLoaded = data_lay_cache.add_string<CIT::Event>(Static::cache_loaded_cs, CST::SubSysEvent);

        // Events: widget
        einx_Click = data_lay_cache.add_string<CIT::Event>(Static::click_cs, CST::WidgetEvent);

        // Events: DB
        einx_Command = data_lay_cache.add_string<CIT::Event>(Static::command_cs, CST::DBEvent);
        einx_CommandResult = data_lay_cache.add_string<CIT::Event>(Static::command_result_cs, CST::DBEvent);
        einx_Query = data_lay_cache.add_string<CIT::Event>(Static::query_cs, CST::DBEvent);
        einx_QueryResult = data_lay_cache.add_string<CIT::Event>(Static::query_result_cs, CST::DBEvent);
        einx_BatchRequest = data_lay_cache.add_string<CIT::Event>(Static::batch_request_cs, CST::DBEvent);
        einx_BatchResponse = data_lay_cache.add_string<CIT::Event>(Static::batch_response_cs, CST::DBEvent);

        // init the fast path vars from the settings established in im_render.hpp:im_start()
        ImGuiStyle& style = ImGui::GetStyle();

        // init the render stack
        stack.clear();
        stack.push_back(data_lay_cache.get_home());
    }

    EventInx next_db_event(EventInx einx) {
        if (einx == einx_Command) return einx_CommandResult;
        if (einx == einx_Query) return einx_QueryResult;
        if (einx == einx_BatchRequest) return einx_BatchResponse;
        return einx_Invalid;
    }

    // Helpers and accessors
    GLFWwindow* get_glfw_window() { return glfw_window; }
    void        set_glfw_window(GLFWwindow* w) { glfw_window = w; }

    bool        cache_is_loaded() { return data_loaded && layout_loaded; }

    void set_critical(Critical error_code) {
        show_stopper = error_code;
        switch (error_code) {
        case WebSockConnectionFailed:
            // action_dispatch(Static::websock_cs, CriticalToString(show_stopper));
            action_dispatch(ninx_Websock, einx_WebSockConnectionFailed);
            break;
        }
    }

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
            push_widget(data_lay_cache.get_pushable(pending_pushes.front()));
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
            dispatch_render(stack[inx]);
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

        if (render_count == 0) {
            // action_dispatch(Static::gui_cs, Static::gui_online_cs);
            action_dispatch(ninx_GUI, einx_Online);
        }

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

    // void notify_server(const std::string& caddr, JSON& old_val, JSON& new_val) {
    template <typename V>
    void notify_server(DataRef* dref, V& old_val, V& new_val) {

        const static char* method = "NDContext::notify_server: ";

        const char* addr = data_lay_cache.get_addr_value(dref->addr_inx);
        const char* tipe = CDTToString(dref->tipe);
        std::cout << method << "type:" << tipe << ", addr:" << addr
            << ", old: " << old_val << ", new: " << new_val << std::endl;

        std::stringstream msgbuf;
        // We supply wrapping "" for data_change_cs and caddr
        // as we know they're strings. For new_val and old_val
        // we rely on operator<< which is supplied by
        // nlohmann::json. For emscripten::val we use our own
        // operator<< from db_cache.hpp.
        msgbuf << "{ \"" << Static::nd_type_cs << "\":\"" << Static::data_change_cs << "\",\""
            << Static::cache_key_cs << "\":\"" << addr << "\",\""
            << Static::new_value_cs << "\":" << new_val << ",\""
            << Static::old_value_cs << "\":" << old_val << "}";
        ws_send(msgbuf.str());
    }

    void dispatch_events(std::queue<JSON>& events) {
        const static char* method = "NDContext::dispatch_events: ";

        while (!events.empty()) {
            const JSON& resp = events.front();
            NDLogger::cout() << method << JPrettyPrint(resp) << std::endl;
            std::string nd_type(JAsString(resp, Static::nd_type_cs));
            // polymorphic as types are hidden inside change
            // Is this a CacheResponse for layout or data?
            if (nd_type == Static::cache_response_cs) {
                if (JAsString(resp, Static::cache_key_cs) == Static::data_cs) {
                    real_data = resp[Static::value_cs];
                    data_loaded = true;
                }
                else if (JAsString(resp, Static::cache_key_cs) == Static::layout_cs) {
                    real_layout = resp[Static::value_cs];
                    layout_loaded = true;
                }
                if (cache_is_loaded()) {
                    data = real_data;
                    layout = real_layout;
                    data_lay_cache.on_json(data, layout, [&]() {on_dlc_init(); } );
                    // NDLogger::cout() << method << "CACHE_LOADED" << std::endl;
                    // action_dispatch(Static::gui_cs, Static::cache_loaded_cs);
                    action_dispatch(ninx_GUI, einx_CacheLoaded);
                }
            }
            // Is this a DataChange?
            else if (nd_type == Static::data_change_cs) {
                std::string ckey = JAsString(resp, Static::cache_key_cs);
                // JSet(data, ckey.c_str(), resp[Static::new_value_cs]);
                data_lay_cache.on_data_change(ckey, resp);
            }
            else if (nd_type == Static::data_change_confirmed_cs) {
                // TODO: add check that type has not mutated
            }
            // Duck or Parquet event...
            else {
                on_db_event(resp);
            }
            events.pop();
        }
    }

    bool db_app() { return proxy.db_app(); }
    void set_done(bool d) { proxy.set_done(d); }

    void on_ws_open() {
        data_loaded = false;
        layout_loaded = false;
        server_request(Static::data_cs);
        server_request(Static::layout_cs);
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

        // Here add_string acts as find_string
        EntityInx ninx{data_lay_cache.add_string<EntityID>(qid, CST::QueryID)};
        EventInx einx{ data_lay_cache.add_string<Event>(nd_type, CST::DBEvent) };

        // if (nd_type == Static::command_cs) {
        if (einx == einx_Command) {
            db_status_color = amber;
        }
        // else if (nd_type == Static::query_cs) {
        else if (einx == einx_Query) {
            db_status_color = amber;
        }
        // else if (nd_type == Static::command_result_cs) {
        else if (einx == einx_CommandResult) {
            db_status_color = green;
            action_dispatch(ninx, einx); // qid, nd_type);
        }
        // else if (nd_type == Static::query_result_cs) {
        else if (einx == einx_QueryResult) {
            db_status_color = green;
            // Typically, a QueryResult is followed by dispatch
            // of a BatchRequest. 
            action_dispatch(ninx, einx); // qid, nd_type);
        }
        // else if (nd_type == Static::batch_response_cs) {
        else if (einx == einx_BatchResponse) {
            db_status_color = green;
            action_dispatch(ninx, einx); // qid, nd_type);
        }
        // else if (nd_type == Static::duck_instance_cs) {
        else if (einx == einx_Online) {
            // TODO: q processing order means this doesn't happen so early in cpp
            // main.ts:on_duck_event invokes check_duck_module.
            // However, we don't need all the check_duck_module JS module stuff,
            // so we can just flip status button color here
            db_status_color = amber;
            // trigger any DuckInstance action
            // action_dispatch(Static::db_online_cs, Static::duck_instance_cs);
            action_dispatch(ninx_DuckDB, einx); // qid, nd_type);
        }
        else {
            NDLogger::cerr() << method << JPrettyPrint(db_msg) << std::endl;
        }
    }

    void get_config(JSON& cfg) { proxy.get_config(cfg); }
    void register_font(const std::string& name, ImFont* f) { font_map[name] = f; }
    void register_ws_sender(VSFunc send) { ws_send = send; }
    void register_msg_pump(VVFunc mpf) { msg_pump = mpf; }
    void pump_messages() { msg_pump(); }

protected:
    int* cspec_int(CacheSpecifier spec, IntValMap& int_val_map, int* target = nullptr) {
        auto cs_int_iter = int_val_map.find(spec);
        if (cs_int_iter != int_val_map.end()) {
            IntInx int_inx{ cs_int_iter->second };
            int* rv = data_lay_cache.get_int_value(int_inx);
            if (target != nullptr && rv != nullptr) *target = *rv;
            return rv;
        }
        return nullptr;
    }

    uint8_t* cspec_bool(CacheSpecifier spec, BoolValMap& bool_val_map, bool* target = nullptr) {
        auto cs_bool_iter = bool_val_map.find(spec);
        if (cs_bool_iter != bool_val_map.end()) {
            BoolInx bool_inx{ cs_bool_iter->second };
            uint8_t* rv = data_lay_cache.get_bool_value(bool_inx);
            if (target != nullptr && rv != nullptr) *target = *rv;
            return rv;
        }
        return nullptr;
    }


    const char* cspec_string(CacheSpecifier spec, StrValMap& str_val_map, const char* dflt) {
        auto cs_str_iter = str_val_map.find(spec);
        if (cs_str_iter != str_val_map.end()) {
            StrInx text_inx{ cs_str_iter->second };
            return data_lay_cache.get_string_value<StrInx>(text_inx);
        }
        return dflt;
    }

    DataRef* cspec_data_ref(CacheSpecifier spec, DataRefMap& data_ref_map) {
        auto cs_dref_iter = data_ref_map.find(spec);
        if (cs_dref_iter != data_ref_map.end()) return &(cs_dref_iter->second);
        return nullptr;
    }
    
    // void dispatch_render(const JSON& w) {
    void dispatch_render(WidgetPtr w) {
        const static char* method = "NDContext::dispatch_render: ";
        // w["rname"] resolve & invoke
        switch (w->rname) {
        case RenderMethod::Noop:
            render_noop(w);
            break;
        case RenderMethod::Home:
            render_home(w);
            break;
        case RenderMethod::InputInt:
            render_input_int(w);
            break;
        case RenderMethod::Combo:
            render_combo(w);
            break;
        case RenderMethod::Checkbox:
            render_checkbox(w);
            break;
        case RenderMethod::Text:
            render_text(w);
            break;
        case RenderMethod::Button:
            render_button(w);
            break;
        case RenderMethod::Table:
            render_table(w);
            break;
        case RenderMethod::Footer:
            render_footer(w);
            break;
        case RenderMethod::DatePicker:
            render_date_picker(w);
            break;
        case RenderMethod::DuckTableSummaryModal:
            render_duck_table_summary_modal(w);
            break;
        case RenderMethod::LoadingModal:
            render_loading_modal(w);
            break;
        case RenderMethod::Separator:
            render_separator(w);
            break;
        case RenderMethod::SameLine:
            render_same_line(w);
            break;
        case RenderMethod::NewLine:
            render_new_line(w);
            break;
        case RenderMethod::Spacing:
            render_spacing(w);
            break;
        case RenderMethod::AlignTextToFramePadding:
            render_align_text_to_frame_padding(w);
            break;
        case RenderMethod::BeginChild:
            render_begin_child(w);
            break;
        case RenderMethod::EndChild:
            render_end_child(w);
            break;
        case RenderMethod::BeginGroup:
            render_begin_group(w);
            break;
        case RenderMethod::EndGroup:
            render_end_group(w);
            break;
        case RenderMethod::PushFont:
            render_push_font(w);
            break;
        case RenderMethod::PopFont:
            render_pop_font(w);
            break;
        default:
            // TODO: error
            break;
        }
    }

    void compound_string(char* buf, int buflen, const std::string& s1,
                            const std::string& s2, const std::string& sep) {
        // First, compose the compound action sequence key from action_id and nd_event
        // NB action_id will be a widget_id or query_id
        char* dest = buf;
        int space = buflen;
        memset(dest, 0, space);
        strncpy(dest, s1.c_str(), space);
        dest += s1.size();
        space -= s1.size();
        strncpy(dest, sep.c_str(), space);
        dest += sep.size();
        space -= sep.size();
        strncpy(dest, s2.c_str(), space);
    }

    // void action_dispatch(const std::string& action_id, const std::string& nd_event) {
    void action_dispatch(EntityInx ninx, EventInx einx) {

        const static char* method = "NDContext::action_dispatch: ";

        /*
        compound_string(act_ev_key_buf, act_ev_key_buf_len, 
            action_id, nd_event, Static::period_cs);

        NDLogger::cout() << method << "ACT_EV(" << act_ev_key_buf << ")" << std::endl;

        if (!JContains(data, Static::actions_cs)) {
            NDLogger::cout() << method << "no actions in data!" << std::endl;
            return;
        } 
        // Get hold of "actions" in data: do we a matching action?
        // NB if there isn't a matching action.event compound key in data.actions,
        // then we have to check the in flight sequences...
        const JSON& actions(data[Static::actions_cs]);
        JSON action_seq = JSON::array(); */
        std::list<InFlight> new_in_flight_list;
        ActionKey akey{ ninx, einx };
        ActionVec* avec = data_lay_cache.get_action_vec(akey);
        // if (JContains(actions, act_ev_key_buf)) {
        if (avec) {
            InFlight resume;
            // we have a match in data.actions to kick off a sequence
            // action_seq = actions[act_ev_key_buf];
            // action_execute(action_seq, 0, resume);
            action_execute(avec, 0, resume);
            // If there's a continuation, add to new list
            // if (resume.next != nullptr)
            if (resume.next.is_valid())
                new_in_flight_list.emplace_back(resume);
        }
        // If there's an action specified in data.actions, we've executed it,
        // and captured the resumption in InFlight resume. So now we check if
        // there were any in flight sequences waiting for an action.event
        auto if_iter = in_flight_list.begin();
        while (if_iter != in_flight_list.end()) {
            if (if_iter->query_id == ninx /*action_id*/ && /*nd_event*/einx == if_iter->next) {
                // we have an in flight match with action.event so execute
                // then add any resumption to new list
                InFlight resume;
                action_execute(if_iter->sequence, if_iter->inx, resume);
                // if (resume.next != nullptr)
                if (resume.next.is_valid())
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
    // void action_execute(JSON& action_seq, int action_inx, InFlight& resume) {
    void action_execute(ActionVec* action_seq, int action_inx, InFlight& resume) {
        const static char* method = "NDContext::action_execute: ";
        int seq_len = action_seq->size(); // JSize(action_seq);
        if (action_inx >= seq_len) {
            NDLogger::cerr() << method << "ACT_EXEC: inx(" << action_inx
                << ") >= len(" << seq_len << ")" << std::endl;
            return;
        }
        // const JSON& action_defn(action_seq[action_inx]);
        const NDAction& action_defn{ (*action_seq)[action_inx] };
        // Now we have a matched action definition in hand we can look
        // for UI push/pop and DB scan/query. If there's both push and pop,
        // pop goes first naturally!
        // if (JContains(action_defn, Static::ui_pop_cs)) {
        if (render_is_valid(action_defn.pop_ui)) {
            // for pops we supply the rname, not the pushable name so
            // the context can check the widget type on pops
            // std::string rname(JAsString(action_defn, Static::ui_pop_cs));
            // NDLogger::cout() << method << "ui_pop(" << rname << ")" << std::endl;
            // pending_pops.push_back(rname);
            pending_pops.push_back(action_defn.pop_ui);
        }
        // if (JContains(action_defn, Static::ui_push_cs)) {
        if (action_defn.push_ui.is_valid()) {
            // for pushes we supply widget_id, not the rname
            // std::string widget_id(JAsString(action_defn, Static::ui_push_cs));
            // uint32_t int_widget_id{ 0 };
            // TODO: memoize action_defn to yield widget_id as uint32, not str
            // salt'n'pepa in da house!
            /*
            auto find_it = entity_indices.find(widget_id);
            if (find_it == entity_indices.end()) {
                NDLogger::cerr() << method << "ui_push(" << widget_id << ") no such pushable" << std::endl;
            }
            else {
                // TODO: fix after C2 refactor
                int_widget_id = find_it->second;
            } */
            // new above, slightly amended below...
            // auto push_it = pushable.find(action_defn.push_ui);
            pending_pushes.push_back(action_defn.push_ui);
            /*
            if (push_it != pushable.end()) {
                NDLogger::cout() << method << "ui_push(" << action_defn.push_ui << ")" << std::endl;
                // NB action_dispatch is called by eg render_button, which ultimately is called
                // by render(), which iterates over stack. So we cannot change stack here...
                pending_pushes.push_back(push_it->second);
            }
            else {
                NDLogger::cerr() << method << "ui_push(" << action_defn.push_ui << ") no such pushable" << std::endl;
            }*/
        }
        // Finally, do we have a DB op to handle?
        // TODO: rejig!
        std::string db_action;
        std::string query_id;
        std::string sql_cache_key;
        if (db_event_is_valid(action_defn.db_action)) {
        // if (JContains(action_defn, Static::db_action_cs)) {
            db_dispatch(action_defn);
            /*
            if (action_defn.db_action == dbBatchRequest) {
                db_dispatch(action_defn.db_)
            }
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
            }*/
            // We've dispatched a DB op from a sequence, so there's a 
            // continuation if this is not the last action.
            if (++action_inx < seq_len) {
                resume.sequence = action_seq;
                resume.inx = action_inx;
                resume.query_id = action_defn.query_id;
                // resume.next = NextEvent(db_action.c_str());
                resume.next = next_db_event(action_defn.db_action);
            }
        }
    }

    /*
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
    } */

    void db_dispatch(const NDAction& action_defn) {
        auto db_request = JNewObject();
        JSet(db_request, Static::nd_type_cs, DBEventTypeToString(action_defn.db_action));
        const char* qid = data_lay_cache.get_string_value(action_defn.query_id);
        if (qid != nullptr) {
            JSet(db_request, Static::query_id_cs, qid);
        }
        if (action_defn.sql_cname.is_valid()) {
            const char* sql = data_lay_cache.get_addr_value(action_defn.sql_cname);
            if (sql != nullptr) {
                JSet(db_request, Static::sql_cs, sql);
            }
        }

    }

    // Render functions
    void render_noop(WidgetPtr) { }

    void render_home(WidgetPtr w) {
        const static char* method = "NDContext::render_home: ";

        /*
        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec( w[Static::cspec_cs]);
        std::string title(Static::nodom_cs);
        if (JContains(cspec, Static::title_cs)) {
            title = JAsString(cspec, Static::title_cs);
        } */

        const char* title = cspec_string(cs_title, w->cspec_str, method);
        LocalFont title_font(w, cs_title_font, cs_title_font_size);
        // Static::title_font_cs, Static::title_font_size_cs);

        /*
        bool fpop = false;
        if (JContains(cspec, Static::title_font_cs))
            fpop = push_font(w, Static::title_font_cs, Static::title_font_size_cs);
            */

        ImGui::Begin(title); //  .c_str());
        if (cache_is_loaded()) {
            // const JSON& children(w[Static::children_cs]);
            // int child_count = JSize(children);
            for (int inx = 0; inx < w->children.size(); inx++) {
                // const JSON& child(children[inx]);
                dispatch_render(w->children[inx]);
            }
        }
        else {
            // we're using init_layout/data, so render last crit error
            if (show_stopper != Clear) {
                switch (show_stopper) {
                case WebSockConnectionFailed:
                    if (critical_messages[WebSockConnectionFailed].empty()) {
                        compound_string(string_buffer, STR_BUF_LEN,
                            Static::could_not_connect_cs,
                            proxy.get_server_url(), Static::space_cs);
                        critical_messages[WebSockConnectionFailed] = std::string(string_buffer);
                    }
                    for (int i = WebSockConnectionFailed; i < EndCritical; ++i) {
                        ImGui::Text(critical_messages[WebSockConnectionFailed].c_str());
                    }
                }
            }
        }
        /* if (fpop) {
            pop_font();
        } */
        ImGui::End();
    }

    void render_input_int(WidgetPtr w) {
        const static char* method = "NDContext::render_input_int: ";

        int step = 1;
        cspec_int(cs_step, w->cspec_int, &step);
        int step_fast = 1;
        cspec_int(cs_step_fast, w->cspec_int, &step);
        int flags = 0;
        cspec_int(cs_flags, w->cspec_int, &flags);

        DataRef* int_data_ref = cspec_data_ref(cs_cname, w->data_refs);
        assert(int_data_ref != nullptr);
        assert(int_data_ref->tipe == cdInt);
        IntInx iinx{ int_data_ref->ref_inx };
        int* input_integer = data_lay_cache.get_int_value(iinx);
        assert(input_integer != nullptr);
        int old_val = *input_integer;

        // get hold of the cname addr to use as default label value
        AddrInx addr{int_data_ref->addr_inx};
        const char* cname = data_lay_cache.get_addr_value(addr);
        const char* label = cspec_string(cs_label, w->cspec_str, cname);

        ImGui::InputInt(label, input_integer, step, step_fast, flags);
        // copy local copy back into cache
        if (*input_integer != old_val) {
            notify_server(int_data_ref, old_val, *input_integer);
        }
    }

    void render_combo(WidgetPtr w) {
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
        /*
        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << JPrettyPrint(w) << ")" << std::endl;
            return;
        }
        const JSON& cspec(w["cspec"]); 
        std::string label; */

        int step = 1;
        const char* label = cspec_string(cs_label, w->cspec_str, method);
        cspec_int(cs_step, w->cspec_int, &step);

        // if (JContains(cspec, Static::text_cs)) label = JAsString(cspec, Static::text_cs);
        // if (JContains(cspec, Static::step_cs)) step = JAsInt(cspec, Static::step_cs);
        // no value params in layout here; all combo layout is data cache refs
        // /cspec/cname should give us a data cache addr for the combo list
        // std::string combo_list_cache_addr(JAsString(cspec, Static::cname_cs));
        // std::string combo_index_cache_addr(JAsString(cspec, Static::cindex_cs));

        DataRef* combo_list_data_ref = cspec_data_ref(cs_cname, w->data_refs);
        DataRef* combo_inx_data_ref = cspec_data_ref(cs_cindex, w->data_refs);
        if (combo_list_data_ref != nullptr && combo_inx_data_ref != nullptr) {
            assert(combo_list_data_ref->tipe == cdStrVec);
            assert(combo_list_data_ref->size < ND_MAX_COMBO_LIST);
            StrInx sinx{ combo_list_data_ref->ref_inx };
            int combo_count = 0;
            for (; combo_count < combo_list_data_ref->size; combo_count++) {
                cs_combo_list[combo_count] = data_lay_cache.get_string_value(sinx++);
            }
            IntInx iinx{ combo_inx_data_ref->ref_inx };
            int* combo_index = data_lay_cache.get_int_value(iinx);
            assert(combo_index != nullptr);
            int old_val = *combo_index;
            ImGui::Combo(label, combo_index, cs_combo_list, combo_count, combo_count);
            int new_val = *combo_index;
            if (old_val != new_val) {
                notify_server(combo_inx_data_ref, old_val, new_val);
            }
        }

        // if no label use cache addr
        /*
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
        } */
    }

    void render_checkbox(WidgetPtr w) {
        const static char* method = "NDContext::render_checkbox: ";

        /*
        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string cname_cache_addr(JAsString(cspec, Static::cname_cs));
        std::string text(cname_cache_addr);
        if (JContains(cspec, Static::text_cs))
            text = JAsString(cspec, Static::text_cs); */

        const char* button_text = cspec_string(cs_text, w->cspec_str, method);

        DataRef* ibool_data_ref = cspec_data_ref(cs_cname, w->data_refs);
        if (ibool_data_ref != nullptr &&
            ibool_data_ref->tipe == cdBool) {
            IntInx sinx{ ibool_data_ref->ref_inx };
            int* ibool = data_lay_cache.get_int_value(sinx);
            if (ibool != nullptr) {
                bool old_val = *ibool;
                bool new_val = old_val;
                ImGui::Checkbox(button_text, &new_val);
                if (old_val != new_val) {
                    notify_server(ibool_data_ref, old_val, new_val);
                }
            }
        }

        /*
        bool checked_value = JAsBool(data, cname_cache_addr.c_str());
        bool old_checked_value = checked_value;
        ImGui::Checkbox(text.c_str(), &checked_value);

        if (checked_value != old_checked_value) {
            JSet(data, cname_cache_addr.c_str(), checked_value);
            JSON j_old_checked_value{ old_checked_value };
            JSON j_checked_value{ checked_value };
            notify_server(cname_cache_addr, j_old_checked_value, j_checked_value);
        } */
    }

    void render_footer(WidgetPtr w) {
        static const char* method = "NDContext::render_footer: ";

        /*
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
        */

        // TODO: understand ems mem anlytics and restore in footer
        // bool memory = w.value(nlohmann::json::json_pointer("/cspec/memory"), true);
        cspec_bool(cs_db, w->cspec_bool, &footer_show_db);
        cspec_bool(cs_fps, w->cspec_bool, &footer_show_fps);
        cspec_bool(cs_demo, w->cspec_bool, &footer_show_demo);
        cspec_bool(cs_id_stack, w->cspec_bool, &footer_show_id_stack);
        cspec_bool(cs_font_scale, w->cspec_bool, &footer_show_font_scale);
        cspec_bool(cs_style, w->cspec_bool, &footer_show_style);


        ImGui::BeginGroup();
        if (footer_show_db) {
            // Push colour styling for the DB button
            ImGui::PushStyleColor(ImGuiCol_Button, (ImU32)db_status_color);
            if (ImGui::Button("DB")) {
                // action_dispatch(Static::i_am_footer_db_button_cs, Static::click_cs);
                action_dispatch(ninx_FooterDBButton, einx_Click);
            }
            ImGui::PopStyleColor(1);
        }
        if (footer_show_fps) {
            ImGui::SameLine();
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        if (footer_show_demo) {
            bool show_demo{ (bool)footer_show_demo };
            ImGui::Checkbox("Demo", &show_demo);
            if (show_demo)  ImGui::ShowDemoWindow();
            footer_show_demo = show_demo;
        }
        if (footer_show_id_stack) {
            bool show_id_stack{ (bool)footer_show_id_stack };
            ImGui::SameLine();
            ImGui::Checkbox("IDStack", &show_id_stack);
            if (show_id_stack)  ImGui::ShowStackToolWindow();
        }
        /* TODO
        if (memory) {

        } */
        if (footer_show_font_scale) {
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
        if (footer_show_style) {
            static const char* cs_combo_list[3] = { Static::dark_cs, Static::light_cs, Static::classic_cs };
            int old_val = style_coloring;
            ImGui::Combo(Static::lb_footer_style_cs, &style_coloring, cs_combo_list, 3, 3);
            if (style_coloring != old_val) SetStyleColoring(style_coloring);
        }
        ImGui::EndGroup();
    }

    // layout "jiggler": see imgui.h "Other layout functions"
    void render_separator(WidgetPtr) {
        ImGui::Separator();
    }

    void render_same_line(WidgetPtr) {
        ImGui::SameLine();
    }

    void render_new_line(WidgetPtr) {
        ImGui::NewLine();
    }

    void render_spacing(WidgetPtr) {
        ImGui::Spacing();
    }

    void render_align_text_to_frame_padding(WidgetPtr) {
        ImGui::AlignTextToFramePadding();
    }

    void render_date_picker(WidgetPtr w) {
        const static char* method = "NDContext::render_date_picker: ";

        static int default_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedSame;
        static int default_combo_flags = ImGuiComboFlags_HeightRegular;
        static std::vector<int> ymd_i = { 0, 0, 0 };
        ImFont* year_month_font = nullptr;
        ImFont* day_date_font = nullptr;
        uint32_t year_month_font_size_base = 0;
        uint32_t day_date_font_size_base = 0;
        /*
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
            */
    }


    void render_text(WidgetPtr w) {
        const static char* method = "NDContext::render_text: ";
        /*
        if (!JContains(w, Static::cspec_cs)) {
            NDLogger::cerr() << method << "no cspec in w(" << w << ")" << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string rtext = JAsString(cspec, Static::text_cs);
        */
        const char* rtext = cspec_string(cs_text, w->cspec_str, method);
        ImGui::Text(rtext);
    }

    void render_button(WidgetPtr w) {
        const static char* method = "NDContext::render_button: ";

        /*
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
        }*/
        const char* button_text = cspec_string(cs_text, w->cspec_str, method);

        if (ImGui::Button(button_text)) {
            // action_dispatch(widget_id, Static::click_cs);
            action_dispatch(w->widget_inx, einx_Click);
        }
    }

    constexpr static int SMRY_COLM_CNT = 12;
    
    void render_duck_table_summary_modal(WidgetPtr w) {
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
        /*
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
            */

        const char* title = cspec_string(cs_title, w->cspec_str, method);

        int table_flags = default_summary_table_flags;
        int window_flags = default_window_flags;
        cspec_int(cs_table_flags, w->cspec_int, &table_flags);
        cspec_int(cs_window_flags, w->cspec_int, &window_flags);
        /*
        if (JContains(cspec, Static::table_flags_cs)) {
            table_flags = JAsInt(cspec, Static::table_flags_cs);
        }
        if (JContains(cspec, Static::window_flags_cs)) {
            window_flags = JAsInt(cspec, Static::window_flags_cs);
        }*/

        /*
        bool title_pop = false;
        if (JContains(cspec, Static::title_font_cs))
            title_pop = push_font(w, Static::title_font_cs, Static::title_font_size_cs);
            */
        if (title) {    // scope to fire title_font dtor and pop before body
            LocalFont title_font(w, cs_title_font, cs_title_font_size);

            ImGui::OpenPopup(title); // .c_str());
            // Always center this window when appearing
            ImGuiViewport* vp = ImGui::GetMainViewport();
            if (!vp) {
                NDLogger::cerr() << method << "NULL_VP_PTR!" << std::endl;
                return;
            }
            auto center = vp->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5, 0.5 });
        }
        else {
            NDLogger::cerr() << method << "NO_TITLE" << std::endl;
            return;
        }

        std::uint64_t colm_count = SMRY_COLM_CNT;
        int colm_index = 0;
        bool body_pop = false;
        if (ImGui::BeginPopupModal(title/*.c_str()*/, nullptr, window_flags)) {
            // if (title_pop) pop_font();
            LocalFont body_font(w, cs_body_font, cs_body_font_size);
            /*
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
            */
            ImGui::Separator();
        }
        /*
        if (body_pop) pop_font();

        bool button_pop = false;
        if (JContains(cspec, Static::button_font_cs))
            button_pop = push_font(w, Static::button_font_cs, Static::button_font_size_cs);
            */

        LocalFont button_font(w, cs_button_font, cs_button_font_size);

        // Note we do not invoke pop_widget() here as we're
        // rendering, and that would change the stack while
        // the topmost render method is iterating over it.
        // Instead we add it to the list of pending_pops
        // so top level render will invoke pop_widget for us.
        // NB we pop_widget asymmetrically with an rname,
        // not a widget_id
        if (ImGui::Button(Static::ok_cs)) {
            ImGui::CloseCurrentPopup();
            pending_pops.push_back(DuckTableSummaryModal);
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button(Static::cancel_cs)) {
            ImGui::CloseCurrentPopup();
            pending_pops.push_back(DuckTableSummaryModal);
        }

        // if (button_pop) pop_font();

        ImGui::EndPopup();
    }

    void render_loading_modal(WidgetPtr w) {
        const static char* method = "NDContext::render_loading_modal: ";

        static ImVec2 position = { 0.5, 0.5 };
        /*
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
            */

        const char* title = cspec_string(cs_title, w->cspec_str, method);

        /*
        bool tpop = false;
        if (JContains(cspec, Static::title_font_cs))
            tpop = push_font(w, Static::title_font_cs, Static::title_font_size_cs);
        */
        LocalFont title_font(w, cs_title_font, cs_title_font_size);

        ImGui::OpenPopup(title); // .c_str());

        // Always center this window when appearing
        ImGuiViewport* vp = ImGui::GetMainViewport();
        if (!vp) {
            NDLogger::cerr() << method << title << ": NULL_VP_PTR";
        }
        auto center = vp->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, position);

        // Get the text list
        // StringVec text_vec;
        // JAsStringVec(data, cname_cache_addr.c_str(), text_vec);
        DataRef* str_vec_data_ref = cspec_data_ref(cs_cname, w->data_refs);

        if (ImGui::BeginPopupModal(title/*.c_str()*/, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            LocalFont body_font(w, cs_body_font, cs_body_font_size);
            /*
            bool bpop = false;
            if (JContains(cspec, Static::body_font_cs))
                bpop = push_font(w, Static::body_font_cs, Static::body_font_size_cs);
            for (auto text_iter = text_vec.begin(); text_iter != text_vec.end(); ++text_iter) {
                    ImGui::TextUnformatted(text_iter->c_str());
            } */
            if (str_vec_data_ref != nullptr &&
                str_vec_data_ref->tipe == cdStr) {
                StrInx sinx{str_vec_data_ref->ref_inx};
                for (int i = 0; i < str_vec_data_ref->size; i++) {
                    const char* text = data_lay_cache.get_string_value(sinx);
                    if (text) ImGui::TextUnformatted(text);
                    sinx++;
                }
            }
            // if (bpop) pop_font();
            int spinner_radius = 5;
            int spinner_thickness = 2;
            cspec_int(cs_spinner_radius, w->cspec_int, &spinner_radius);
            cspec_int(cs_spinner_thickness, w->cspec_int, &spinner_thickness);
            /*
            if (JContains(cspec, Static::spinner_radius_cs))
                spinner_radius = JAsInt(cspec, Static::spinner_radius_cs);
            if (JContains(cspec, Static::spinner_thickness_cs))
                spinner_thickness = JAsInt(cspec, Static::spinner_thickness_cs);
                */
            if (!ImGui::Spinner(Static::i_am_loading_spinner_cs, (float)spinner_radius, spinner_thickness, 0)) {
                // TODO: spinner always fails IsClippedEx on first render
                NDLogger::cout() << method << "SPINNER_FAIL" << std::endl;
            }
            ImGui::EndPopup();
        }
        // if (tpop) pop_font();
    }

    void render_table(WidgetPtr w) {
        const static char* method = "NDContext::render_table: ";
        static int default_summary_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
        static int default_window_flags = ImGuiWindowFlags_AlwaysAutoResize;

        /*
        if (!JContains(w, Static::cspec_cs) || !JContains(w[Static::cspec_cs], Static::qname_cs)) {
            NDLogger::cerr() << method << "BAD_CSPEC" << w << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        const std::string qname(JAsString(cspec, Static::qname_cs));*/

        int table_flags = default_summary_table_flags;
        cspec_int(cs_table_flags, w->cspec_int, &table_flags);
        /*
        if (JContains(cspec, Static::table_flags_cs)) {
            table_flags = JAsInt(cspec, Static::table_flags_cs);
        }*/

        int window_flags = default_window_flags;
        cspec_int(cs_window_flags, w->cspec_int, &window_flags);
        /*
        if (JContains(cspec, Static::window_flags_cs)) {
            window_flags = JAsInt(cspec, Static::window_flags_cs);
        }*/

        // TODO: recode proxy.get_meta_data() to lazy load 
        // and report geom
        std::uint64_t colm_count = 0;
        std::uint64_t row_count = 0;
        int colm_index = 0;
        /*
        bool body_pop = false;
        if (JContains(cspec, Static::body_font_cs))
            body_pop = push_font(w, Static::body_font_cs, Static::body_font_size_cs);
            */
        LocalFont body_font(w, cs_body_font, cs_body_font_size);
        /*
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
        }*/
    }

    void render_push_font(WidgetPtr w) {
        push_font(w, cs_font, cs_font_size);
    }

    void render_pop_font(WidgetPtr) {
        pop_font();
    }

    void render_begin_child(WidgetPtr w) {
        const static char* method = "NDContext::render_begin_child: ";

        // NB BeginChild is a grouping mechanism, so there's no single
        // cache datum to which we refer, so no cname. However, we do look
        // require a title (for imgui ID purposes) and we can have styling
        // attributes like ImGuiChildFlags
        /*
        if (!JContains(w, Static::cspec_cs) || !JContains(w[Static::cspec_cs], Static::title_cs)) {
            NDLogger::cerr() << method << Static::BAD_CSPEC_cs << JPrettyPrint(w) << std::endl;
            return;
        }
        const JSON& cspec(w[Static::cspec_cs]);
        std::string title(JAsString(cspec, Static::title_cs));
        */
        // default title
        const char* title = cspec_string(cs_title, w->cspec_str, method);

        int child_flags = 0;
        // TODO: recover child flags from widget
        ImVec2 size{ 0, 0 };
        // TODO: we're not checking the ret val to see if we
        // need to render the children. We need a way to
        // memoize the BeginChild RV on the stack...
        ImGui::BeginChild(title, size, child_flags);
    }

    void render_end_child(WidgetPtr w) {
        ImGui::EndChild();
    }

    void render_begin_group(WidgetPtr w) {
        ImGui::BeginGroup();
    }

    void render_end_group(WidgetPtr w) {
        ImGui::EndGroup();
    }

    void push_widget(WidgetPtr w) {
        stack.push_back(w);
    }

    void pop_widget(RenderMethod m) {
        const static char* method = "NDContext::pop_widget: ";
        // if rname specifies a class we check the
        //      popped widget rname
        if (stack.back()->rname == m) {
            stack.pop_back();
        }
        else {
            NDLogger::cerr() << method << "POP_FAIL mismatch rname("
                << data_lay_cache.render_name(m) << ")" << std::endl;
        }
    }

    bool push_font(WidgetPtr w, CacheSpecifier cs_font_name, 
                                        CacheSpecifier cs_font_size) {
        const static char* method = "NDContext::push_font: ";

        // get size if specified, otherwise default to 0
        int font_size_base = 0;
        cspec_int(cs_font_size, w->cspec_int, &font_size_base);
        /*
        auto cs_int_it = w->cspec_int.find(font_size_spec);
        if (cs_int_it != w->cspec_int.end()) {
            int* font_size_val_ptr = data_lay_cache.get_int_value(cs_int_it->second);
            if (font_size_val_ptr != nullptr)
                font_size_base = *font_size_val_ptr;
        } */

        // defaults so resolution failure doesn't stop us pushing
        ImFont* font = font_map[Static::default_cs];
        const char* font_name = cspec_string(cs_font_name, w->cspec_str, Static::default_cs);
        if (font_name != nullptr) {
            auto font_it = font_map.find(font_name);
            if (font_it != font_map.end()) font = font_it->second;
        }

        /*
        auto cs_str_it = w->cspec_str.find(font_name_spec);
        if (cs_str_it != w->cspec_str.end()) {
            StrInx font_name_inx{ cs_str_it->second };
            const char* font_name = data_lay_cache.get_string_value<StrInx>(font_name_inx);
            if (font_name != nullptr) {
                auto font_it = font_map.find(font_name);
                if (font_it != font_map.end()) {
                    font = font_it->second;
                }
                else {
                    bad_font_pushes.push_back(font_name_inx);
                }
            }
        } */

        ImGui::PushFont(font, font_size_base);
        font_push_count++;
        return true;
    }

    void pop_font() {
        ImGui::PopFont();
        font_pop_count++;
    }
};


