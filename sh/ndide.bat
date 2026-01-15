@echo off
echo "'start devenv' to launch the MSVC IDE"
echo "'start zed' to launch the Zed editor"
:: IDE env vars
set PATH=C:\osullivj\src\h3gui\venv\Lib\site-packages\pyarrow;%PATH%
"%ND_MSVC_HOME%\VC\Auxiliary\Build\vcvars64.bat"
:: user: "start devenv"
:: user: "start zed"
