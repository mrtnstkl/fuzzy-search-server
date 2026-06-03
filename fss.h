#pragma once

#include <cstddef>
#include <exception>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>
#include <regex>

#include "dataset.h"
#include "fuzzy.hpp"

struct dataset_entry
{
	dataset::element_id element_id;
	uint16_t dataset_id;
	dataset_entry(dataset::element_id element_id = 0, uint16_t dataset_id = 0)
		: element_id(element_id), dataset_id(dataset_id)
	{
	}
	bool operator==(const dataset_entry& other) const
	{
		return element_id == other.element_id && dataset_id == other.dataset_id;
	}
};

class fuzzy_search_server
{
	std::vector<std::unique_ptr<dataset>> datasets_;
	fuzzy::sorted_database<dataset_entry> database_;

	unsigned dataset_count_ = 0;
	unsigned total_element_count_ = 0;
	bool multi_key_ = false;
	bool ready_ = false;
	std::unordered_set<size_t> element_hashes_;

	void print_element(const dataset_entry& entry, std::ostream& os) const;
	std::string format_results(const std::vector<fuzzy::result<dataset_entry>>& results, bool as_list);

public:
	fuzzy_search_server(int ngram_size = 2, size_t result_limit = 100, uint64_t max_bucket_size = UINT64_MAX)
		: database_(ngram_size, result_limit, false, max_bucket_size)
	{
	}

	bool load_dataset(const char* path, bool keep_elements_in_memory, bool discard_duplicates, std::regex name_field);
	bool finalize();

	std::string search_fuzzy(std::string q, bool as_list);
	std::string search_exact(std::string q, bool as_list, int page = 0, int count = 10);
	std::string search_complete(std::string q, bool as_list, int page = 0, int count = 10);
	std::string search_fuzzy_complete(std::string q, bool as_list, int tol = 2, int max = 50);

};



namespace std
{
	template <>
	struct hash<dataset_entry>
	{
		size_t operator()(const dataset_entry& entry) const
		{
			return entry.element_id ^ (entry.dataset_id << 16);
		}
	};
}