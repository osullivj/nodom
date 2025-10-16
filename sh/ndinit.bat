pushd %ND_EMS_HOME%
:: updating EMS may break build
:: call emsdk install latest
call emsdk_env
popd
:: https://emscripten.org/docs/getting_started/FAQ.html
:: It can be useful to compile with EMCC_DEBUG=1 set for the environment
:: (EMCC_DEBUG=1 emcc ... on Linux, set EMCC_DEBUG=1 on Windows).
:: This splits up the compilation steps and saves them in /tmp/emscripten_temp.
:: You can then see at what stage the code vanishes (you will need to do llvm-dis
:: on the bitcode stages to read them, or llvm-nm, etc.).
:: NB 1 did not work. The %TEMP%\emscripten_temp dir was not
:: deleted, but contents were. 2 fixed that...
set EMCC_DEBUG=2
:: WASM bin tools, pyarrow venv dir to pick up arrow.dll
set PATH=%ND_DUCK_HOME%;%ND_WABT_HOME%;%ND_HOME\venv\Lib\site-packages\pyarrow;%ND_GNU_HOME%;%PATH%

