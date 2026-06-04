#pragma once
#include "nd_types.hpp"
#include "static_strings.hpp"
#include "config.hpp"

#ifdef __EMSCRIPTEN__
// fwd decl callback funcs used by IDBFontCache::next()
void on_async_exists_error(void* fc);
void on_async_exists(void* fc, int exists);

// fwd decl callback funcs used by IDBFontWriter::write()
void on_async_store_error(void* fw);
void on_async_store(void* fw);
// Only used in ems as a font data memo passed to
// ems IDBStore callbacks, and as a mem cache too.
using FileLoadFunc = std::function<void* (ImGuiIO& io, void* data, int sz)>;
using FileDeployFunc = std::function<void(const std::string& name, void* data)>;

using StringVec = std::vector<std::string>;

struct IDBFileCache {
    IDBFileCache(FileLoadFunc lf, FileDeployFunc df, const StringVec& files)
        :load_func(lf), deploy_func(df), file_list(files) { }
    ~IDBFileCache() { std::for_each(file_mem_vec.begin(), file_mem_vec.end(), [](auto ptr) {free(ptr); }); }
    void next() {
        if (file_list.empty())
            return;
        file_name = file_list.back();
        file_list.pop_back();
        emscripten_idb_async_exists(Static::nodom_cs, file_name.c_str(), this,
            on_async_exists, on_async_exists_error);
    }
    FileLoadFunc        load_func{ nullptr };
    FileDeployFunc      deploy_func{ nullptr };
    StringVec           file_list;
    std::string         file_name;
    std::vector<void*>  file_mem_vec;
};

using IDBFileCachePtr = std::unique_ptr<IDBFileCache>;

struct IDBFileWriter {
    void write(const char* data, int data_len) {
        emscripten_idb_async_store(Static::nodom_cs, file_name.c_str(),
            (void*)data, data_len, this, on_async_store, on_async_store_error);
    }
    std::string file_name;
};

// Callbacks for emscripten_idb_async_exists and 
// emscripten_idb_async_load. NB no \n in log
// lines as the impl below is only ever in play
// in the browser. 

// em_arg_callback_func: typedef void (*em_arg_callback_func)(void*);
inline void on_async_exists_error(void* fc) {
    const char* method = "on_async_exists_error: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);
    // NoDOM IndexedDB check fails in emscripten_idb_async_exists
    fprintf(stdout, "%sIDB_EXIST_FAIL(%s)", method, file_cache->file_name.c_str());
}

// em_arg_callback_func: typedef void (*em_arg_callback_func)(void*);
inline void on_load_error(void* fc) {
    const char* method = "on_load_error: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);
    // NoDOM IndexedDB doesn't exist
    fprintf(stdout, "%sIDB_LOAD_FAIL(%s)", method, file_cache->file_name.c_str());
}

// em_idb_onload_func: typedef void (*em_idb_onload_func)(void*, void*, int);
inline void on_load(void* fc, void* buf, int sz) {
    const char* method = "on_load: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);

    // copy the file memory as ImGui assumes it won't
    // get freed from underneath...
    if (sz < 100) {
        fprintf(stdout, "%ssz=%d", method, sz);
        return;
    }
    void* file_memory = malloc(sz);
    std::memcpy(file_memory, buf, sz);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    void* data = file_cache->load_func(io, file_memory, sz);

    // discard the .ttf suffix for NDContext registration
    auto fflen = file_cache->file_name.size();
    file_cache->file_name.erase(fflen - 4, fflen);
    file_cache->deploy_func(file_cache->file_name, data);
    file_cache->file_mem_vec.push_back(file_memory);
    printf("%sFILE_LOADED(%s)\n", method, file_cache->file_name.c_str());
    // kick off the next load, if there is one...
    file_cache->next();
}

// em_idb_exists_func: typedef void (*em_idb_exists_func)(void*, int);
inline void on_async_exists(void* fc, int exists) {
    const char* method = "on_async_exists: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);
    if (exists != 0) {
        emscripten_idb_async_load("NoDOM", file_cache->file_name.c_str(), fc, on_load, on_load_error);
    }
    else {
        fprintf(stdout, "%sIDB_ACCESS_FAIL(%s)\n", method, file_cache->file_name.c_str());
    }
}

inline void on_async_store_error(void* fw) {
    const char* method = "on_async_store_error: ";
    IDBFileWriter* file_writer = reinterpret_cast<IDBFileWriter*>(fw);
    assert(file_writer != nullptr);
    // NoDOM IndexedDB write fails in emscripten_idb_async_store
    fprintf(stdout, "%sIDB_STORE_FAIL(%s)", method, file_writer->file_name.c_str());
}

inline void on_async_store(void* fw) {
    const char* method = "on_async_store: ";
    IDBFileWriter* file_writer = reinterpret_cast<IDBFileWriter*>(fw);
    assert(file_writer != nullptr);
    // NoDOM IndexedDB write fails in emscripten_idb_async_store
    fprintf(stdout, "%sIDB_STORE_OK(%s)", method, file_writer->file_name.c_str());
}

#endif