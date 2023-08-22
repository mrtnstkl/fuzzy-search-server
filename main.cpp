#include <fstream>
#include <csignal>

#include "httplib.h"
#include "json.hpp"

#include "fuzzy.hpp"
#include "util.h"
#include "handlers.h"

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

void initialize_database(fuzzy::sorted_database<std::string> &database, const char *dataset_path)
{
	std::ifstream input(dataset_path);
	std::string line;
	timer init_timer;
	unsigned line_count = 0;
	unsigned element_count = 0;
	if (input.good())
	{
		std::cout << "parsing dataset" << std::endl;
	}
	while (std::getline(input, line), !line.empty() || input.good())
	{
		try
		{
			auto json = nlohmann::json::parse(line);
			database.add(json["name"].template get<std::string>(), line);
			++element_count;
		}
		catch (const std::exception &e)
		{
			if (!line.empty())
			{
				std::cerr << "error while parsing line " << line_count << ": " << e.what() << std::endl;
			}
		}
		++line_count;
	}
	input.close();
	std::cout << "parsed " << element_count << " entries in " << init_timer.get_and_reset() << "ms" << std::endl;
	std::cout << "preparing database " << std::flush;
	database.build();
	std::cout << "took " << init_timer.get_and_reset() << "ms" << std::endl;
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

	fuzzy::sorted_database<std::string> database;
	initialize_database(database, argv[1]);
	
	server.Get("/fuzzy", fuzzy_handler(database));

	std::cout << "\nstarting server on port " << port << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	return 0;
}
