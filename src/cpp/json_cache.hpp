#pragma once
#ifndef __EMSCRIPTEN__
#include "json.hpp"
#else
#endif

// obviously wrong non specialised func
template <typename JSON>
JSON JParse(const char* json_string) {
	return JSON{};
}

template <typename JSON>
bool JContains(JSON& obj, const char* key) {
	return false;
}

template <typename JSON>
std::string JAsString(JSON& obj, const char* key) {
	return "";
}

#ifndef __EMSCRIPTEN__
// nlohmann::json implementations of JSON cache ops
template <>
nlohmann::json JParse(const char* json_string) {
	return nlohmann::json::parse(json_string);
}

template <>
bool JContains(nlohmann::json& obj, const char* json_string) {
	return obj.contains(json_string);
}

template <>
std::string JAsString(nlohmann::json& obj, const char* key) {
	return obj[key].template get<std::string>();
}
#else
bool JContains(emscripten::val& obj, const char* json_string) {
	return obj.hasOwnProperty(json_string);
}
template <>
std::string JAsString(emscripten::val& obj, const char* key) {
	return obj[key].as<std::string>();
}

// emscripten::val implementations of JSON cache ops
#endif

