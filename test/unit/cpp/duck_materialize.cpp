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
        std::cout << method << "nd_type: " << nd_type << ", qid: " << query_id;
        if (nd_type == "BatchResponse") {
            int addr = result["chunk"].template as<int>();
            std::cout << ", chunk: " << addr;
        }
        std::cout << std::endl;
    }

    int get_chunk_cpp(int size) {
        const static char* method = "get_chunk_cpp";
        // batch_materializer calls us to get a handle on WASM memory
        // https://stackoverflow.com/questions/56010390/emscripten-how-to-get-uint8-t-array-from-c-to-javascript
        // Real impl will use some kind of mem pooling
        uint16_t* buffer = new uint16_t[size];
        memset(buffer, 0, size*2);
        int buffer_address = reinterpret_cast<int>(buffer);
        printf("%s: size: %d, buffer_address: %d\n", method, size, buffer_address);
        return buffer_address;
    }

    void on_chunk_cpp(int* chunk) {
        const static char* method = "on_chunk_cpp";
        uint16_t* chunk_ptr = reinterpret_cast<uint16_t*>(chunk);
        // First 3 bytes are done, col_count, row_count
        int buffer_position = 0;
        int done = chunk_ptr[buffer_position++];
        int col_count = chunk_ptr[buffer_position++];
        int row_count = chunk_ptr[buffer_position++];
        printf("%s: done: %d, col_count: %d, row_count: %d\n", method, done, col_count, row_count);
        // next col_count bytes are the types
        for (int i = 0; i < col_count; i++) {
            int tipe = chunk_ptr[buffer_position++];
            printf("%s: type: %d\n", method, tipe);
        }
        // next col_count zero terminated ASCII strings
        for (int i = 0; i < col_count; i++) {
            char* name = reinterpret_cast<char*>(chunk_ptr[buffer_position]);
            printf("%s: name: %s\n", method, name);
            buffer_position += 1 + strlen(name);
        }
    }
};