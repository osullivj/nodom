# nodom
NoDOM High Performance Web GUI

- Implemented in Data Oriented Modern C++
- Builds on highly performant GameDev tech
- Compiles to WASM with Emscripten, and Win32 x64 with MSVC 2022
- Integrates with DuckDB WASM for bulk data
- Declarative GUI specified in JSON served to WASM client by websocket
- - GUI specification JSON designed for AI generation
- WebGL based GUI uses GPU rendering
- No emscripten::val roundtrips on the render hotpath
- Designed for the trading floor: performance over presentation

# Standing on the shoulders of giants...
NoDOM builds on top of some amazing OSS code...

- [Dr ImGui](https://github.com/ocornut/imgui)
- [ImPlot](https://github.com/epezent/implot)
- [Emscripten](https://emscripten.org/)
- [DuckDB-WASM](https://duckdb.org/docs/lts/clients/wasm/overview)
