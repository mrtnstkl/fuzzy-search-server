#include "fss.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <regex>
#include <memory>

#include "handlers.h"
#include "util.h"
#include "json.hpp"

extern std::atomic_bool quit;

#define FSS_DEEP_NAME


dataset_entry::dataset_entry(dataset::element_id element_id, uint16_t dataset_id)
	: element_id(element_id), dataset_id(dataset_id)
{
}

bool dataset_entry::operator==(const dataset_entry &other) const
{
  	return element_id == other.element_id && dataset_id == other.dataset_id;
}

fss_options fss_options::or_else(const fss_options& other) const
{
	fss_options result;
	result.ngram_size = ngram_size.has_value() ? ngram_size : other.ngram_size;
	result.keep_elements_in_memory = keep_elements_in_memory.has_value() ? keep_elements_in_memory : other.keep_elements_in_memory;
	result.enforce_first_letter_match = enforce_first_letter_match.has_value() ? enforce_first_letter_match : other.enforce_first_letter_match;
	result.check_duplicates = check_duplicates.has_value() ? check_duplicates : other.check_duplicates;
	result.result_limit = result_limit.has_value() ? result_limit : other.result_limit;
	result.bucket_capacity = bucket_capacity.has_value() ? bucket_capacity : other.bucket_capacity;
	result.name_field = name_field.has_value() ? name_field : other.name_field;
	return result;
}

fss_options fss_options::or_defaults() const
{
	return or_else(defaults());
}

fss_options fss_options::defaults()
{
	fss_options options;
	options.ngram_size = 2;
	options.keep_elements_in_memory = true;
	options.enforce_first_letter_match = false;
	options.check_duplicates = false;
	options.result_limit = 100;
	options.bucket_capacity = 1000;
	options.name_field = std::regex("name");
	return options;
}

void fuzzy_search_server::print_element(const dataset_entry& entry, std::ostream& os) const
{
	os << datasets_[entry.dataset_id]->get_element(entry.element_id);
}

std::string fuzzy_search_server::format_results(const std::vector<fuzzy::result<dataset_entry>>& results, bool as_list)
{
	std::stringstream strstream;
	if (!as_list)
	{
		if (results.empty())
		{
			return "";
		}
		print_element(results[0].element->meta, strstream);
		return strstream.str();
	}
	if (results.empty())
	{
		return "[]";
	}

	strstream << "[\n";

	if (multi_key_)
	{
		bool first = true;
		std::unordered_set<dataset_entry> seen_elements;
		for (const auto& result : results)
		{
			auto db_entry = result.element->meta;
			if (seen_elements.find(db_entry) == seen_elements.end())
			{
				if (!first)
				{
					strstream << ",\n\t";
				}
				else
				{
					strstream << "\t";
					first = false;
				}
				print_element(db_entry, strstream);
				seen_elements.insert(db_entry);
			}
		}
	}
	else
	{
		bool first = true;
		for (const auto& result : results)
		{
			if (!first)
			{
				strstream << ",\n\t";
			}
			else
			{
				strstream << "\t";
				first = false;
			}
			print_element(result.element->meta, strstream);
		}
	}

	strstream << "\n]";
	return strstream.str();
}

fuzzy_search_server::fuzzy_search_server(const fss_options& options)
	: fuzzy_search_server(options.or_defaults().ngram_size.value(), options.or_defaults().result_limit.value(), options.or_defaults().bucket_capacity.value())
{
	options_ = options_.or_else(options);
}

fuzzy_search_server::fuzzy_search_server(int ngram_size, size_t result_limit, uint64_t max_bucket_size)
	: database_(ngram_size, result_limit, false, max_bucket_size)
{
	options_.ngram_size = ngram_size;
	options_.result_limit = result_limit;
	options_.bucket_capacity = max_bucket_size;
}

bool fuzzy_search_server::load_dataset(const char* path, bool keep_elements_in_memory, bool discard_duplicates, std::regex name_field)
{
	if (ready_)
	{
		std::cerr << "Cannot load dataset after the database has been finalized" << std::endl;
		return false;
	}

	unsigned element_count = 0;
	unsigned duplicate_count = 0;

	std::function<void(dataset::element_id, const std::string&)> element_handler =
		[&](dataset::element_id id, const std::string &str)
		{
			try
			{
				const auto entry = dataset_entry{id, uint16_t(datasets_.size())};
				bool added = false;
				const auto json = nlohmann::json::parse(str);
				#ifdef FSS_DEEP_NAME
				recursive_iterate(json,
					[&](const auto& it)
					{
						if (std::regex_match(it.key().c_str(), name_field) && it.value().is_string())
						{
							multi_key_ |= added;
							database_.add(it.value().template get<std::string>(), entry);
							added = true;
						}
					}
				);
				#else
				for (auto it = json.begin(); it != json.end(); ++it)
				{
					if (std::regex_match(it.key().c_str(), name_field) && it.value().is_string())
					{
						multi_key_ |= added;
						database_.add(it.value().template get<std::string>(), entry);
						added = true;
					}
				}
				#endif
				element_count += added ? 1 : 0;
			}
			catch (const std::exception &e)
			{
				if (!str.empty())
				{
					std::cerr << "error while parsing line " << id << ": " << e.what() << std::endl;
				}
			}
		};
	if (discard_duplicates)
	{
		element_handler =
			[&, base_handler = element_handler, hasher = std::hash<std::string>{}]
			(dataset::element_id id, const std::string &str)
			{
				if (element_hashes_.insert(hasher(str)).second) [[likely]]
					base_handler(id, str);
				else
					++duplicate_count;
			};
	}

	timer parse_timer;

	std::cout << "parsing dataset \"" << path << '"' << std::endl;
	auto new_dataset = std::make_unique<dataset>(path, keep_elements_in_memory, quit, element_handler);
	if (new_dataset->ready())
	{
		std::cout << "parsed " << element_count << " entries in " << parse_timer.get() << "ms";
		if (duplicate_count > 0) std::cout << " (" << duplicate_count << " duplicates)";
		std::cout << std::endl;
		datasets_.push_back(std::move(new_dataset));
		++dataset_count_;
		total_element_count_ += element_count;
		return true;
	}
	else if (element_count > 0)
	{
		// A file error occurred during parsing.
		// We don't want entries from broken files in our
		// database, but we can't get them out anymore.
		return false; // ...So we abort
	}
	return false;
}

bool fuzzy_search_server::finalize()
{
	std::cout << "preparing database" << std::endl;
	element_hashes_ = std::unordered_set<size_t>{}; // free memory used for duplicate checking
	timer db_init_timer;
	database_.build();
	std::cout << "database prepared in " << db_init_timer.stop().get() << "ms" << std::endl;
	ready_ = true;
	return true;
}

std::string fuzzy_search_server::search_fuzzy(std::string q, bool as_list)
{
	timer query_timer;
	// try an exact search first, because it is much faster
	auto result = database_.exact_search(q, 0, as_list ? SIZE_MAX : 1);
	if (result.empty())
	{
		result = database_.fuzzy_search(q);
	}
	std::cout << "fuzzy-searched " << q << " in " << query_timer.get() << "ms" << std::endl;
	return format_results(result.best(), as_list);
}

std::string fuzzy_search_server::search_exact(std::string q, bool as_list, int page, int count)
{
	timer query_timer;
	auto result = database_.exact_search(q, 0, as_list ? SIZE_MAX : 1);
	std::cout << "exact-searched " << q << " in " << query_timer.get() << "ms" << std::endl;
	return format_results(result.best(), as_list);
}

std::string fuzzy_search_server::search_complete(std::string q, bool as_list, int page, int count)
{
	timer query_timer;
	auto query_result = database_.completion_search(q, std::max(0, page), std::max(0, count));
	std::cout << "completion-searched " << q << " in " << query_timer.get() << "ms" << std::endl;
	return format_results(query_result.all(), as_list);
}

std::string fuzzy_search_server::search_fuzzy_complete(std::string q, bool as_list, int tol, int max)
{
	constexpr auto max_distance = INT_MAX;
	constexpr auto max_results = 50;
	timer query_timer;
	auto query_result = database_.fuzzy_search(q, q.length());
	const auto result_list = as_list
		? query_result.extract(0, max_results, true, tol, max_distance)
		: query_result.extract(0, 1, true);
	std::cout << "fuzzycomplete-searched " << q << " in " << query_timer.get() << "ms" << std::endl;
	return format_results(result_list, as_list);
}

void fuzzy_search_server::set_handlers(httplib::Server &server)
{
	auto base_url = base_url_;
	if (!base_url.empty() && !base_url.starts_with('/'))
	{
		base_url = '/' + base_url;
	}
	std::cout << "configuring endpoints at " << (base_url.empty() ? "/" : base_url) << std::endl;
	server.Get((base_url.empty() ? "/" : base_url), info_handler(*this));
	server.Get(base_url + "/fuzzy", fuzzy_handler(*this));
	server.Get(base_url + "/fuzzy/list", fuzzy_list_handler(*this));
	server.Get(base_url + "/fuzzycomplete", fuzzycomplete_handler(*this));
	server.Get(base_url + "/fuzzycomplete/list", fuzzycomplete_list_handler(*this));
	server.Get(base_url + "/exact", exact_handler(*this));
	server.Get(base_url + "/exact/list", exact_list_handler(*this));
	server.Get(base_url + "/complete", completion_handler(*this));
	server.Get(base_url + "/complete/list", completion_list_handler(*this));
}

fuzzy_search_server fuzzy_search_server::from_config(const nlohmann::json& config, const fss_options& defaults)
{
	fss_options options = defaults.or_else(fss_options::defaults());

	if (config.contains("ngramSize"))
	{
		auto str = config.at("ngramSize").get<std::string>();
		if (str == "bi")
			options.ngram_size = 2;
		else if (str == "tri")
			options.ngram_size = 3;
		else if (str == "tetra")
			options.ngram_size = 4;
		else
			throw std::runtime_error("invalid ngram size: " + str);
	}
	if (config.contains("bucketCapacity"))
		options.bucket_capacity = config.at("bucketCapacity").get<int>();
	if (config.contains("maxResults"))
		options.result_limit = config.at("maxResults").get<int>();
	if (config.contains("allowDuplicates"))
		options.check_duplicates = !config.at("allowDuplicates").get<bool>();
	if (config.contains("enforceFirstLetterMatch"))
		options.enforce_first_letter_match = config.at("enforceFirstLetterMatch").get<bool>();
	if (config.contains("disk"))
		options.keep_elements_in_memory = !config.at("disk").get<bool>();
	if (config.contains("key"))
		options.name_field = std::regex(config.at("key").get<std::string>());

	fuzzy_search_server fss(options.ngram_size.value(), options.result_limit.value(), options.bucket_capacity.value());

	if (config.contains("baseUrl"))
		fss.base_url_ = config.at("baseUrl").get<std::string>();

	for (const auto& dataset_config : config.at("datasets"))
	{			
		bool in_memory = options.keep_elements_in_memory.value();
		std::regex name_field = options.name_field.value();
		std::string path;
		if (dataset_config.is_string())
		{
			path = dataset_config.get<std::string>();
		}
		else
		{
			path = dataset_config.at("path").get<std::string>();
			if (dataset_config.contains("disk"))
				in_memory = !dataset_config.at("disk").get<bool>();
			if (dataset_config.contains("key"))
				name_field = std::regex(dataset_config.at("key").get<std::string>());
		}
		if (!fss.load_dataset(path.c_str(), in_memory, options.check_duplicates.value(), name_field))
		{
			throw std::runtime_error("failed to load dataset \"" + path + "\"");
		}
	}
	fss.options_ = options;
	fss.finalize();
	return fss;
}
