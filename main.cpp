#include <fstream>
#include <csignal>

#include "httplib.h"
#include "json.hpp"

#include "fuzzy.hpp"

int port = 8080;

httplib::Server server;

void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		std::cout << "SIGINT received\nstopping server" << std::endl;
		server.stop();
	}
}

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


int main(int argc, char const *argv[])
{
	std::signal(SIGINT, signal_handler);

	if (argc < 2 || argc > 3)
	{
		std::cerr << "Usage: " << argv[0] << " INFILE [PORT]\n";
		return 1;
	}

	if (argc == 3)
	{
		port = atoi(argv[2]);
	}

	fuzzy::database<std::string> database;

	std::ifstream input(argv[1]);
	std::string line;
	unsigned element_count = 0;
	if (input.good())
	{
		std::cout << "reading entries." << std::flush;
	}
	while (input.good())
	{
		std::getline(input, line);
		try
		{
			auto json = nlohmann::json::parse(line);
			database.add(json["name"].template get<std::string>(), line);
			++element_count;
		}
		catch (const std::exception &e)
		{
		}
	}
	input.close();
	std::cout
		<< " done\n"
		<< "processed " << element_count << " entries" << std::endl;

	server.Get(
		"/fuzzy",
		[&](const httplib::Request &req, httplib::Response &res)
		{
			using namespace std::chrono;

			if (!req.has_param("q"))
			{
				res.status = 400;
				res.set_content("missing query parameter q", "text/plain");
				return;
			}

			auto query_string = req.get_param_value("q");
			bool respond_with_list = req.has_param("list") && req.get_param_value("list") == "yes";

			auto start = steady_clock::now();
			auto query_result = database.fuzzy_search(query_string);
			auto end = steady_clock::now();
			std::cout
				<< "fuzzy-searched " << query_string << " in " << duration_cast<milliseconds>(end - start).count() << "ms: "
				<< (query_result.empty() ? "not found" : query_result.best()[0].element->name) << std::endl;

			if (!respond_with_list && query_result.empty())
			{
				res.status = 404;
				res.set_content("no matches", "text/plain");
				return;
			}
			res.set_content(process_results(query_result.best(), respond_with_list), "application/json");
		});

	std::cout << "\nstarting server on port " << port << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	return 0;
}
