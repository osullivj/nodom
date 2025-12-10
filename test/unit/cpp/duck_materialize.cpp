// Test harness for materializing chunks and
// passing them to C++ WASM
#include <string>
#include <iostream>
#include <emscripten/val.h>

extern "C" {
    void on_db_result_cpp(emscripten::EM_VAL result_handle) {
        const static char* method = "on_db_result_cpp: ";
    // void on_db_result_cpp(void* result_handle) {
        emscripten::val result = emscripten::val::take_ownership(result_handle);
        std::string nd_type = result["nd_type"].template as<std::string>();
        std::string query_id = result["query_id"].template as<std::string>();
        // auto int_vec = emscripten::convertJSArrayToNumberVector<int>(result["types"]);
        // auto str_vec = emscripten::vecFromJSArray<std::string>(result["names"]);
        // auto row_count = result["rwcnt"].template as<int>();
        std::cout << method << "nd_type: " << nd_type << ", qid: " << query_id << std::endl;
    }

    int get_chunk_cpp(int size) {
        const static char* method = "get_chunk_cpp";
        // batch_materializer calls us to get a handle on WASM memory
        // https://stackoverflow.com/questions/56010390/emscripten-how-to-get-uint8-t-array-from-c-to-javascript
        // Real impl will use some kind of mem pooling
        uint8_t* buffer = new uint8_t[size];
        memset(buffer, 0, size);
        int buffer_address = reinterpret_cast<int>(buffer);
        printf("%s: size: %d, buffer_address: %d\n", method, size, buffer_address);
        return buffer_address;
    }

    void on_chunk_cpp(int* chunk) {
        const static char* method = "on_chunk_cpp";
        uint8_t* chunk_ptr = reinterpret_cast<uint8_t*>(chunk);
        int col_count = chunk_ptr[0];
        int row_count = chunk_ptr[1];
        printf("%s: col_count: %d, row_count: %d\n", method, col_count, row_count);
    }
};