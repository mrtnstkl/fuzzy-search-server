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
		"/:query",
		[&](const httplib::Request &req, httplib::Response &res)
		{
			auto query_string = req.path_params.at("query");
			auto start = std::chrono::steady_clock::now();
			auto query_result = database.fuzzy_search(query_string);
			auto end = std::chrono::steady_clock::now();
			using namespace std::chrono;
			std::cout << "fuzzy-searched " << query_string << " in " << duration_cast<milliseconds>(end - start).count() << "ms: "
					  << (query_result.empty() ? "not found" : query_result.best()[0].element->name) << std::endl;
			res.set_content(query_result.empty() ? "{}" : query_result.best()[0].element->meta, "application/json");
		});

	std::cout << "\nstarting server on port " << port << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	return 0;
}
