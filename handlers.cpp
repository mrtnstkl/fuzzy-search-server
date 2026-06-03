#include "handlers.h"

#include "httplib.h"
#include "fss.h"

httplib::Server::Handler fuzzy_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const auto result = fss.search_fuzzy(query_string, false); 
		if (result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler fuzzy_list_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const auto result = fss.search_fuzzy(query_string, true); 
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler fuzzycomplete_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const auto result = fss.search_fuzzy_complete(query_string, false);
		if (result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler fuzzycomplete_list_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const int similarity_tolerance = req.has_param("tol") ? std::stoi(req.get_param_value("tol")) : 2;
		const auto result = fss.search_fuzzy_complete(query_string, true, similarity_tolerance);
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler exact_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const auto result = fss.search_exact(query_string, false);
		if (result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler exact_list_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const int page_number = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 0;
		const int page_size = req.has_param("count") ? std::stoi(req.get_param_value("count")) : 10;
		const auto result = fss.search_exact(query_string, true, page_number, page_size);
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler completion_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const int page_number = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 0;
		const int page_size = req.has_param("count") ? std::stoi(req.get_param_value("count")) : 10;
		const auto result = fss.search_complete(query_string, false, std::max(0, page_number), std::max(0, page_size));
		if (result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(result, "application/json");
	};
}

httplib::Server::Handler completion_list_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}
		const auto query_string = req.get_param_value("q");
		const int page_number = req.has_param("page") ? std::stoi(req.get_param_value("page")) : 0;
		const int page_size = req.has_param("count") ? std::stoi(req.get_param_value("count")) : 10;
		const auto result = fss.search_complete(query_string, true, std::max(0, page_number), std::max(0, page_size));
		res.set_content(result, "application/json");
	};
}
