#pragma once
#include <string>
#include <map>
#include <queue>
#include <functional>
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "locals.hpp"
#include "ufuncs.hpp"
#include "widgets.hpp"
#include "db_cache.hpp"
#include "dl_cache.hpp"
#include "logger.hpp"
#include "ems_idb.hpp"

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
    DB&                     proxy;  // DB proxy decouples NDContext from DB detail
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
    std::string         app_key_s;
    std::string         ini_path;
    std::string         init_data_s;
    std::string         init_layout_s;
    JSON                real_data;
    JSON                real_layout;
    bool                data_loaded{ true };
    bool                layout_loaded{ true };

    std::string         server_url;

    // manage down logging by tracking misconfiged queries that throw
    // BAD_HANDLE_FAIL, especially for browser console log
    std::map<std::string, uint32_t>   bad_handle_map;

    // action_dispatch is called while rendering, and changes
    // the size of the render stack. JS will let us do that in the root
    // render() method. But in C++ we use an STL iterator in the root render
    // method, and that segfaults. So in C++ we have pending pushes done
    // outside the render stack walk. JOS 2025-01-31
    std::deque<EntityInx>       pending_pushes;
    std::deque<RenderMethod>    pending_pops;
    // In flight action sequences. Key is query_id, not query_id.event
    struct InFlight {
        InFlight() {}
        InFlight(ActionVec* avec, int i, EventInx n, EntityInx qid)
            :sequence(avec), inx(i), next(n), query_id(qid) {}
        ActionVec* sequence;
        int         inx{ 0 };
        EventInx next;
        EntityInx query_id;
    };
    std::list<InFlight> in_flight_list;

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

    bool  footer_show_db{ false };
    bool  footer_show_fps{ false };
    bool  footer_show_demo{ false };
    bool  footer_show_id_stack{ false };
    bool  footer_show_font_scale{ false };
    bool  footer_show_style{ false };

    int   style_coloring{ StyleColor::Dark };   // match im_start StyleColorsDark()
    bool  show_demo{ false };
    bool  show_id_stack{ false };
    bool  show_memory{ false};

    // colours: https://www.w3schools.com/colors/colors_picker.asp
    ImColor red;    // ImGui.COL32(255, 51, 0);
    ImColor green;  // ImGui.COL32(102, 153, 0);
    ImColor amber;  // ImGui.COL32(255, 153, 0);
    ImColor db_status_color;

    const ImVec4    vec4z{ 0.0, 0.0, 0.0, 0.0 };
    const ImVec2    vec2z{ 0.0, 0.0 };
    ImVec2          vec2;

    std::map<std::string, ImFont*>  font_map;
    std::uint32_t font_push_count = 0;
    std::uint32_t font_pop_count = 0;
    std::uint32_t render_count = 0;
    std::deque<StrInx> bad_font_pushes;

    VSFunc ws_send = nullptr;  // ref to NDWebSockClient::send
    VVFunc msg_pump = nullptr;
    GLFWwindow* glfw_window = nullptr;

    // Buffer for action_event compound keys
    char act_ev_key_buf[act_ev_key_buf_len];
    // Buffers for err msg fmt, labels, dates...
    char string_buffer[STR_BUF_LEN];
    char date_buffer[STR_BUF_LEN];
    // "local variable": normally method locals,
    // but we want to minimise stack thrashing
    fmt::format_to_n_result<char*> fmt_result;
    DatePickerLocals    dp_vars;
    SpinnerLocals       sp_vars;
    ShadedPlotLocals    sh_pl_vars;
#ifdef __EMSCRIPTEN__
    IDBFileWriter       ini_writer;
    IDBFileCachePtr     ini_cache_ptr;
    StringVec           ini_list;
#endif

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
    NDContext(DB& s, const std::string& app_key, const std::string& ipath, const char* idata = nullptr, const char* ilayout = nullptr)
        :proxy(s), app_key_s(app_key), ini_path(ipath),
        init_data_s(idata?idata:Static::init_data_cs),
        init_layout_s(ilayout?ilayout:Static::init_layout_cs),
        red{ 255, 51, 0 },
        green{ 102, 153, 0 },
        amber{ 255, 153, 0 }
    {
        // init status is not connected
        db_status_color = red;

        // cp lambdas into LocalFont statics
        LocalFont::push_func = [&](WidgetPtr w, CacheSpecifier nm, CacheSpecifier sz) {push_font(w, nm, sz); };
        LocalFont::pop_func = [&]() {pop_font(); };

        NDConfig<JSON>& cfg{ NDConfig<JSON>::get_instance() };
        cfg.get_value(Static::server_url_cs, server_url);
#ifdef __EMSCRIPTEN__
        ini_writer.file_name = app_key_s + "_layout.ini";
        ini_list.push_back(ini_writer.file_name);
        ini_cache_ptr.reset(new IDBFileCache(
            [](ImGuiIO& io, void* f, int sz)->char* {
                return LoadIniFromMem((char*)f, sz);
            },
            [](const std::string& n, void* f) {},
            ini_list
        ));
#endif
    }

    void initialize() {
        const char* method = "NDContext::initialize: ";
        // NDContext ctor is fired before im_start initializes imgui. Some
        // init tasks require imgui to be initialized; we do those here and
        // are invoked on the 1st render cycle
        // 
        // glfwSwapInterval invokes _emscripten_set_main_loop_timing, which
        // throws if main the main loop hasn't been started by emscripten_set_main_loop[_arg]
        // Not an issue for Breadboard, but it is an EMS break
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
        ninx_GUI = data_lay_cache.template get_string_index<CIT::EntityID>(Static::gui_cs, CST::SubSysID);
        ninx_Websock = data_lay_cache.template get_string_index<CIT::EntityID>(Static::websock_cs, CST::SubSysID);
        ninx_DuckDB = data_lay_cache.template get_string_index<CIT::EntityID>(Static::duck_db_cs, CST::SubSysID);

        // EntityIDs for predefined widgets like LoadingModal
        ninx_FooterDBButton = data_lay_cache.template get_string_index<CIT::EntityID>(Static::i_am_footer_db_button_cs, CST::WidgetID);

        // Events: subsys
        einx_WebSockConnectionFailed = data_lay_cache.template get_string_index<CIT::Event>(
                                        CriticalToString(WebSockConnectionFailed), CST::SubSysEvent);
        einx_Online = data_lay_cache.template get_string_index<CIT::Event>(Static::online_cs, CST::SubSysEvent);
        einx_CacheLoaded = data_lay_cache.template get_string_index<CIT::Event>(Static::cache_loaded_cs, CST::SubSysEvent);

        // Events: widget
        einx_Click = data_lay_cache.template get_string_index<CIT::Event>(Static::click_cs, CST::WidgetEvent);

        // Events: DB
        einx_Command = data_lay_cache.template get_string_index<CIT::Event>(Static::command_cs, CST::DBEvent);
        einx_CommandResult = data_lay_cache.template get_string_index<CIT::Event>(Static::command_result_cs, CST::DBEvent);
        einx_Query = data_lay_cache.template get_string_index<CIT::Event>(Static::query_cs, CST::DBEvent);
        einx_QueryResult = data_lay_cache.template get_string_index<CIT::Event>(Static::query_result_cs, CST::DBEvent);
        einx_BatchRequest = data_lay_cache.template get_string_index<CIT::Event>(Static::batch_request_cs, CST::DBEvent);
        einx_BatchResponse = data_lay_cache.template get_string_index<CIT::Event>(Static::batch_response_cs, CST::DBEvent);

        data_lay_cache.report_cache_state();
        size_t dlc_error_count = data_lay_cache.error_count();
        if (dlc_error_count > 0) {
            data_lay_cache.report_cache_errors();
        }
        else {
            // init the render stack
            stack.clear();
            stack.push_back(data_lay_cache.get_home());
#ifdef __EMSCRIPTEN__
            // Now we have the real layout, and not init layout,
            // load persisted layout details...
            ini_cache_ptr->next();
#endif
        }
    }

    // EventInx next_db_event(EventInx einx) {
    DBEventType next_db_event(DBEventType dbet) {
        switch (dbet) {
        case dbCommand:
            return dbCommandResult;
        case dbQuery:
            return dbQueryResult;
        case dbBatchRequest:
            return dbBatchResponse;
        default:
            return EndDBEventTypes;
        }
    }

    EventInx db_event_type_to_event_inx(DBEventType dbet) {
        switch (dbet) {
        case dbCommand:
            return einx_Command;
        case dbCommandResult:
            return einx_CommandResult;
        case dbQuery:
            return einx_Query;
        case dbQueryResult:
            return einx_QueryResult;
        case dbBatchRequest:
            return einx_BatchRequest;
        case dbBatchResponse:
            return einx_BatchResponse;
        default:
            return EndDBEventTypes;
        }
    }

    // Helpers and accessors
    GLFWwindow* get_glfw_window() { return glfw_window; }
    void        set_glfw_window(GLFWwindow* w) { glfw_window = w; }

    const char* get_ini_path() { return ini_path.empty() ? nullptr : ini_path.c_str(); }
    int* get_style_coloring() { return &style_coloring; }

    bool        cache_is_loaded() { return data_loaded && layout_loaded; }

    void set_critical(Critical error_code) {
        show_stopper = error_code;
        switch (error_code) {
        case WebSockConnectionFailed:
            action_dispatch(ninx_Websock, einx_WebSockConnectionFailed);
            break;
        case Clear:
        case EndCritical:
            break;  // TODO
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
        pix_report(RenderBadHandlePC, (float)bad_handle_sum);

        if (render_count % 120 == 0) {   // every 120 renders eg ~2 sec
            for (auto citer = bad_handle_map.cbegin(); citer != bad_handle_map.cend(); ++citer) {
                NDLogger::cout() << method << citer->first << " BHC: " << citer->second << std::endl;
            }
#ifdef __EMSCRIPTEN__
            ImGuiContext& g = *GImGui;
            ImGuiIO& io = ImGui::GetIO();
            if (io.WantSaveIniSettings) {
                ImGui::SaveIniSettingsToMemory();
                ini_writer.write(g.SettingsIniData.c_str(), g.SettingsIniData.size());
                io.WantSaveIniSettings = false;
            }
#endif
        }
    }

    void server_request(const std::string& key) {
        std::stringstream msgbuf;
        msgbuf << "{ \"" << Static::nd_type_cs << "\":\"" << Static::cache_request_cs << "\",\""
            << Static::cache_key_cs << "\":\"" << key << "\"}";
        ws_send(msgbuf.str());
    }

    // void notify_server(const std::string& caddr, JSON& old_val, JSON& new_val) {
    template <typename V, typename W>
    void notify_server(DataRef* dref, V& old_val, W& new_val) {

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
        // operator<< from dl_cache.hpp.
        msgbuf << "{ \"" << Static::nd_type_cs << "\":\"" << Static::data_change_cs << "\",\""
            << Static::cache_key_cs << "\":\"" << addr << "\",\""
            << Static::new_value_cs << "\":" << new_val << ",\""

            << Static::old_value_cs << "\":" << old_val << "}";
        ws_send(msgbuf.str());
    }

    template <typename V, typename W>
    void notify_server(DataRef* dref, V& old_val, W* new_val) {

        const static char* method = "NDContext::notify_server: ";

        const char* addr = data_lay_cache.get_addr_value(dref->addr_inx);
        const char* tipe = CDTToString(dref->tipe);
        std::cout << method << "type:" << tipe << ", addr:" << addr
            << ", old: " << old_val << ", new: " << *new_val << std::endl;

        std::stringstream msgbuf;
        // We supply wrapping "" for data_change_cs and caddr
        // as we know they're strings. For new_val and old_val
        // we rely on operator<< which is supplied by
        // nlohmann::json. For emscripten::val we use our own
        // operator<< from dl_cache.hpp.
        msgbuf << "{ \"" << Static::nd_type_cs << "\":\"" << Static::data_change_cs << "\",\""
            << Static::cache_key_cs << "\":\"" << addr << "\",\""
            << Static::new_value_cs << "\":" << *new_val << ",\""
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
                    NDLogger::cout() << method << "CACHE_LOADED" << std::endl;
                    action_dispatch(ninx_GUI, einx_CacheLoaded);
                }
            }
            // Is this a DataChange?
            else if (nd_type == Static::data_change_cs) {
                std::string ckey = JAsString(resp, Static::cache_key_cs);
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
        EntityInx ninx{data_lay_cache.template get_string_index<EntityID>(qid, CST::QueryID)};
        EventInx einx_db{ data_lay_cache.template get_string_index<Event>(nd_type, CST::DBEvent) };
        EventInx einx_ss{ data_lay_cache.template get_string_index<Event>(nd_type, CST::SubSysEvent) };

        if (einx_db == einx_Command) {
            db_status_color = amber;
        }
        else if (einx_db == einx_Query) {
            db_status_color = amber;
        }
        else if (einx_db == einx_CommandResult) {
            db_status_color = green;
            action_dispatch(ninx, einx_db); // qid, nd_type);
        }
        else if (einx_db == einx_QueryResult) {
            db_status_color = green;
            // Typically, a QueryResult is followed by dispatch
            // of a BatchRequest. 
            action_dispatch(ninx, einx_db); // qid, nd_type);
        }
        else if (einx_db == einx_BatchResponse) {
            db_status_color = green;
            action_dispatch(ninx, einx_db); // qid, nd_type);
        }
        else if (einx_ss == einx_Online) {
            // TODO: q processing order means this doesn't happen so early in cpp
            // main.ts:on_duck_event invokes check_duck_module.
            // However, we don't need all the check_duck_module JS module stuff,
            // so we can just flip status button color here
            db_status_color = amber;
            // signal DuckDB online
            action_dispatch(ninx_DuckDB, einx_Online);
        }
        else {
            NDLogger::cerr() << method << JPrettyPrint(db_msg) << std::endl;
        }
    }

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

    bool* cspec_bool(CacheSpecifier spec, BoolValMap& bool_val_map, bool* target = nullptr) {
        auto cs_bool_iter = bool_val_map.find(spec);
        if (cs_bool_iter != bool_val_map.end()) {
            BoolInx bool_inx{ cs_bool_iter->second };
            bool* rv = data_lay_cache.get_bool_value(bool_inx);
            if (target != nullptr && rv != nullptr) *target = *rv;
            return rv;
        }
        return nullptr;
    }


    const char* cspec_string(CacheSpecifier spec, StrValMap& str_val_map, const char* dflt) {
        auto cs_str_iter = str_val_map.find(spec);
        if (cs_str_iter != str_val_map.end()) {
            StrInx text_inx{ cs_str_iter->second };
            return data_lay_cache.template get_string_value<StrInx>(text_inx);
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
        // const static char* method = "NDContext::dispatch_render: ";
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
        case RenderMethod::Window:
            render_window(w);
            break;
        case RenderMethod::ShadedPlot:
            render_shaded_plot(w);
            break;

        default:
            // TODO: error
            break;
        }
    }

    void compound_string(char* buf, int buflen, const char* s1,
                            const char* s2, const char* sep = nullptr) {
        // First, compose the compound action sequence key from action_id and nd_event
        // NB action_id will be a widget_id or query_id
        char* dest = buf;
        size_t space = buflen;
        size_t slen = strlen(s1);
        memset(dest, 0, space);
        strncpy(dest, s1, space);
        dest += slen;
        space -= slen;
        if (sep != nullptr) {
            slen = strlen(sep);
            strncpy(dest, sep, space);
            dest += slen;
            space -= slen;
        }
        strncpy(dest, s2, space);
    }

    void action_dispatch(EntityInx ninx, EventInx einx) {
        // const static char* method = "NDContext::action_dispatch: ";

        std::list<InFlight> new_in_flight_list;
        ActionKey akey{ ninx, einx };
        ActionVec* avec = data_lay_cache.get_action_vec(akey);
        if (avec) {
            InFlight resume;
            // we have a match in data.actions to kick off a sequence
            action_execute(avec, 0, resume);
            // If there's a continuation, add to new list
            if (resume.next.is_valid())
                new_in_flight_list.emplace_back(resume);
        }
        // If there's an action specified in data.actions, we've executed it,
        // and captured the resumption in InFlight resume. So now we check if
        // there were any in flight sequences waiting for an action.event
        auto if_iter = in_flight_list.begin();
        while (if_iter != in_flight_list.end()) {
            if (if_iter->query_id == ninx && einx == if_iter->next) {
                // we have an in flight match with action.event so execute
                // then add any resumption to new list
                InFlight resume;
                action_execute(if_iter->sequence, if_iter->inx, resume);
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


    void action_execute(ActionVec* action_seq, int action_inx, InFlight& resume) {
        const static char* method = "NDContext::action_execute: ";

        assert(action_seq != nullptr);
        size_t seq_len = action_seq->size(); // JSize(action_seq);
        if (action_inx >= seq_len) {
            NDLogger::cerr() << method << "ACT_EXEC: inx(" << action_inx
                << ") >= len(" << seq_len << ")" << std::endl;
            return;
        }
        const NDAction& action_defn{ (*action_seq)[action_inx] };
        // Now we have a matched action definition in hand we can look
        // for UI push/pop and DB scan/query. If there's both push and pop,
        // pop goes first naturally!
        if (render_is_valid(action_defn.pop_ui)) {
            // for pops we supply the rname, not the pushable name so
            // the context can check the widget type on pops
            pending_pops.push_back(action_defn.pop_ui);
        }
        if (action_defn.push_ui.is_valid()) {
            // for pushes we supply widget_id, not the rname
            pending_pushes.push_back(action_defn.push_ui);
        }
        // Finally, do we have a DB op to handle?
        if (db_event_is_valid(action_defn.db_action)) {
            db_dispatch(action_defn);
            // We've dispatched a DB op from a sequence, so there's a 
            // continuation if this is not the last action.
            if (++action_inx < seq_len) {
                resume.sequence = action_seq;
                resume.inx = action_inx;
                resume.query_id = action_defn.query_id;
                resume.next = db_event_type_to_event_inx(next_db_event(action_defn.db_action));
            }
        }
    }


    void db_dispatch(const NDAction& action_defn) {
        // const static char* method = "NDContext::db_dispatch: ";

        auto db_request = JNewObject();
        JSet(db_request, Static::nd_type_cs, DBEventTypeToString(action_defn.db_action));
        const char* qid = data_lay_cache.get_string_value(action_defn.query_id);
        assert(qid != nullptr);
        JSet(db_request, Static::query_id_cs, qid);
        // BatchRequest just needs QID, no SQL; Command and Query need SQL
        if (action_defn.db_action != dbBatchRequest) {
            assert(action_defn.sql_cname.is_valid());
            const char* sql_cname = data_lay_cache.get_addr_value(action_defn.sql_cname);
            assert(sql_cname != nullptr);
            DataRef* data_ref = data_lay_cache.get_data_ref(action_defn.sql_cname);
            assert(data_ref != nullptr);
            const char* sql = data_lay_cache.template get_string_value<AddrInx>(data_ref->ref_inx);
            assert(sql != nullptr);
            JSet(db_request, Static::sql_cs, sql);
        }
        proxy.db_dispatch(db_request);
    }

    // Render functions
    void render_noop(WidgetPtr) { }

    void render_home(WidgetPtr w) {
        const static char* method = "NDContext::render_home: ";

        const char* title = cspec_string(cs_title, w->cspec_str, method);
        LocalFont title_font(w, cs_title_font, cs_title_font_size);

        ImGui::Begin(title);
        if (cache_is_loaded()) {
            for (int inx = 0; inx < w->children.size(); inx++) {
                dispatch_render(w->children[inx]);
            }
        }
        else {
            // we're using init_layout/data, so render last crit error
            if (show_stopper != Clear) {
                switch (show_stopper) {
                case Clear:
                case EndCritical:
                    // TODO
                    break;
                case WebSockConnectionFailed:
                    if (critical_messages[WebSockConnectionFailed].empty()) {
                        compound_string(string_buffer, STR_BUF_LEN,
                            Static::could_not_connect_cs, server_url.c_str(), Static::space_cs);
                        critical_messages[WebSockConnectionFailed] = std::string(string_buffer);
                    }
                    for (int i = WebSockConnectionFailed; i < EndCritical; ++i) {
                        ImGui::TextUnformatted(critical_messages[WebSockConnectionFailed].c_str());
                    }
                }
            }
        }
        ImGui::End();
    }

    void render_input_int(WidgetPtr w) {
        // const static char* method = "NDContext::render_input_int: ";

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
        int* int_ptr = data_lay_cache.get_int_value(iinx);
        assert(int_ptr != nullptr);
        int old_val = *int_ptr;

        // get hold of the cname addr to use as default label value
        AddrInx addr{int_data_ref->addr_inx};
        const char* cname = data_lay_cache.get_addr_value(addr);
        const char* label = cspec_string(cs_label, w->cspec_str, cname);

        ImGui::InputInt(label, int_ptr, step, step_fast, flags);
        // copy local copy back into cache
        if (*int_ptr != old_val) {
            notify_server(int_data_ref, old_val, int_ptr);
        }
    }

    void render_combo(WidgetPtr w) {
        const static char* method = "NDContext::render_combo: ";
        // Static storage for the combo list. NB single GUI thread!
        // No malloc at runtime, but we will clear the array with a memset
        // on each visit. JOS 2025-01-26
        static const char* cs_combo_list[ND_MAX_COMBO_LIST];
        memset(cs_combo_list, 0, ND_MAX_COMBO_LIST * sizeof(char*));

        int step = 1;
        const char* label = cspec_string(cs_label, w->cspec_str, method);
        cspec_int(cs_step, w->cspec_int, &step);

        DataRef* combo_list_data_ref = cspec_data_ref(cs_cname, w->data_refs);
        DataRef* combo_inx_data_ref = cspec_data_ref(cs_cindex, w->data_refs);
        if (combo_list_data_ref != nullptr && combo_inx_data_ref != nullptr) {
            assert(combo_list_data_ref->tipe == cdStrVec);
            assert(combo_list_data_ref->size < ND_MAX_COMBO_LIST);
            StrInx sinx{ combo_list_data_ref->ref_inx };
            uint32_t combo_count = 0;
            for (; combo_count < combo_list_data_ref->size; combo_count++) {
                cs_combo_list[combo_count] = data_lay_cache.get_string_value(sinx);
                sinx++;
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
    }

    void render_checkbox(WidgetPtr w) {
        const static char* method = "NDContext::render_checkbox: ";

        const char* button_text = cspec_string(cs_text, w->cspec_str, method);
        DataRef* bool_data_ref = cspec_data_ref(cs_cname, w->data_refs);

        if (bool_data_ref != nullptr &&
            bool_data_ref->tipe == cdBool) {
            BoolInx binx{ bool_data_ref->ref_inx };
            bool* bool_ptr = data_lay_cache.get_bool_value(binx);
            if (bool_ptr != nullptr) {
                bool old_val = *bool_ptr;
                ImGui::Checkbox(button_text, bool_ptr);
                if (old_val != *bool_ptr) {
                    notify_server(bool_data_ref, old_val, bool_ptr);
                }
            }
        }
    }

    void render_footer(WidgetPtr w) {
        // static const char* method = "NDContext::render_footer: ";

        // TODO: understand ems mem anlytics and restore in footer
        cspec_bool(cs_show_footer_db, w->cspec_bool, &footer_show_db);
        cspec_bool(cs_show_footer_fps, w->cspec_bool, &footer_show_fps);
        cspec_bool(cs_show_footer_demo, w->cspec_bool, &footer_show_demo);
        cspec_bool(cs_show_footer_id_stack, w->cspec_bool, &footer_show_id_stack);
        cspec_bool(cs_show_footer_font_scale, w->cspec_bool, &footer_show_font_scale);
        cspec_bool(cs_show_footer_style, w->cspec_bool, &footer_show_style);

        ImGui::BeginGroup();
        if (footer_show_db) {
            // Push colour styling for the DB button
            ImGui::PushStyleColor(ImGuiCol_Button, (ImU32)db_status_color);
            if (ImGui::Button("DB")) {
                action_dispatch(ninx_FooterDBButton, einx_Click);
            }
            ImGui::PopStyleColor(1);
        }
        if (footer_show_fps) {
            ImGui::SameLine();
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        }
        if (footer_show_demo) {
            ImGui::SameLine();
            ImGui::Checkbox("Demo", &show_demo);
            if (show_demo) {
                ImGui::ShowDemoWindow();
                ImPlot::ShowDemoWindow();
            }
        }
        if (footer_show_id_stack) {
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
        // const static char* method = "NDContext::render_date_picker: ";

        // don't bother rendering if not visible
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        if (window == nullptr || window->SkipItems)
            return;

        dp_vars.combo_flags = ImGuiComboFlags_HeightRegular;
        cspec_int(cs_combo_flags, w->cspec_int, &dp_vars.combo_flags);

        // now get hold of the YMD int[3]
        DataRef* ymd_data_ref = cspec_data_ref(cs_cname, w->data_refs);
        assert(ymd_data_ref != nullptr);
        assert(ymd_data_ref->tipe == cdIntVec);
        IntInx iinx{ ymd_data_ref->ref_inx };
        int* int_ptr = data_lay_cache.get_int_value(iinx);
        assert(int_ptr != nullptr);

        // get hold of the the original cname char* in case we
        // don't have label in the cspec
        const char* cname = data_lay_cache.get_addr_value(ymd_data_ref->addr_inx);
        const char* _label = cspec_string(cs_label, w->cspec_str, cname);
        const char* label = _label == nullptr ? cname : _label;
        // label will be eg "Start date" next to the combo
        // so we need a hidden label for the Combo
        compound_string(string_buffer, STR_BUF_LEN, Static::double_hash_cs, label);

        // convert YMD int[3] to string for combo...
        std::copy_n(int_ptr, dp_vars.old_date.size(), dp_vars.old_date.begin());
        fmt_result = fmt::format_to_n(date_buffer, STR_BUF_LEN, 
            "{:04d}-{:02d}-{:02d}", dp_vars.old_date[0], 
                dp_vars.old_date[1], dp_vars.old_date[2]);
        date_buffer[fmt_result.size] = 0;
        dp_vars.new_date = dp_vars.old_date;

        // Main drop down combo for the date
        if (ImGui::BeginCombo(string_buffer, date_buffer, dp_vars.combo_flags)) {
            {   // local scope so year_month_font is popped before the day date table
                LocalFont year_month_font(w, cs_year_month_font, cs_year_month_font_size);
                // compose hidden label for the month combo box
                compound_string(string_buffer, STR_BUF_LEN, Static::month_combo_cs, label);
                dp_vars.month_index = int_ptr[Month] - 1;
                if (ImGui::Combo(string_buffer, &dp_vars.month_index, Static::months_array_cs.data(), 12)) {
                    dp_vars.new_date[Month] = 1 + dp_vars.month_index;         // jan index 0, month 1
                    int_ptr[Month] = 1 + dp_vars.month_index;
                }
                // compose hidden label for the year input int
                compound_string(string_buffer, STR_BUF_LEN, Static::year_input_int_cs, label);
                if (ImGui::InputInt(string_buffer, &int_ptr[Year])) {
                    dp_vars.new_date[Year] = int_ptr[Year];
                }
            }

            dp_vars.content_width = ImGui::GetContentRegionAvail().x;
            dp_vars.arrow_size = ImGui::GetFrameHeight();
            dp_vars.arrow_button_width = dp_vars.arrow_size * 2.0f + ImGui::GetStyle().ItemSpacing.x;
            dp_vars.bullet_size = dp_vars.arrow_size - 5.0f;
            dp_vars.bullet_button_width = dp_vars.bullet_size + ImGui::GetStyle().ItemSpacing.x;
            dp_vars.combined_width = dp_vars.arrow_button_width + dp_vars.bullet_button_width;
            dp_vars.offset = (dp_vars.content_width - dp_vars.combined_width) * 0.5f;

            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + dp_vars.offset);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, vec4z);
            ImGui::PushStyleColor(ImGuiCol_Border, vec4z);
            
            // Left arrow for previous month; compose hidden label
            compound_string(string_buffer, STR_BUF_LEN, Static::prev_month_cs, label);
            vec2[1] = vec2[0] = dp_vars.arrow_size;
            if (ImGui::ArrowButtonEx(string_buffer, ImGuiDir_Left, vec2)) {
                DecrementMonth(dp_vars.new_date);
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

            // Centre button
            compound_string(string_buffer, STR_BUF_LEN, Static::mid_button_cs, label);
            vec2[1] = vec2[0] = dp_vars.arrow_size;
            if (ImGui::ButtonEx(string_buffer, vec2)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, vec4z);
            ImGui::PushStyleColor(ImGuiCol_Border, vec4z);

            // Right arrow for next month
            compound_string(string_buffer, STR_BUF_LEN, Static::next_month_cs, label);
            vec2[1] = vec2[0] = dp_vars.arrow_size;
            if (ImGui::ArrowButtonEx(string_buffer, ImGuiDir_Right, vec2)) {
                IncrementMonth(dp_vars.new_date);
            }
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar();

            // Now for the day date table; a table full of DD
            // with 7 cols for Mo, Tu, We, Th, Fr, Sa, Su and 
            // several rows for the dates
            // eg  1  2  3  4  5  6  7
            //     8  9 10 11 12 13 14
            //    15 16 17 18 19 20 21
            int table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedSame;
            cspec_int(cs_table_flags, w->cspec_int, &table_flags);
            compound_string(string_buffer, STR_BUF_LEN, Static::date_table_cs, label);
            if (ImGui::BeginTable(string_buffer, 7, table_flags)) {
                LocalFont day_date_font(w, cs_day_date_font, cs_day_date_font_size);
                for (const auto& day : Static::day_array_cs) {
                    ImGui::TableSetupColumn(day);
                }
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::GetStyleColorVec4(ImGuiCol_TableHeaderBg));
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor(2);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                int first_day_of_month = WeekDay(1, int_ptr[Month], int_ptr[Year]);
                int month_day_count = MonthDayCount(int_ptr[Month], int_ptr[Year]);
                int month_week_count = MonthWeekCount(int_ptr[Month], int_ptr[Year]);

                for (int i = 1; i <= month_week_count; ++i) {
                    WeekDates(i, first_day_of_month, month_day_count, dp_vars.day_array);
                    for (auto day : dp_vars.day_array) {
                        if (day != 0) {
                            const bool selected = day == int_ptr[Day];
                            if (!selected) {
                                ImGui::PushStyleColor(ImGuiCol_Button, vec4z);
                                ImGui::PushStyleColor(ImGuiCol_Border, vec4z);
                            }
                            if (ImGui::Button(Static::integers_cs[day], vec2z)) {
                                // NB ImGui::Button doesn't operate directly
                                // on int_ptr data, unlike Combo and InputInt,
                                // so we set explicitly here.
                                int_ptr[Day] = day;
                                dp_vars.new_date[Day] = day;
                                ImGui::CloseCurrentPopup();
                            }
                            if (!selected)
                                ImGui::PopStyleColor(2);
                        }
                        if (day != month_day_count)
                            ImGui::TableNextColumn();
                    }
                }
                ImGui::EndTable();
            }
            ImGui::EndCombo();
        }
        if (dp_vars.new_date != dp_vars.old_date) {
            notify_server(ymd_data_ref, dp_vars.old_date, dp_vars.new_date);
        }
    }


    void render_text(WidgetPtr w) {
        const static char* method = "NDContext::render_text: ";

        const char* rtext = cspec_string(cs_text, w->cspec_str, method);
        ImGui::TextUnformatted(rtext);
    }

    void render_button(WidgetPtr w) {
        const static char* method = "NDContext::render_button: ";

        const char* button_text = cspec_string(cs_text, w->cspec_str, method);

        if (ImGui::Button(button_text)) {
            action_dispatch(w->widget_inx, einx_Click);
        }
    }

    
    void render_duck_table_summary_modal(WidgetPtr w) {
        const static char* method = "NDContext::render_duck_table_summary_modal: ";

        // Window, table, col layout disussion...
        // https://github.com/ocornut/imgui/issues/5478
        // ...uses these: ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter
        // removed from prev impl: ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;
        static int default_summary_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
        static int default_window_flags = ImGuiWindowFlags_AlwaysAutoResize;

        const char* title = cspec_string(cs_title, w->cspec_str, method);
        int table_flags = default_summary_table_flags;
        int window_flags = default_window_flags;
        cspec_int(cs_table_flags, w->cspec_int, &table_flags);
        cspec_int(cs_window_flags, w->cspec_int, &window_flags);

        DataRef* result_set_data_ref = cspec_data_ref(cs_query_id, w->data_refs);
        assert(result_set_data_ref != nullptr);
        const char* query_id = data_lay_cache.get_string_value(result_set_data_ref->addr_inx);

        if (title) {    // scope to fire title_font dtor and pop before body
            LocalFont title_font(w, cs_title_font, cs_title_font_size);
            ImGui::OpenPopup(title);
            // Always center this window when appearing
            ImGuiViewport* vp = ImGui::GetMainViewport();
            assert(vp != nullptr);
            auto center = vp->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, { 0.5, 0.5 });
        }
        else {
            NDLogger::cerr() << method << "NO_TITLE" << std::endl;
            return;
        }

        std::uint32_t colm_count = Static::SMRY_COLM_CNT;
        std::uint32_t colm_index = 0;
        if (ImGui::BeginPopupModal(title, nullptr, window_flags)) {
            LocalFont body_font(w, cs_body_font, cs_body_font_size);
            std::uint64_t result_handle = proxy.get_handle(query_id);
            if (!result_handle) {
                auto [iter, inserted] = bad_handle_map.insert(std::make_pair(query_id, 1));
                if (!inserted) iter->second++;
                return;
            }
            if (ImGui::BeginTable(query_id, (int)colm_count, table_flags)) {
                for (colm_index = 0; colm_index < colm_count; colm_index++) {
                    ImGui::TableSetupColumn(Static::duck_table_summary_colm_names[colm_index]);
                }
                ImGui::TableHeadersRow();
                std::uint32_t row_count = proxy.get_row_count(result_handle);
                proxy.get_meta_data(result_handle, colm_count, row_count);
                for (uint32_t row_index = 0; row_index < row_count; row_index++) {
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
        {   // Local scope so we're poppped before ImGui::EndPopup()
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
        }
        ImGui::EndPopup();
    }

    void render_loading_modal(WidgetPtr w) {
        const static char* method = "NDContext::render_loading_modal: ";

        static ImVec2 position = { 0.5, 0.5 };
        const char* title = cspec_string(cs_title, w->cspec_str, method);
        LocalFont title_font(w, cs_title_font, cs_title_font_size);
        ImGui::OpenPopup(title);
        // Always center this window when appearing
        ImGuiViewport* vp = ImGui::GetMainViewport();
        assert(vp != nullptr);
        auto center = vp->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, position);
        // Get the text list
        DataRef* str_vec_data_ref = cspec_data_ref(cs_cname, w->data_refs);
        assert(str_vec_data_ref != nullptr);
        assert(str_vec_data_ref->tipe == cdStrVec);

        if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            {   // local scope for LocalFont so it pops before ImGui::EndPopup()
                LocalFont body_font(w, cs_body_font, cs_body_font_size);
                StrInx sinx{ str_vec_data_ref->ref_inx };
                for (uint32_t i = 0; i < str_vec_data_ref->size; i++) {
                    const char* text = data_lay_cache.get_string_value(sinx);
                    if (text) {
                        ImGui::TextUnformatted(text);
                    }
                    sinx++;
                }
                cspec_int(cs_spinner_radius, w->cspec_int, &sp_vars.radius);
                cspec_int(cs_spinner_thickness, w->cspec_int, &sp_vars.thickness);
                if (!Spinner(Static::i_am_loading_spinner_cs, sp_vars)) {
                    // TODO: spinner always fails IsClippedEx on first render
                    NDLogger::cout() << method << "SPINNER_FAIL" << std::endl;
                }
            }
            ImGui::EndPopup();
        }
    }

    void render_window(WidgetPtr w) {
        const static char* method = "NDContext::render_window: ";
        static int default_window_flags = ImGuiWindowFlags_AlwaysAutoResize;

        LocalFont title_font(w, cs_title_font, cs_title_font_size);
        const char* title = cspec_string(cs_title, w->cspec_str, method);
        int window_flags = default_window_flags;
        cspec_int(cs_window_flags, w->cspec_int, &window_flags);

        if (ImGui::Begin(title, nullptr, window_flags)) {
            for (int inx = 0; inx < w->children.size(); inx++) {
                dispatch_render(w->children[inx]);
            }
        }
        // https://github.com/ocornut/imgui/issues/6683
        // We always need a matching End() even if Begin() rets false
        ImGui::End();
    }

    void render_shaded_plot(WidgetPtr w) {
        const static char* method = "NDContext::render_shaded_plot: ";
        static ImPlotShadedFlags default_shaded_plot_flags{ 0 };

        const char* title = cspec_string(cs_title, w->cspec_str, method);
        int shaded_plot_flags = default_shaded_plot_flags;
        // See enum ImPlotShadedFlags: there is only default
        // enable cspec setting of shaded when there is a real choice
        // cspec_int(cs_shaded_plot_flags, w->cspec_int, &shaded_plot_flags);

        cspec_bool(cs_show_lines, w->cspec_bool, &sh_pl_vars.show_lines);
        cspec_bool(cs_show_fills, w->cspec_bool, &sh_pl_vars.show_fills);

        DataRef* result_set_data_ref = cspec_data_ref(cs_query_id, w->data_refs);
        assert(result_set_data_ref != nullptr);
        const char* query_id = data_lay_cache.get_string_value(result_set_data_ref->addr_inx);
        RSHandle handle = proxy.get_handle(query_id);

        DataRef* x_data_ref = cspec_data_ref(cs_xname, w->data_refs);
        DataRef* y_data_ref = cspec_data_ref(cs_yname, w->data_refs);
        assert(x_data_ref != nullptr);
        assert(y_data_ref != nullptr);

        StrInx xinx{ x_data_ref->ref_inx };
        const char* x_col_name = data_lay_cache.get_string_value(xinx);

        // TODO: extend for multiple y plots
        StrInx yinx{ y_data_ref->ref_inx };
        const char* y_col_name = data_lay_cache.get_string_value(yinx);

        // get ranges from bulk cache
        proxy.get_min_max(handle, x_col_name, sh_pl_vars.xmin_dbl, sh_pl_vars.xmax_dbl);
        proxy.get_min_max(handle, y_col_name, sh_pl_vars.ymin_dbl, sh_pl_vars.ymax_dbl);
        // TODO: connect row_count & offset to slider widgets
        sh_pl_vars.row_count = proxy.get_row_count(handle);
        sh_pl_vars.offset = 0;

        DB::XYRange* range{ 0 };
        if (ImPlot::BeginPlot(title)) {
            ImPlot::SetupAxes(x_col_name, y_col_name);
            ImPlot::SetupAxesLimits(sh_pl_vars.xmin_dbl, sh_pl_vars.xmax_dbl,
                                        sh_pl_vars.ymin_dbl, sh_pl_vars.ymax_dbl);
            if (sh_pl_vars.show_fills) {
                sh_pl_vars.spec.Flags = shaded_plot_flags;
                sh_pl_vars.spec.FillAlpha = 0.25f;
                range = proxy.init_xy_range(handle, x_col_name, y_col_name, sh_pl_vars.offset, sh_pl_vars.row_count);
                range = proxy.next_xy_range(range);
                while (range != nullptr) {
                    ImPlot::PlotShaded(title, range->xdata, range->ydata, range->plot_count, 0.0, sh_pl_vars.spec);
                    range = proxy.next_xy_range(range);
                }
            }
            // Lines on top of fills
            if (sh_pl_vars.show_lines) {
                range = proxy.init_xy_range(handle, x_col_name, y_col_name, sh_pl_vars.offset, sh_pl_vars.row_count);
                range = proxy.next_xy_range(range);
                while (range != nullptr) {
                    ImPlot::PlotLine(title, range->xdata, range->ydata, range->plot_count);
                    range = proxy.next_xy_range(range);
                }
            }
        }
    }

    void render_table(WidgetPtr w) {
        const static char* method = "NDContext::render_table: ";

        static int default_table_flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

        const char* title = cspec_string(cs_title, w->cspec_str, method);
        int table_flags = default_table_flags;
        cspec_int(cs_table_flags, w->cspec_int, &table_flags);

        DataRef* result_set_data_ref = cspec_data_ref(cs_query_id, w->data_refs);
        assert(result_set_data_ref != nullptr);
        const char* query_id = data_lay_cache.get_string_value(result_set_data_ref->addr_inx);

        // TODO: recode proxy.get_meta_data() to lazy load 
        // and report geom
        std::uint32_t colm_count = 0;
        std::uint32_t row_count = 0;
        std::uint32_t colm_index = 0;
        {
            LocalFont body_font(w, cs_body_font, cs_body_font_size);
            RSHandle result_handle = proxy.get_handle(query_id);
            if (!result_handle) {
                auto [iter, inserted] = bad_handle_map.insert(std::make_pair(query_id, 1));
                if (!inserted) iter->second++;
                return;
            }
            if (!proxy.get_meta_data(result_handle, colm_count, row_count)) {
                NDLogger::cout() << method << "GET_META_DATA_FAIL for QID: " << query_id << std::endl;
                return;
            }
            StringVec& colm_names = proxy.get_col_names(result_handle);
            if (ImGui::BeginTable(title, (int)colm_count, table_flags)) {
                ImGui::TableSetupScrollFreeze(1, 1);
                for (colm_index = 0; colm_index < colm_count; colm_index++) {
                    ImGui::TableSetupColumn(colm_names[colm_index].c_str(), ImGuiTableColumnFlags_None);
                }
                ImGui::TableHeadersRow();
                ImGuiListClipper clipper;
                clipper.Begin((int)row_count, -1.0f);
                while (clipper.Step()) {
                    for (int row_index = clipper.DisplayStart; row_index < clipper.DisplayEnd; row_index++) {
                        ImGui::TableNextRow();
                        for (colm_index = 0; colm_index < colm_count; colm_index++) {
                            if (ImGui::TableSetColumnIndex(colm_index)) {
                                const char* endchar = proxy.get_datum(result_handle, colm_index, row_index);
                                if (endchar) {
                                    ImGui::TextUnformatted(proxy.buffer, endchar);
                                }
                                else {
                                    ImGui::TextUnformatted(proxy.buffer);
                                }
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }
        }
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
        // const static char* method = "NDContext::push_font: ";

        // get size if specified, otherwise default to 0
        int font_size_base = 0;
        cspec_int(cs_font_size, w->cspec_int, &font_size_base);

        // defaults so resolution failure doesn't stop us pushing
        ImFont* font = font_map[Static::default_cs];
        const char* font_name = cspec_string(cs_font_name, w->cspec_str, Static::default_cs);
        if (font_name != nullptr) {
            auto font_it = font_map.find(font_name);
            if (font_it != font_map.end()) font = font_it->second;
        }

        ImGui::PushFont(font, (float)font_size_base);
        font_push_count++;
        return true;
    }

    void pop_font() {
        ImGui::PopFont();
        font_pop_count++;
    }
};


