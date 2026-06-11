#pragma once

#include "httplib.h"
#include "fss.h"


httplib::Server::Handler info_handler(fuzzy_search_server &fss);
httplib::Server::Handler fuzzy_handler(fuzzy_search_server &fss);
httplib::Server::Handler fuzzy_list_handler(fuzzy_search_server &fss);
httplib::Server::Handler fuzzycomplete_handler(fuzzy_search_server &fss);
httplib::Server::Handler fuzzycomplete_list_handler(fuzzy_search_server &fss);
httplib::Server::Handler exact_handler(fuzzy_search_server &fss);
httplib::Server::Handler exact_list_handler(fuzzy_search_server &fss);
httplib::Server::Handler completion_handler(fuzzy_search_server &fss);
httplib::Server::Handler completion_list_handler(fuzzy_search_server &fss);
