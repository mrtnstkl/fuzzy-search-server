#include <fstream>
#include <csignal>

#include "httplib.h"
#include "json.hpp"

#include "fuzzy.hpp"
#include "util.h"
#include "handlers.h"
#include "dataset.h"

#define RETURN_IF_QUIT(x) if (quit) return x 
#define PRINT_USAGE(argv0) std::cerr << "Usage: " << argv0 << " DATASET... [-p PORT] [-nf NAME_FIELD] [-l RESULT_LIMIT] [-bi | -tri | -tetra] [-fl] [-disk]" << std::endl

int port = 8080;
std::atomic_bool quit = false;

httplib::Server server;
std::vector<std::unique_ptr<dataset>> datasets;

void signal_handler(int signal)
{
	if (signal == SIGINT)
	{
		std::cout << "SIGINT received\nstopping" << std::endl;
		quit = true;
		server.stop();
	}
}

struct dataset_entry
{
	dataset::element_id element_id;
	uint16_t dataset_id;
	dataset_entry(dataset::element_id element_id = 0, uint16_t dataset_id = 0)
		: element_id(element_id), dataset_id(dataset_id)
	{
	}
	friend std::ostream& operator<<(std::ostream& os, const dataset_entry& dse)
	{
		os << datasets[dse.dataset_id]->get_element(dse.element_id);
		return os;
	}
};


int main(int argc, char const *argv[])
{
	if (argc < 2)
	{
		PRINT_USAGE(argv[0]);
		return 1;
	}

	// process args
	int ngram_size = 2;
	bool keep_elements_in_memory = true;
	bool enforce_first_letter_match = false;
	int result_limit = 100;
	const char* name_field = "name";
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
		if (arg == "-disk")
		{
			keep_elements_in_memory = false;
			continue;
		}
		if (arg == "-fl" || arg == "-first-letter")
		{
			enforce_first_letter_match = true;
			continue;
		}
		if (arg == "-p" || arg == "-port")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing parameter for " << arg << std::endl;
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
		if (arg == "-l" || arg == "-limit")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing parameter for " << arg << std::endl;
				PRINT_USAGE(argv[0]);
				return 1;
			}
			result_limit = atoi(argv[i + 1]);
			++i;
			continue;
		}
		if (arg == "-nf" || arg == "-name-field")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing parameter for " << arg << std::endl;
				PRINT_USAGE(argv[0]);
				return 1;
			}
			name_field = argv[i + 1];
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

	fuzzy::sorted_database<dataset_entry> database(ngram_size, result_limit > 0 ? result_limit : SIZE_MAX, enforce_first_letter_match);
	timer init_timer;

	std::signal(SIGINT, signal_handler);
	server.Get("/fuzzy", fuzzy_handler(database));
	server.Get("/fuzzy/list", fuzzy_list_handler(database));
	server.Get("/exact", exact_handler(database));
	server.Get("/exact/list", exact_list_handler(database));
	server.Get("/complete", completion_handler(database));
	server.Get("/complete/list", completion_list_handler(database));

	// process datasets
	int dataset_count = 0;
	for (const char* path : dataset_paths)
	{
		timer parse_timer;
		unsigned element_count = 0;
		std::cout << "parsing dataset \"" << path << '"' << std::endl;
		auto new_dataset = std::make_unique<dataset>(
			path, keep_elements_in_memory, quit,
			[&](dataset::element_id id, const std::string &str)
			{
				try
				{
					auto json = nlohmann::json::parse(str);
					database.add(json[name_field].template get<std::string>(), dataset_entry{id, uint16_t(datasets.size())});
					++element_count;
				}
				catch (const std::exception &e)
				{
					if (!str.empty())
					{
						std::cerr << "error while parsing line " << id << ": " << e.what() << std::endl;
					}
				}
			});
		RETURN_IF_QUIT(0);
		if (new_dataset->ready())
		{
			std::cout << "parsed " << element_count << " entries in " << parse_timer.get() << "ms" << std::endl;
			datasets.push_back(std::move(new_dataset));
			++dataset_count;
		}
		else if (element_count > 0)
		{
			// A file error occurred during parsing.
			// We don't want entries from broken files in our
			// database, but we can't get them out anymore.
			return 1; // ...So we abort
		}
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

	datasets.clear();
	return 0;
}
