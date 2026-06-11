#include <csignal>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>

#include "httplib.h"
#include "json.hpp"

#include "util.h"
#include "fss.h"

#define RETURN_IF_QUIT(x) if (quit) return x 
#define PRINT_USAGE(argv0) std::cerr << "Usage: " << argv0 << " DATASET... [-p PORT] [-c CONFIG_FILE] [-nf NAME_FIELD] [-l RESULT_LIMIT] [-bc BUCKET_CAPACITY] [-bi | -tri | -tetra] [-fl] [-disk] [-dc]" << std::endl

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


int main(int argc, char const *argv[])
{
	if (argc < 2)
	{
		PRINT_USAGE(argv[0]);
		return 1;
	}

	// process args
	int port = 8080;
	fss_options defaults;
	std::vector<const char*> dataset_paths;
	const char* config_path = nullptr;
	for (int i = 1; i < argc; i++)
	{
		const std::string arg = argv[i];
		if (arg == "-bi")
		{
			defaults.ngram_size = 2;
			continue;
		}
		if (arg == "-tri")
		{
			defaults.ngram_size = 3;
			continue;
		}
		if (arg == "-tetra")
		{
			defaults.ngram_size = 4;
			continue;
		}
		if (arg == "-disk")
		{
			defaults.keep_elements_in_memory = false;
			continue;
		}
		if (arg == "-fl" || arg == "-first-letter")
		{
			defaults.enforce_first_letter_match = true;
			continue;
		}
		if (arg == "-dc" || arg == "-duplicate-check")
		{
			defaults.check_duplicates = true;
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
			defaults.result_limit = atoi(argv[i + 1]);
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
			defaults.bucket_capacity = atol(argv[i + 1]);
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
			defaults.name_field = std::regex(argv[i + 1]);
			++i;
			continue;
		}
		if (arg == "-c" || arg == "-config")
		{
			if (i + 1 >= argc)
			{
				std::cerr << "Missing parameter for " << arg << std::endl;
				PRINT_USAGE(argv[0]);
				return 1;
			}
			config_path = argv[i + 1];
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
	if (dataset_paths.empty() && config_path == nullptr)
	{
		PRINT_USAGE(argv[0]);
		return 1;
	}


	nlohmann::json config_json = nlohmann::json::array();
	try
	{
		if (config_path != nullptr)
		{
			std::ifstream (config_path) >> config_json;
			if (config_json.is_object())
			{
				config_json = nlohmann::json::array({config_json});
			}
		}
	}
	catch (const std::exception& e)
	{
		throw std::runtime_error("Failed to parse config file: " + std::string(e.what()));
	}

	if (!dataset_paths.empty())
	{
		config_json.push_back(nlohmann::json{{"datasets", dataset_paths}});
	}

	if (defaults.result_limit.has_value() && defaults.result_limit.value() <= 0)
		defaults.result_limit = SIZE_MAX;
	if (defaults.bucket_capacity.has_value() && defaults.bucket_capacity.value() <= 0)
		defaults.bucket_capacity = UINT64_MAX;


	std::signal(SIGINT, signal_handler);
	timer init_timer;
	std::list<fuzzy_search_server> search_modules;

	for (int i = 0; const auto& module_config : config_json)
	{
		if (config_path != nullptr)
		{
			std::cout << "loading module " << ++i << "/" << config_json.size();
			if (module_config.contains("baseUrl"))
			{
				std::cout << " (" << module_config.at("baseUrl").get<std::string>() << ")";
			}
			std::cout << "..." << std::endl;
		}
		try
		{
			search_modules.push_back(fuzzy_search_server::from_config(module_config, defaults));
		}
		catch (const std::exception& e)
		{
			std::cerr << "Failed to load module " << i << ": " << e.what() << std::endl;
			return 1;
		}
		std::cout << "done loading module " << i << std::endl;
	}
	std::cout << "\nloaded " << search_modules.size() << " search module(s)\n" << std::endl;

	for (auto& search_module : search_modules)
	{
		search_module.set_handlers(server);
	}

	std::cout << "\ninitialization took " << init_timer.stop().get() << "ms" << std::endl;

	server.Get("/info", [&](const auto &, httplib::Response &res) {
		res.set_content(
			nlohmann::json({
				{"startupTime", init_timer.get()}
			}).dump(4),
			"application/json"
		);
	});
	server.set_post_routing_handler([](const auto&, auto& res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		return true;
	});
	server.Options(".*", [](const auto&, auto& res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		res.set_header("Access-Control-Allow-Methods", "GET");
		res.set_header("Access-Control-Allow-Headers", "Content-Type");
	});


	std::cout << "\nstarting server on port " << port << "\n" << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	return 0;
}
