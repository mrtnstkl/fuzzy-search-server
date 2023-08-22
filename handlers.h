#pragma once

#include "httplib.h"

#include "fuzzy.hpp"
#include "util.h"

httplib::Server::Handler fuzzy_handler(fuzzy::sorted_database<std::string> &database);
