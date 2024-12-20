#pragma once

#include "httplib.h"

#include "fuzzy.hpp"
#include "util.h"

template <typename T>
httplib::Server::Handler fuzzy_handler(fuzzy::sorted_database<T> &database);
template <typename T>
httplib::Server::Handler fuzzy_list_handler(fuzzy::sorted_database<T> &database);
template <typename T>
httplib::Server::Handler exact_handler(fuzzy::sorted_database<T> &database);
template <typename T>
httplib::Server::Handler exact_list_handler(fuzzy::sorted_database<T> &database);
template <typename T>
httplib::Server::Handler completion_handler(fuzzy::sorted_database<T> &database);
template <typename T>
httplib::Server::Handler completion_list_handler(fuzzy::sorted_database<T> &database);


template <typename T>
std::string process_results(const std::vector<fuzzy::result<T>>& results, bool as_list = false)
{
	std::stringstream strstream;
	if (!as_list)
	{
		assert(!results.empty());
		strstream << results[0].element->meta;
		return strstream.str();
	}

	if (results.empty())
	{
		return "[]";
	}

	strstream << "[\n";
	for (size_t i = 0; i + 1 < results.size(); i++)
	{
		strstream << "\t" << results[i].element->meta << ",\n";
	}
	strstream << "\t" << results.back().element->meta << "\n]";
	return strstream.str();
}


template <typename T>
httplib::Server::Handler fuzzy_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		auto query_result = database.exact_search(query_string, 0, 1);
		if (query_result.empty())
		{
			query_result = database.fuzzy_search(query_string);
		}
		std::cout << "fuzzy-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		if (query_result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(query_result.best(), false), "application/json");
	};
}

template <typename T>
httplib::Server::Handler fuzzy_list_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		auto query_result = database.exact_search(query_string);
		if (query_result.empty())
		{
			query_result = database.fuzzy_search(query_string);
		}
		std::cout << "fuzzy-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		res.set_content(process_results(query_result.best(), true), "application/json");
	};
}

template <typename T>
httplib::Server::Handler fuzzycomplete_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		const auto result_list = database.fuzzy_search(query_string, query_string.length()).extract(0, 1, true);
		std::cout << "fuzzycomplete-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		if (result_list.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(result_list, false), "application/json");
	};
}

template <typename T>
httplib::Server::Handler fuzzycomplete_list_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		// todo: dont hardcode max_count
		const auto result_list = database.fuzzy_search(query_string, query_string.length()).extract(0, 50, true, similarity_tolerance);
		std::cout << "fuzzycomplete-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		res.set_content(process_results(result_list, true), "application/json");
	};
}

template <typename T>
httplib::Server::Handler exact_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		auto query_result = database.exact_search(query_string, 0, 1);
		std::cout << "exact-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		if (query_result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(query_result.all(), false), "application/json");
	};
}

template <typename T>
httplib::Server::Handler exact_list_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		auto query_result = database.exact_search(query_string, std::max(0, page_number), std::max(0, page_size));
		std::cout << "exact-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		res.set_content(process_results(query_result.all(), true), "application/json");
	};
}

template <typename T>
httplib::Server::Handler completion_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		auto query_result = database.completion_search(query_string, std::max(0, page_number), std::max(0, page_size));
		std::cout << "completion-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		if (query_result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(query_result.all(), false), "application/json");
	};
}

template <typename T>
httplib::Server::Handler completion_list_handler(fuzzy::sorted_database<T> &database)
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
		timer query_timer;
		auto query_result = database.completion_search(query_string, std::max(0, page_number), std::max(0, page_size));
		std::cout << "completion-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;
		res.set_content(process_results(query_result.all(), true), "application/json");
	};
}
