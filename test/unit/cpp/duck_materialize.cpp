// Test harness for materializing chunks and
// passing them to C++ WASM. Remember, this is
// never compiled to x64 by MSVC, it is only
// ever compiled by EMS, so we're Posix not
// Win32 here.
#include <string>
#include <map>
#include <iostream>
#include <emscripten/val.h>
#include <time.h>
#include "nd_types.hpp"


extern "C" {
    void on_db_result_cpp(emscripten::EM_VAL result_handle) {
        const static char* method = "on_db_result_cpp: ";
        emscripten::val result = emscripten::val::take_ownership(result_handle);
        std::string nd_type = result["nd_type"].template as<std::string>();
        std::string query_id = result["query_id"].template as<std::string>();
        std::cout << method << "nd_type: " << nd_type << ", qid: " << query_id;
        if (nd_type == "BatchResponse") {
            int addr = result["chunk"].template as<int>();
            std::cout << ", chunk: " << addr;
        }
        std::cout << std::endl;
    }

    int get_chunk_cpp(const char* qid, int size) {
        const static char* method = "get_chunk_cpp";
        // batch_materializer calls us to get a handle on WASM memory
        // https://stackoverflow.com/questions/56010390/emscripten-how-to-get-uint8-t-array-from-c-to-javascript
        // Real impl will use some kind of mem pooling
        uint64_t* buffer = new uint64_t[size];
        memset(buffer, 0, size*8);
        int buffer_address = reinterpret_cast<int>(buffer);
        printf("%s: size: %d, buffer_address: %d, qid: %s\n", method, size, buffer_address, qid);
        return buffer_address;
    }

    void on_chunk_cpp(int* chunk) {
        const static char* method = "on_chunk_cpp";
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(chunk);
        printf("%s: METADATA\n", method);
        // First 3 words are done, col_count, row_count
        int bptr = 0;
        int done = chunk_ptr[bptr++];
        int col_count = chunk_ptr[bptr++];
        int row_count = chunk_ptr[bptr++];
        printf("%s: done: %d, col_count: %d, row_count: %d\n", method, done, col_count, row_count);
        // next col_count words are the types
        printf("%s: types:", method);
        for (int i = 0; i < col_count; i++) {
            WasmDuckType tipe{ static_cast<int32_t>(chunk_ptr[bptr++]) };
            const char* dt = WasmDuckTypeToString(tipe);
            printf("%d/%s", tipe, dt);
            printf_comma(i, col_count);
        }
        // next col_count words are the col addrs
        printf("%s: caddr:", method);
        for (int i = 0; i < col_count; i++) {
            uint32_t* col_addr = reinterpret_cast<uint32_t*>(chunk_ptr[bptr++]);
            printf("%d/%d", i, col_addr);
            printf_comma(i, col_count);
        }
        // next col_count zero terminated ASCII strings
        std::vector<std::string> col_names;
        printf("%s: names:", method);
        for (int i = 0; i < col_count; i++) {
            std::string name;
            while (chunk[bptr] != 0) {
                name.push_back(chunk[bptr]);
                bptr++;
            }
            printf("%s", name.c_str());
            printf_comma(i, col_count);
            bptr++;
            col_names.push_back(name);
        }
        // And now the columns themselves. Each one has
        // a type(32),row_count(32) preamble. We check the
        // lo bit of bptr first to see if it's "odd" ie on
        // a 4 byte boundary.
        printf("%s: COLUMNS bptr(%d)\n", method, bptr);
        char cbuf[128];
        for (int i = 0; i < col_count; i++) {
            // mv bptr fwd to next 8 byte boundary
            if (bptr & 1) {
                bptr++;
                printf("%s: incremented bptr to %d\n", method, bptr);
            }
            else {
                printf("%s:             bptr on %d\n", method, bptr);
            }
            int32_t tipe = chunk_ptr[bptr++];
            uint32_t stored_sz = chunk_ptr[bptr++];
            const std::string& name(col_names[i]);
            printf("%s type(%d) sz(%d) rows(%d)\n", name.c_str(), tipe, stored_sz, row_count);
            int stride = stored_sz / 4;    // 1 for 4 bytes, 2 for 8 bytes
            sprintf_value(cbuf, chunk_ptr, bptr, tipe, 0);
            sprintf_value(cbuf, chunk_ptr, bptr+stride, tipe, 1);
            sprintf_value(cbuf, chunk_ptr, bptr+(stride*(row_count-2)), tipe, row_count-2);
            sprintf_value(cbuf, chunk_ptr, bptr+(stride*(row_count-1)), tipe, row_count-1);
            bptr += stride * row_count;
        }
    }
};