pushd %ND_HOME%
:: windows: where is your py3 install?
%ND_PY_HOME%\python.exe -m venv venv
:: Update pip to latest before getting pkgs; especially
::   important for pkgs with C/C++ extensions
::   NB using python3 in venv, not systemwide!
venv\scripts\python -m pip install pip --upgrade pip
venv\scripts\python -m pip install -r requirements.txt
