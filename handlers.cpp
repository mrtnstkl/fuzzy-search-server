#include "handlers.h"

#include "httplib.h"
#include "fss.h"

httplib::Server::Handler info_handler(fuzzy_search_server &fss)
{
	return [&](const httplib::Request &req, httplib::Response &res)
	{
		nlohmann::json info_json;
		info_json["baseUrl"] = fss.base_url_;
		info_json["datasetCount"] = fss.dataset_count_;
		info_json["totalElementCount"] = fss.total_element_count_;
		info_json["multiKey"] = fss.multi_key_;
		info_json["ready"] = fss.ready_;
		nlohmann::json options_json;
		if (fss.options_.ngram_size.has_value())
			options_json["ngramSize"] = fss.options_.ngram_size.value();
		if (fss.options_.result_limit.has_value())
			options_json["resultLimit"] = fss.options_.result_limit.value();
		if (fss.options_.bucket_capacity.has_value())
			options_json["bucketCapacity"] = fss.options_.bucket_capacity.value();
		if (fss.options_.enforce_first_letter_match.has_value())
			options_json["enforceFirstLetterMatch"] = fss.options_.enforce_first_letter_match.value();
		if (fss.options_.check_duplicates.has_value())
			options_json["checkDuplicates"] = fss.options_.check_duplicates.value();
		if (fss.options_.keep_elements_in_memory.has_value())
			options_json["keepElementsInMemory"] = fss.options_.keep_elements_in_memory.value();
		// if (fss.options_.name_field.has_value())
		// 	options_json["nameField"] = fss.options_.name_field.value();
		info_json["options"] = options_json;

		res.set_content(info_json.dump(4), "application/json");
	};
}

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
