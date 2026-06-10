#pragma once

#include <cstddef>
#include <string>
#include <unordered_set>
#include <vector>
#include <memory>
#include <optional>
#include <regex>

#include "dataset.h"
#include "fuzzy.hpp"
#include "httplib.h"
#include "json.hpp"

struct dataset_entry
{
	dataset::element_id element_id;
	uint16_t dataset_id;
	dataset_entry(dataset::element_id element_id = 0, uint16_t dataset_id = 0);
	bool operator==(const dataset_entry &other) const;
};

struct fss_options
{
	std::optional<int> ngram_size;
	std::optional<bool> keep_elements_in_memory;
	std::optional<bool> enforce_first_letter_match;
	std::optional<bool> check_duplicates;
	std::optional<int> result_limit;
	std::optional<long> bucket_capacity;
	std::optional<std::regex> name_field;

	fss_options or_else(const fss_options& other) const;
	fss_options or_defaults() const;

	static fss_options defaults();
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

	std::string base_url_ = "";
	fss_options options_;

	void print_element(const dataset_entry& entry, std::ostream& os) const;
	std::string format_results(const std::vector<fuzzy::result<dataset_entry>>& results, bool as_list);

public:
	fuzzy_search_server(const fss_options& options);
	fuzzy_search_server(int ngram_size = 2, size_t result_limit = 100, uint64_t max_bucket_size = UINT64_MAX);

	bool load_dataset(const char* path, bool keep_elements_in_memory, bool discard_duplicates, std::regex name_field);
	bool finalize();

	std::string search_fuzzy(std::string q, bool as_list);
	std::string search_exact(std::string q, bool as_list, int page = 0, int count = 10);
	std::string search_complete(std::string q, bool as_list, int page = 0, int count = 10);
	std::string search_fuzzy_complete(std::string q, bool as_list, int tol = 2, int max = 50);

	void set_handlers(httplib::Server &server);

	static fuzzy_search_server from_config(const nlohmann::json &config, const fss_options &defaults = fss_options());

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