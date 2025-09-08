#!/bin/bash
#
# Ubuntu: upgrade python3.12-venv first with...
#   sudo apt install python3.12-venv
# Create py 3 virtual env in ./venv for supporting pkgs
#   using system wide py3
python3 -m venv venv
# Update pip to latest before getting pkgs; especially
#   important for pkgs with C/C++ extensions
#   NB using python3 in venv, not systemwide!
venv/bin/python3 -m pip install pip --upgrade pip
venv/bin/python3 -m pip install -r requirements.txt
