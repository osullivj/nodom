// Test harness for materializing chunks and
// passing them to C++ WASM
#include <emscripten/val.h>

extern "C" {
    void on_db_result_cpp(emscripten::EM_VAL result_handle) {
        emscripten::val result = emscripten::val::take_ownership(result_handle);

    }
};