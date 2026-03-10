#pragma once

// Python consts
// TODO: move into statics.hpp which should be
// included in main<>.cpp so expanded template
// NDContext can use
struct Static {
	// const string use by NDContext for JSON data and layout
	// Render method names
	inline static const char* rm_home_cs{ "Home" };		// Home
	inline static const char* rm_input_int_cs{ "InputInt" };	// Native
	inline static const char* rm_combo_cs{ "Combo" };
	inline static const char* rm_checkbox_cs{ "Checkbox" };
	inline static const char* rm_text_cs{ "Text" };
	inline static const char* rm_button_cs{ "Button" };
	inline static const char* rm_table_cs{ "Table" };
	inline static const char* rm_footer_cs{ "Footer" };		// Compount
	inline static const char* rm_date_picker_cs{ "DatePicker" };
	inline static const char* rm_duck_table_summary_modal_cs{ "DuckTableSummaryModal" };
	inline static const char* rm_loading_modal_cs{ "LoadingModal" };
	inline static const char* rm_separator_cs{ "Separator" };	// Layout
	inline static const char* rm_same_line_cs{ "SameLine" };
	inline static const char* rm_new_line_cs{ "NewLine" };
	inline static const char* rm_spacing_cs{ "Spacing" };
	inline static const char* rm_align_text_to_frame_padding_cs{ "AlignTextToFramePadding" };
	inline static const char* rm_begin_child_cs{ "BeginChild" };	// Grouping
	inline static const char* rm_end_child_cs{ "EndChild" };
	inline static const char* rm_begin_group_cs{ "BeginGroup" };
	inline static const char* rm_end_group_cs{ "EndGroup" };
	inline static const char* rm_push_font_cs{ "PushFont" };	// Fonts
	inline static const char* rm_pop_font_cs{ "PopFont" };

	inline static const char* is_duck_app_cs{"is_duck_app"};
	

	inline static const char* __nodom__cs{ "__nodom__" };


	

	inline static const char* sys_cs{ "sys" };

	// cspec keys

	inline static const char* spinner_thickness_cs{ "spinner_thickness" };
	inline static const char* sql_cs{"sql"};
	inline static const char* db_cs{ "db" };
	inline static const char* fps_cs{ "fps" };
	inline static const char* demo_cs{ "demo" };
	inline static const char* id_stack_cs{ "id_stack" };
	inline static const char* font_scale_cs{ "font_scale" };
	inline static const char* style_cs{ "style" };
	inline static const char* lb_footer_style_cs{ "Style" };
	inline static const char* cspec_cs{"cspec"};
	inline static const char* cname_cs{"cname"};
	inline static const char* qname_cs{"qname"};
	inline static const char* rname_cs{"rname"};
	inline static const char* title_cs{"title"};
	inline static const char* text_cs{"text"};
	inline static const char* index_cs{"index"};
	inline static const char* step_cs{"step"};
	inline static const char* step_fast_cs{"step_fast"};
	inline static const char* flags_cs{"flags"};
	inline static const char* table_flags_cs{"table_flags"};
	inline static const char* combo_flags_cs{"combo_flags"};
	inline static const char* window_flags_cs{"window_flags"};
	inline static const char* child_flags_cs{"child_flags"};
	inline static const char* path_cs{"path"};
	inline static const char* service_cs{"service"};
	inline static const char* children_cs{"children"};
	inline static const char* breadboard_cs{"breadboard"};
	inline static const char* duck_module_cs{"duck_module"};
	inline static const char* empty_cs{""};
	inline static const char* nodom_cs{"NoDOM"};
	inline static const char* font_cs{"font"};
	inline static const char* font_size_cs{"font_size"};
	inline static const char* title_font_cs{"title_font"};
	inline static const char* title_font_size_cs{"title_font_size"};
	inline static const char* body_font_cs{"body_font"};
	inline static const char* body_font_size_cs{"body_font_size"};
	inline static const char* button_font_cs{"button_font"};
	inline static const char* button_font_size_cs{"button_font_size"};
	inline static const char* year_month_font_cs{"year_month_font"};
	inline static const char* year_month_font_size_cs{"year_month_font_size"};
	inline static const char* day_date_font_cs{"day_date_font"};
	inline static const char* day_date_font_size_cs{"day_date_font_size"};
	inline static const char* spinner_radius_cs{"spinner_radius"};
	inline static const char* size_cs{"size"};
	inline static const char* widget_id_cs{"widget_id"};
	
	inline static const char* layout_cs{"layout"};
	inline static const char* data_cs{"data"};
	inline static const char* value_cs{"value"};
	inline static const char* error_cs{"Error"};
	inline static const char* default_cs{"Default"};
	inline static const char* ok_cs{"OK"};
	inline static const char* cancel_cs{"Cancel"};

	// compound widget IDs: we alreay use _id as a var name suffix eg widget_id
	// and query_id, so we signal the IDness of these strings with i_am_ prefix
	inline static const char* i_am_footer_db_button_cs{ "i_am_footer_db_button" };
	inline static const char* i_am_footer_style_combo_cs{ "i_am_footer_style_combo" };
	inline static const char* i_am_loading_spinner_cs{ "i_am_loading_spinner" };
	// inline static const char* i_am_duck_table_summary_modal_cs{ "i_am_duck_table_summary_modal" };
	// inline static const char* i_am_loading_modal_cs{ "i_am_loading_modal" };

	// DB
	inline static const char* null_cs{"NULL"};
	inline static const char* bad_cs{"BAD"};

	// Actions: data.actions should be a dict
	// of dicts. Those leaf dicts should supply
	// a list of strings as the nd_events value
	// specifying which events can trigger the action.
	inline static const char* actions_cs{"actions"};
	inline static const char* nd_events_cs{"nd_events"};
	// An action can push or pop a UI element
	inline static const char* ui_pop_cs{"ui_pop"};
	inline static const char* ui_push_cs{"ui_push"};
	// An action dict may have a db subdict
	// specifying a DB action
	inline static const char* db_action_cs{"db_action"};
	// DB subdict must supply an action (Command|Query|BatchRequest),
	// a unique query ID and a cache address for the SQL in sql_cname
	inline static const char* action_cs{"action"};
	inline static const char* sql_cname_cs{"sql_cname"};

	// Events: message keys
	inline static const char* nd_type_cs{ "nd_type" };
	inline static const char* new_value_cs{ "new_value" };
	inline static const char* old_value_cs{ "old_value" };
	inline static const char* cache_key_cs{ "cache_key" };
	inline static const char* on_data_change_cs{ "on_data_change" };
	inline static const char* query_id_cs{ "query_id" };

	// Events: possible values for nd_type
	inline static const char* batch_request_cs{"BatchRequest"};
	inline static const char* batch_response_cs{"BatchResponse"};
	inline static const char* cache_response_cs{"CacheResponse"};
	inline static const char* cache_request_cs{"CacheRequest"};
	inline static const char* click_cs{"Click"};
	inline static const char* data_change_cs{"DataChange"};
	inline static const char* data_change_confirmed_cs{"DataChangeConfirmed"};
	inline static const char* duck_instance_cs{"DuckInstance"};
	inline static const char* query_cs{"Query"};
	inline static const char* query_result_cs{"QueryResult"};
	inline static const char* command_cs{"Command"};
	inline static const char* command_result_cs{"CommandResult"};
	inline static const char* gui_cs{ "GUI" };
	inline static const char* websock_cs{ "WebSock" };

	// Fake QIDs
	inline static const char* gui_online_cs{ "gui_online" };	// GUI.gui_online
	inline static const char* db_online_cs{ "db_online" };		// DuckInstance.db_online
	inline static const char* cache_loaded_cs{ "cache_loaded" };// GUI.cache_loaded

	// Critical strings
	inline static const char* websock_connection_failed_cs{ "websock_connection_failed" };	// WebSock.connection_failed
	inline static const char* clear_cs{ "clear" };	// WebSock.clear

	// Misc
	inline static const char* space_cs{ " " };
	inline static const char* indent_cs{"  "};
	inline static const char* chunk_cs{"chunk"};
	inline static const char* period_cs{"."};
	inline static const char* colon_cs{ ":" };
	inline static const char* db_config_cs{ "db_config" };

	// Error codes for logging fingerprints
	inline static const char* BAD_CSPEC_cs{ "BAD_CSPEC" };

	// fast path cache variables
	inline static const char underscore_c{ '_' };
	inline static const char* _font_scale_dpi_cs{ "_font_scale_dpi" };
	inline static const char* _font_scale_main_cs{ "_font_scale_main" };
	inline static const char* _style_coloring{ "_style_coloring" };
	inline static const char* _server_url{ "_server_url"};

	// _style_coloring values
	inline static const char* dark_cs{ "Dark" };
	inline static const char* light_cs{ "Light" };
	inline static const char* classic_cs{ "Classic" };

	// hardwired init data and layout
	inline static const char* init_data_cs{ ""};
	inline static const char* init_layout_cs{ "{rname: 'Home', cspec:{ title: 'NoDOM HW', children: [] }}" };

	// error messages
	inline static const char* could_not_connect_cs{ "Could not connect to" };
};