@echo off
echo "Downloading ESM modules for %ND_DUCK_ESM%"
%ND_HOME%\venv\scripts\download-esm %ND_DUCK_ESM% %ND_HOME%\bld
