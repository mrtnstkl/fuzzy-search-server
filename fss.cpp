#include "fss.h"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <regex>
#include <memory>

#include "util.h"
#include "json.hpp"

extern std::atomic_bool quit;

#define FSS_DEEP_NAME

void fuzzy_search_server::print_element(const dataset_entry& entry, std::ostream& os) const
{
	os << datasets_[entry.dataset_id]->get_element(entry.element_id);
}

std::string fuzzy_search_server::format_results(const std::vector<fuzzy::result<dataset_entry>>& results, bool as_list)
{
	std::stringstream strstream;
	if (!as_list)
	{
		assert(!results.empty());
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

bool fuzzy_search_server::load_dataset(const char* path, bool keep_elements_in_memory, bool discard_duplicates, std::regex name_field)
{
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
	}
	else if (element_count > 0)
	{
		// A file error occurred during parsing.
		// We don't want entries from broken files in our
		// database, but we can't get them out anymore.
		return false; // ...So we abort
	}
	return true;
}

bool fuzzy_search_server::finalize()
{
	std::cout << "preparing database" << std::endl;
	element_hashes_ = std::unordered_set<size_t>{}; // free memory used for duplicate checking
	timer db_init_timer;
	database_.build();
	std::cout << "database prepared in " << db_init_timer.stop().get() << "ms" << std::endl;
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
