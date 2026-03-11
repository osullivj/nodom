#pragma once
#include <iostream>
#ifndef __EMSCRIPTEN__
#include "nlohmann.hpp"
#else
#endif
#include "nd_types.hpp"
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
void JAsStringVec(const JSON& obj, const char* key, StringVec& vec);

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

template <typename JSON>
std::string JPrettyPrint(const JSON& cache_object);

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
	return obj[key].template get<int>();
}

template <>
bool JAsBool(const nlohmann::json& obj, const char* key) {
	return obj[key].template get<bool>();
}

template <>
void JAsStringVec(const nlohmann::json& obj, const char* key, StringVec& vec) {
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

nlohmann::json JNewObject() { return nlohmann::json::object(); }

template <>
std::string JPrettyPrint(const nlohmann::json& cache_object) {
	std::string pp = cache_object.dump(2);
	return pp;
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
int JAsInt(const emscripten::val& obj, K key) {
	return obj[key].template as<int>();
}

template <>
bool JAsBool(const emscripten::val& obj, const char* key) {
	return obj[key].template as<bool>();
}

template <>
void JAsStringVec(const emscripten::val& obj, const char* key, StringVec& vec) {
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

// no params to drive template type deduction, so we use a 
// lambda to invoke the static object() method
emscripten::val JNewObject() { return emscripten::val::object(); }

template <>
std::string JPrettyPrint(const emscripten::val& v) {
	emscripten::val json_global = emscripten::val::global("JSON");
	emscripten::val json = json_global.call<emscripten::val>("stringify", v, emscripten::val::null(), 2);
	return json.as<std::string>();
}

// no format stream operator for ems::val. nlohmann::json provides
// operator<<, ems::val does not.
std::ostream& operator<<(std::ostream& os, const emscripten::val& v)
{
	emscripten::val json_global = emscripten::val::global("JSON");
	emscripten::val json = json_global.call<emscripten::val>("stringify", v);
	os << json.as<std::string>();
	return os;
}

#endif

// Now we have the extractors: template funcs built out
// of the operations above to aid the construction of
// NDWidget instances. These funcs can invoke other
// simple synch utils, but not anything in NDContext.
template <typename JSON>
uint32_t extract_entity_id(StringVec& id_vec, StringIntMap& id_map, const JSON& w, const char* key) {
	if (JContains(w, key)) {
		std::string id(JAsString(w, key));
		auto id_map_iter = id_map.find(id);
		if (id_map_iter != id_map.end())
			return id_map_iter->second;		// already registered
		id_vec.push_back(id);
		id_map.emplace(std::make_pair(std::move(id), id_vec.size()));
		return id_vec.size();
	}
	return 0;
}

template <typename JSON>
RenderMethod extract_render_name(const JSON& w) {
	if (JContains(w, Static::rname_cs)) {
		std::string rname(JAsString(w, Static::rname_cs));
		return RenderMethodFromString(rname.c_str());
	}
	return EndRenderMethod;
}

template <typename JSON>
const JSON& extract_cspec(const JSON& w) {
	if (JContains(w, Static::cspec_cs)) return w[Static::cspec_cs];
	// Cannot fail to return an obj, so return an empty one,
	// and we'll error later when we look for fields in cspec
	return JNewObject();
}