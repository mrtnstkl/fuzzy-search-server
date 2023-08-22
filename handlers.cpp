#include "handlers.h"

template <typename T>
std::string process_results(std::vector<fuzzy::result<T>> results, bool as_list = false)
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

httplib::Server::Handler fuzzy_handler(fuzzy::sorted_database<std::string> &database)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}

		auto query_string = req.get_param_value("q");
		bool respond_with_list = req.has_param("list") && req.get_param_value("list") == "yes";

		timer query_timer;
		auto query_result = database.fuzzy_search(query_string);
		std::cout
			<< "fuzzy-searched " << query_string << " in " << query_timer.get() << "ms: "
			<< (query_result.empty() ? "not found" : query_result.best()[0].element->name) << std::endl;

		if (!respond_with_list && query_result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(query_result.best(), respond_with_list), "application/json");
	};
}

httplib::Server::Handler exact_handler(fuzzy::sorted_database<std::string> &database)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}

		auto query_string = req.get_param_value("q");
		bool respond_with_list = req.has_param("list") && req.get_param_value("list") == "yes";

		timer query_timer;
		auto query_result = database.exact_search(query_string);
		std::cout << "exact-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;

		if (!respond_with_list && query_result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(query_result.extract(), respond_with_list), "application/json");
	};
}

httplib::Server::Handler completion_handler(fuzzy::sorted_database<std::string> &database)
{
		return [&](const httplib::Request &req, httplib::Response &res)
	{
		if (!req.has_param("q"))
		{
			res.status = 400;
			res.set_content("missing query parameter q", "text/plain");
			return;
		}

		auto query_string = req.get_param_value("q");
		bool respond_with_list = !(req.has_param("list") && req.get_param_value("list") == "no");

		timer query_timer;
		auto query_result = database.completion_search(query_string);
		std::cout << "completion-searched " << query_string << " in " << query_timer.get() << "ms" << std::endl;

		if (!respond_with_list && query_result.empty())
		{
			res.status = 404;
			res.set_content("no matches", "text/plain");
			return;
		}
		res.set_content(process_results(query_result.extract(), respond_with_list), "application/json");
	};
}
