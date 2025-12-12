// Test harness for materializing chunks and
// passing them to C++ WASM
#include <string>
#include <iostream>
#include <emscripten/val.h>

// types = 2, 10, 3, 2, 3, 3, 2, 2, 10, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 10, 10
// typ_o = Int32, Timestamp<MICROSECOND>, Float64, Int32, Float64, Float64, Int32, Int32, Timestamp<MICROSECOND>, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Int32, Timestamp<MICROSECOND>, Timestamp<MICROSECOND>
// batch_materializer JS code stores type as uint32
// ND these enums are not the same as duckdb.h!
enum DuckType : uint32_t {
    Int32 = 2,
    Timestamp_micro = 10,
    Float64 = 3
};

const char* DuckTypeToString(DuckType dt) {
    switch (dt) {
    case DuckType::Int32:
        return "Int32";
    case DuckType::Float64:
        return "Float64";
    case DuckType::Timestamp_micro:
        return "Timestamp_micro";
    }
    return "Unknown";
}

int DuckTypeToSize(DuckType dt) {
    switch (dt) {
    case DuckType::Int32:
        return 4;
    case DuckType::Float64:
        return 8;
    case DuckType::Timestamp_micro:
        return 4;
    }
    return 0;
}

void printf_comma(int i, int col_count) {
    if (i == col_count - 1) printf("\n");
    else printf(",");
}

int sprintf_value(char* cbuf, uint32_t* chunk_ptr, int bptr, uint32_t tipe, int row_index) {
    DuckType dt{ tipe };
    uint32_t* ui32data = &(chunk_ptr[bptr]);
    int32_t* i32data = nullptr;
    double* dbldata = nullptr;
    time_t* timdata = nullptr;
    tm* tmdata = nullptr;

    switch (dt) {
    case DuckType::Int32:
        i32data = reinterpret_cast<int32_t*>(ui32data);
        sprintf(cbuf, "[%d]=%d", row_index, *i32data);
        return DuckTypeToSize(dt);
    case DuckType::Float64:
        dbldata = reinterpret_cast<double*>(ui32data);
        sprintf(cbuf, "[%d]=%f", row_index, *dbldata);
        return DuckTypeToSize(dt);
    case DuckType::Timestamp_micro:
        timdata = reinterpret_cast<time_t*>(ui32data);
        tmdata = gmtime(timdata);
        sprintf(cbuf, "[%d]=", row_index);
        strftime(cbuf+strlen(cbuf), sizeof(cbuf), "%Y%m%dT%I:%M:%S.%p", tmdata);
        return DuckTypeToSize(dt);
    default:
        sprintf(cbuf, "[%d]=%s", row_index, "unk");
        return 0;
    }
}

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

    int get_chunk_cpp(int size) {
        const static char* method = "get_chunk_cpp";
        // batch_materializer calls us to get a handle on WASM memory
        // https://stackoverflow.com/questions/56010390/emscripten-how-to-get-uint8-t-array-from-c-to-javascript
        // Real impl will use some kind of mem pooling
        uint64_t* buffer = new uint64_t[size];
        memset(buffer, 0, size*8);
        int buffer_address = reinterpret_cast<int>(buffer);
        printf("%s: size: %d, buffer_address: %d\n", method, size, buffer_address);
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
        // next col_count bytes are the types
        printf("%s: types:", method);
        for (int i = 0; i < col_count; i++) {
            DuckType tipe{ chunk_ptr[bptr++] };
            const char* dt = DuckTypeToString(tipe);
            printf("%d/%s", tipe, dt);
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
        // a type(32),row_count(32) preamble.
        printf("%s: COLUMNS\n", method);
        char cbuf[128];
        for (int i = 0; i < col_count; i++) {
            // mv bptr fwd to next 8 byte boundary
            if (bptr % 2) bptr++;
            int tipe = chunk_ptr[bptr++];
            int stored_sz = chunk_ptr[bptr++];
            const std::string& name(col_names[i]);
            int calced_sz = sprintf_value(cbuf, chunk_ptr, bptr, tipe, 0);
            assert(stored_sz == calced_sz);
            printf("%s type(%d) sz(%d) rows(%d)\n", name.c_str(), tipe, stored_sz, row_count);
            int stride = stored_sz / 4;    // 1 for 4 bytes, 2 for 8 bytes
            sprintf_value(cbuf, chunk_ptr, bptr+stride, tipe, 1);
            sprintf_value(cbuf, chunk_ptr, bptr+(stride*(row_count-1)), tipe, row_count-1);
            sprintf_value(cbuf, chunk_ptr, bptr+(stride*row_count), tipe, row_count);
            bptr += stride * row_count;
        }
    }
};