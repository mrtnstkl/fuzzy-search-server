#include <fstream>
#include <csignal>

#include "httplib.h"
#include "json.hpp"

#include "fuzzy.hpp"
#include "util.h"
#include "handlers.h"

#define RETURN_IF_QUIT(x) if (quit) return x 
#define PRINT_USAGE(argv0) std::cerr << "Usage: " << argv0 << " DATASET... [-p PORT] [-bi | -tri | -tetra]" << std::endl

int port = 8080;
std::atomic_bool quit = false;

httplib::Server server;

void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		std::cout << "SIGINT received\nstopping" << std::endl;
		quit = true;
		server.stop();
	}
}

bool populate_database(fuzzy::sorted_database<std::string> &database, const char *dataset_path)
{
	std::ifstream input(dataset_path);
	std::string line;
	timer parse_timer;
	unsigned line_count = 0;
	unsigned element_count = 0;
	if (!input.is_open())
	{
		std::cerr << "could not open dataset \"" << dataset_path << '"' << std::endl;
		return false;
	}
	std::cout << "parsing dataset \"" << dataset_path << '"' << std::endl;

	while (std::getline(input, line), !line.empty() || input.good())
	{
		RETURN_IF_QUIT(false);
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
	std::cout << "parsed " << element_count << " entries in " << parse_timer.get() << "ms" << std::endl;
	return true;
}

int main(int argc, char const *argv[])
{
	if (argc < 2)
	{
		PRINT_USAGE(argv[0]);
		return 1;
	}

	// process args
	int ngram_size = 2;
	std::vector<const char*> dataset_paths;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		if (arg == "-bi")
		{
			ngram_size = 2;
			continue;
		}
		if (arg == "-tri")
		{
			ngram_size = 3;
			continue;
		}
		if (arg == "-tetra")
		{
			ngram_size = 4;
			continue;
		}
		if (arg == "-p")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing parameter for -p" << std::endl;
				PRINT_USAGE(argv[0]);
				return 1;
			}
			if (atoi(argv[i + 1]) == 0)
			{
				std::cerr << "Invalid port \"" << argv[i + 1] << '"' << std::endl;
				PRINT_USAGE(argv[0]);
				return 1;
			}
			port = atoi(argv[i + 1]);
			++i;
			continue;
		}
		dataset_paths.push_back(argv[i]);

	}
	if (dataset_paths.empty())
	{
		PRINT_USAGE(argv[0]);
		return 1;
	}

	fuzzy::sorted_database<std::string> database(ngram_size);
	timer init_timer;

	std::signal(SIGINT, signal_handler);
	server.Get("/fuzzy", fuzzy_handler(database));
	server.Get("/fuzzy/list", fuzzy_list_handler(database));
	server.Get("/exact", exact_handler(database));
	server.Get("/exact/list", exact_list_handler(database));
	server.Get("/complete", completion_handler(database));
	server.Get("/complete/list", completion_list_handler(database));

	int dataset_count = 0;
	for (const char* path : dataset_paths)
	{
		dataset_count += populate_database(database, path) ? 1 : 0;
		RETURN_IF_QUIT(0);
	}

	std::cout << "processed " << dataset_count << "/" << dataset_paths.size() << " datasets" << std::endl;

	std::cout << "preparing database" << std::endl;
	timer db_init_timer;
	database.build();
	RETURN_IF_QUIT(0);
	std::cout << "database prepared in " << db_init_timer.get() << "ms" << std::endl;

	std::cout << "\ninitialization took " << init_timer.get() << "ms" << std::endl;

	std::cout << "\nstarting server on port " << port << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	return 0;
}
