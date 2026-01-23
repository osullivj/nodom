#pragma once
#include <iostream>
#ifndef __EMSCRIPTEN__
#include "nlohmann.hpp"
#else
#endif

// non specialised func decls: no impl
// because same logic cannot work for
// both nlohmann::json and emscripten::val

template <typename JSON>
JSON JParse(const std::string& json_string);

template <typename JSON>
bool JContains(const JSON& obj, const char* key);

template <typename JSON, typename K>
std::string JAsString(const JSON& obj, K key);

template <typename JSON>
float JAsFloat(const JSON& obj, const char* key);

template <typename JSON, typename K>
float JAsInt(const JSON& obj, K key);

template <typename JSON>
bool JAsBool(const JSON& obj, const char* key);

template <typename JSON>
void JAsStringVec(const JSON& obj, const char* key, std::vector<std::string>& vec);

template <typename JSON>
int JSize(const JSON& obj);

template <typename JSON, typename V>
void JSet(JSON& obj, const char* key, const V& val);

// JArray: both nloh and ems take std::vector<V>
// ctor params for constructing lists. This
// method is slightly too generic as it
// doesn't constrain V to be atomic.
template <typename JSON, typename V>
JSON JArray(const std::vector<V>& values) {
	return JSON(values);
}

// DB handles need an array[2] on nlohmann::json
// as we can only get 32 bit int into an atomic
// For ems::val, we just have an object(arrow_table)
template <typename JSON>
JSON JAsHandle(const JSON& data, const char* key);

#ifndef __EMSCRIPTEN__
// nlohmann::json implementations of JSON cache ops
// nlohmann::json JSON cache ops only run in breadboard,
// so we can use exception handling...
template <>
nlohmann::json JParse(const std::string& json_string) {
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
int JAsInt(const nlohmann::json& obj, K key) {
	static const char* method = "JAsInt: ";
	return obj[key].template get<int>();
}

template <>
bool JAsBool(const nlohmann::json& obj, const char* key) {
	return obj[key].template get<bool>();
}

template <>
void JAsStringVec(const nlohmann::json& obj, const char* key, std::vector<std::string>& vec) {
	vec = obj[key];
}

template <>
int JSize(const nlohmann::json& obj) {
	return (int)obj.size();
}

template <typename V>
void JSet(nlohmann::json& obj, const char* key, const V& val) {
	obj[key] = val;
}

template <typename V>
nlohmann::json JArray(const std::vector<V>& values) {
	return nlohmann::json(values);
}

// TODO: rm JAsHandle
template <>
nlohmann::json JAsHandle(const nlohmann::json& data, const char* key) {
	nlohmann::json handle_array = nlohmann::json::array();
	handle_array = data[key];
	return handle_array;
}

nlohmann::json JNewObject() { return nlohmann::json::object(); }

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
int JAsInt(const emscripten::val& obj, K key) {
	return obj[key].template as<int>();
}

template <>
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

template <>
emscripten::val JParse(const std::string& json_string) {
	emscripten::val json_global = emscripten::val::global("JSON");
	emscripten::val rv = json_global.call<emscripten::val>("parse", json_string);
	return rv;
}

template <>
emscripten::val JAsHandle(const emscripten::val& data, const char* key) {
	return data[key];
}

// no params to drive template type deduction, so we use a 
// lambda to invoke the static object() method
emscripten::val JNewObject() { return emscripten::val::object(); }

std::ostream& operator<<(std::ostream& os, const emscripten::val& v)
{
	emscripten::val json_global = emscripten::val::global("JSON");
	emscripten::val json = json_global.call<emscripten::val>("stringify", v);
	os << json.as<std::string>();
	return os;
}

#endif

