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
bool JContains(const JSON& obj, const char* key) {
	return false;
}

template <typename JSON, typename K>
std::string JAsString(const JSON& obj, K key) {
	return "";
}

template <typename JSON>
float JAsFloat(const JSON& obj, const char* key) {
	return 0.0;
}

template <typename JSON, typename K>
float JAsInt(const JSON& obj, K key) {
	return 0;
}

template <typename JSON>
bool JAsBool(const JSON& obj, const char* key) {
	return false;
}

template <typename JSON>
void JAsStringVec(const JSON& obj, const char* key, std::vector<std::string>& vec) {
}

template <typename JSON>
int JSize(const JSON& obj) {
	return 0;
}

template <typename JSON, typename V>
void JSet(JSON& obj, const char* key, const V& val) {
	obj[key] = val;
}

template <typename JSON, typename V>
JSON JArray(const std::vector<V>& values) {
	return JSON(values);
}

#ifndef __EMSCRIPTEN__
// nlohmann::json implementations of JSON cache ops
template <>
nlohmann::json JParse(const char* json_string) {
	return nlohmann::json::parse(json_string);
}

template <>
bool JContains(const nlohmann::json& obj, const char* json_string) {
	return obj.contains(json_string);
}

template <typename K>
std::string JAsString(const nlohmann::json& obj, K key) {
	return obj[key].template get<std::string>();
}

template <>
float JAsFloat(const nlohmann::json& obj, const char* key) {
	return obj[key].template get<float>();
}

template <typename K>
float JAsInt(const nlohmann::json& obj, K key) {
	return obj[key].template get<int>();
}

template <typename JSON>
bool JAsBool(const nlohmann::json& obj, const char* key) {
	return obj[key].template get<bool>();
}

template <>
void JAsStringVec(const nlohmann::json& obj, const char* key, std::vector<std::string>& vec) {
	vec = obj[key];
}

template <>
int JSize(const nlohmann::json& obj) {
	return obj.size();
}

template <typename V>
void JSet(nlohmann::json& obj, const char* key, const V& val) {
	obj[key] = val;
}

template <typename V>
nlohmann::json JArray(const std::vector<V>& values) {
	return nlohmann::json(values);
}

#else

bool JContains(const emscripten::val& obj, const char* json_string) {
	return obj.hasOwnProperty(json_string);
}

template <typename K>
std::string JAsString(const emscripten::val& obj, K key) {
	return obj[key].template as<std::string>();
}

template <>
float JAsFloat(const emscripten::val& obj, const char* key) {
	return obj[key].as<float>();
}

template <typename K>
float JAsInt(const emscripten::val& obj, K key) {
	return obj[key].template as<int>();
}

template <typename JSON>
bool JAsBool(const emscripten::val& obj, const char* key) {
	return obj[key].template as<bool>();
}

template <>
void JAsStringVec(const emscripten::val& obj, const char* key, std::vector<std::string>& vec) {
	vec = emscripten::vecFromJSArray<std::string>(obj[key]);
}

template <>
int JSize(const emscripten::val& obj) {
	return obj["length"].as<int>();
}

template <typename V>
void JSet(emscripten::val& obj, const char* key, const V& val) {
	obj.set(key, val);
}


template <typename V>
emscripten::val JArray(const std::vector<V>& values) {
	return emscripten::val::array(values);
}

// emscripten::val implementations of JSON cache ops
#endif

