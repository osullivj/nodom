#pragma once
#ifndef __EMSCRIPTEN__
#include "json.hpp"
#else
#endif

// obviously wrong non specialised func
template <typename JSON>
JSON parse(const char* json_string) {
	return JSON{};
}

// Both nlohmann::json and emscripten::val support
// STL style containers, so we can use unspecialised funcs
template <typename JSON>
typename JSON::iterator begin(JSON& obj) {
	return obj.begin();
}

template <typename JSON>
typename JSON::iterator end(JSON& obj) {
	return obj.end();
}

#ifndef __EMSCRIPTEN__
// nlohmann::json implementations of JSON cache ops
template <>
nlohmann::json parse(const char* json_string) {
	return nlohmann::json::parse(json_string);
}



#else
// emscripten::val implementations of JSON cache ops
#endif

