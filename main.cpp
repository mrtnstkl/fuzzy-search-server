#include <fstream>
#include <csignal>
#include <functional>
#include <string>
#include <unordered_set>

#include "httplib.h"
#include "json.hpp"

#include "fuzzy.hpp"
#include "util.h"
#include "handlers.h"
#include "dataset.h"

#define RETURN_IF_QUIT(x) if (quit) return x 
#define PRINT_USAGE(argv0) std::cerr << "Usage: " << argv0 << " DATASET... [-p PORT] [-nf NAME_FIELD] [-l RESULT_LIMIT] [-bc BUCKET_CAPACITY] [-bi | -tri | -tetra] [-fl] [-disk] [-dc]" << std::endl

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
	int port = 8080;
	int ngram_size = 2;
	bool keep_elements_in_memory = true;
	bool enforce_first_letter_match = false;
	bool check_duplicates = false;
	int result_limit = 100;
	long bucket_capacity = 1000;
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
		if (arg == "-dc" || arg == "-duplicate-check")
		{
			check_duplicates = true;
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
		if (arg == "-bc" || arg == "-bucket-cap")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing parameter for " << arg << std::endl;
				PRINT_USAGE(argv[0]);
				return 1;
			}
			bucket_capacity = atol(argv[i + 1]);
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
		if (arg[0] == '-')
		{
			std::cerr << "Invalid argument \"" << arg << '"' << std::endl;
			PRINT_USAGE(argv[0]);
			return 1;
		}
		dataset_paths.push_back(argv[i]);
	}
	if (dataset_paths.empty())
	{
		PRINT_USAGE(argv[0]);
		return 1;
	}

	fuzzy::sorted_database<dataset_entry> database(ngram_size, result_limit > 0 ? result_limit : SIZE_MAX, enforce_first_letter_match, bucket_capacity > 0 ? bucket_capacity : UINT64_MAX);
	timer init_timer;

	std::signal(SIGINT, signal_handler);
	server.Get("/fuzzy", fuzzy_handler(database));
	server.Get("/fuzzy/list", fuzzy_list_handler(database));
	server.Get("/fuzzycomplete", fuzzycomplete_handler(database));
	server.Get("/fuzzycomplete/list", fuzzycomplete_list_handler(database));
	server.Get("/exact", exact_handler(database));
	server.Get("/exact/list", exact_list_handler(database));
	server.Get("/complete", completion_handler(database));
	server.Get("/complete/list", completion_list_handler(database));
	server.set_post_routing_handler([](const auto&, auto& res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		return true;
	});
	server.Options(".*", [](const auto&, auto& res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		res.set_header("Access-Control-Allow-Methods", "GET");
		res.set_header("Access-Control-Allow-Headers", "Content-Type");
	});

	std::cout << "port set to " << port << std::endl;
	std::cout << "name field set to \"" << name_field << "\"" << std::endl;
	std::cout << "max page size set to " << (result_limit > 0 ? std::to_string(result_limit) : "unlimited") << std::endl;
	std::cout << "bucket capacity set to " << (bucket_capacity > 0 ? std::to_string(bucket_capacity) : "unlimited") << std::endl;
	std::cout << "using " << (ngram_size == 2 ? "bigrams" : (ngram_size == 3 ? "trigrams" : "tetragrams")) << std::endl;
	if (enforce_first_letter_match)
		std::cout << "enforcing first letter match for fuzzy search" << std::endl;
	if (keep_elements_in_memory)
		std::cout << "using in-memory mode" << std::endl;
	else
		std::cout << "using disk mode: do not modify dataset files while the program is running!" << std::endl;
	if (check_duplicates)
		std::cout << "entry duplication check enabled" << std::endl;
	std::cout << std::endl;


	unsigned dataset_count = 0;
	unsigned total_element_count = 0;
	unsigned current_dataset_element_count = 0;
	unsigned current_dataset_duplicates = 0;

	std::unordered_set<size_t> element_hashset; 
	std::function<void(dataset::element_id, const std::string&)> element_handler =
		[&](dataset::element_id id, const std::string &str)
		{
			try
			{
				auto json = nlohmann::json::parse(str);
				database.add(json[name_field].template get<std::string>(), dataset_entry{id, uint16_t(datasets.size())});
				++current_dataset_element_count;
			}
			catch (const std::exception &e)
			{
				if (!str.empty())
				{
					std::cerr << "error while parsing line " << id << ": " << e.what() << std::endl;
				}
			}
		};
	if (check_duplicates)
	{
		element_handler =
			[&, base_handler = element_handler, hasher = std::hash<std::string>{}]
			(dataset::element_id id, const std::string &str)
			{
				if (element_hashset.insert(hasher(str)).second) [[likely]]
					base_handler(id, str);
				else
					++current_dataset_duplicates;
			};
	}

	// process datasets
	for (const char* path : dataset_paths)
	{
		timer parse_timer;
		std::cout << "parsing dataset \"" << path << '"' << std::endl;
		auto new_dataset = std::make_unique<dataset>(path, keep_elements_in_memory, quit, element_handler);
		RETURN_IF_QUIT(0);
		if (new_dataset->ready())
		{
			std::cout << "parsed " << current_dataset_element_count << " entries in " << parse_timer.get() << "ms";
			if (current_dataset_duplicates > 0) std::cout << " (" << current_dataset_duplicates << " duplicates)";
			std::cout << std::endl;
			datasets.push_back(std::move(new_dataset));
			++dataset_count;
			total_element_count += current_dataset_element_count;
		}
		else if (current_dataset_element_count > 0)
		{
			// A file error occurred during parsing.
			// We don't want entries from broken files in our
			// database, but we can't get them out anymore.
			return 1; // ...So we abort
		}
		current_dataset_element_count = 0;
		current_dataset_duplicates = 0;
	}
	element_hashset = std::unordered_set<size_t>();
	std::cout << "processed " << total_element_count << " elements from " << dataset_count << "/" << dataset_paths.size() << " datasets" << std::endl;

	std::cout << "preparing database" << std::endl;
	timer db_init_timer;
	database.build();
	RETURN_IF_QUIT(0);
	std::cout << "database prepared in " << db_init_timer.stop().get() << "ms" << std::endl;

	std::cout << "\ninitialization took " << init_timer.stop().get() << "ms" << std::endl;

	server.Get("/info", [&](const auto &, httplib::Response &res) {
		res.set_content(
			nlohmann::json({
				{"ngramSize", ngram_size},
				{"inMemory", keep_elements_in_memory},
				{"duplicateCheck", check_duplicates},
				{"firstLetterMatch", enforce_first_letter_match},
				{"resultLimit", result_limit},
				{"datasetCount", dataset_count},
				{"elementCount", total_element_count},
				{"startupTime", init_timer.get()}
			}).dump(4),
			"application/json"
		);
	});

	std::cout << "\nstarting server on port " << port << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	datasets.clear();
	return 0;
}
