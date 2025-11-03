#include <filesystem>
#include <iostream>
#include <fstream>
#include "proxy.hpp"
#include "static_strings.hpp"

NDProxy::NDProxy(int argc, char** argv)
    :is_db_app(false), done(false)

}

NDProxy::~NDProxy() {

}

