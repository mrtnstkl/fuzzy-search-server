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

class timer
{
	std::chrono::steady_clock::time_point start_, stop_;
	bool running_;

public:
	timer()
	{
		reset();
	}
	void reset()
	{
		start_ = std::chrono::steady_clock::now();
		running_ = true;
	}
	void stop()
	{
		running_ = false;
		stop_ = std::chrono::steady_clock::now();
	}
	template <typename Duration = std::chrono::milliseconds>
	uint64_t get()
	{
		return std::chrono::duration_cast<Duration>((running_ ? std::chrono::steady_clock::now() : stop_) - start_).count();
	}
	template <typename Duration = std::chrono::milliseconds>
	uint64_t get_and_reset()
	{
		auto v = get<Duration>();
		reset();
		return v;
	}
};

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

	server.Get(
		"/fuzzy",
		[&](const httplib::Request &req, httplib::Response &res)
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
		});

	std::cout << "\nstarting server on port " << port << std::endl;
	if (!server.listen("0.0.0.0", port))
	{
		std::cerr << "failed to start server" << std::endl;
	}

	return 0;
}
