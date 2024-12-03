#pragma once

#include <climits>
#include <cassert>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <algorithm>
#include <ranges>

namespace fuzzy
{
	using ngram_token = uint32_t;
	using ngram_char = uint8_t;

	using string = std::basic_string<ngram_char>;
	using string_view = std::basic_string_view<ngram_char>;

	using id_type = uint32_t;

	constexpr size_t bigram_limit = 6;
	constexpr size_t trigram_limit = 12;

	namespace internal
	{
		inline ngram_token make_token(ngram_char c1 = '\0', ngram_char c2 = '\0', ngram_char c3 = '\0', ngram_char c4 = '\0')
		{
			return ngram_token(c1) << 0 | ngram_token(c2) << 8 | ngram_token(c3) << 16 | ngram_token(c4) << 24;
		}

		inline fuzzy::string to_ngram_string(const std::string_view str)
		{
			// assumes str to be utf-8 encoded
			const uint8_t* ptr = (uint8_t*)str.data();
			const uint8_t* end = (uint8_t*)(ptr + str.size());			
			fuzzy::string ngram_str;

			while (ptr < end)
			{
				uint32_t char_value;
				if ((ptr[0] & 0b10000000) == 0)
				{
					// single byte utf-8 character
					ngram_str.push_back(ngram_char(tolower(ptr[0])));
					ptr += 1;
					continue;
				}
				else if ((ptr[0] & 0b11100000) == 0b11000000)
				{
					// two byte utf-8 character
					char_value = (uint16_t(ptr[0] & 0b00011111) << 6) | (ptr[1] & 0b00111111);
					ptr += 2;
				}
				else if ((ptr[0] & 0b11110000) == 0b11100000)
				{
					// three byte utf-8 character
					char_value = (uint16_t(ptr[0] & 0b00001111) << 12) | (uint16_t(ptr[1] & 0b00111111) << 6) | (ptr[2] & 0b00111111);
					ptr += 3;
				}
				else if ((ptr[0] & 0b11111000) == 0b11110000)
				{
					// four byte utf-8 character
					char_value = (uint16_t(ptr[0] & 0b00000111) << 18) | (uint16_t(ptr[1] & 0b00111111) << 12) | (uint16_t(ptr[2] & 0b00111111) << 6) | (ptr[3] & 0b00111111);
					ptr += 4;
				}
				else
				{
					// invalid utf-8 character
					++ptr;
					continue;
				}
				ngram_str.push_back(ngram_char(1 + char_value % 31));
			}
			return ngram_str;
		}

		inline bool string_compare(const fuzzy::string_view a, const fuzzy::string_view b)
		{
			for (uint16_t i = 0; i < std::min(a.size(), b.size()); ++i)
			{
				if (a[i] < b[i])
					return true;
				if (a[i] > b[i])
					return false;
			}
			return a.size() < b.size();
		}

		inline bool string_starts_with(const fuzzy::string_view str, const fuzzy::string_view sub_str)
		{
			if (str.size() < sub_str.size())
				return false;
			for (uint16_t i = 0; i < sub_str.size(); ++i)
			{
				if (str[i] != sub_str[i])
					return false;
			}
			return true;
		}

		inline int osa_distance(fuzzy::string_view s1, fuzzy::string_view s2)
		{
			const int len_s1 = s1.size();
			const int len_s2 = s2.size();

			std::vector<int> prev(len_s2 + 1);
			std::vector<int> curr(len_s2 + 1);

			for (int j = 0; j <= len_s2; j++)
			{
				prev[j] = j;
			}

			for (int i = 1; i <= len_s1; i++)
			{
				curr[0] = i;
				for (int j = 1; j <= len_s2; j++)
				{
					const int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
					curr[j] = std::min({
						prev[j] + 1,        // deletion
						curr[j - 1] + 1,    // insertion
						prev[j - 1] + cost  // substitution
					});

					if (s1[i - 1] == s2[j - 2] && s1[i - 2] == s2[j - 1] && i > 1 && j > 1)
					{
						curr[j] = std::min(curr[j], prev[j - 2] + 1); // transposition
					}
				}
				std::swap(prev, curr);
			}
			return prev[len_s2];
		}

		inline std::vector<ngram_token> ngram_tokens(const fuzzy::string_view str, const int ngram_size)
		{
			std::vector<ngram_token> tokens;
			switch (ngram_size)
			{
			case 2:
				// bigrams
				for (size_t i = 0; i + 1 < str.length(); i++)
					tokens.push_back(make_token(str[i], str[i + 1]));
				break;
			case 3:
				// trigrams
				for (size_t i = 0; i + 2 < str.length(); i++)
					tokens.push_back(make_token(str[i], str[i + 1], str[i + 2]));
				// for short words, also do bigrams
				if (str.length() <= bigram_limit)
				{
					for (size_t i = 0; i + 1 < str.length(); i++)
						tokens.push_back(make_token(str[i], str[i + 1]));
				}
				break;
			case 4:
				// tetragrams
				for (size_t i = 0; i + 3 < str.length(); i++)
					tokens.push_back(make_token(str[i], str[i + 1], str[i + 2], str[i + 3]));
				// for short words, also do trigrams
				if (str.length() <= trigram_limit)
				{
					for (size_t i = 0; i + 2 < str.length(); i++)
						tokens.push_back(make_token(str[i], str[i + 1], str[i + 2]));
				}
				// ...and bigrams
				if (str.length() <= bigram_limit)
				{
					for (size_t i = 0; i + 1 < str.length(); i++)
						tokens.push_back(make_token(str[i], str[i + 1]));
				}
				break;
			default:
				abort();
			}
			return tokens;
		}

	}

	using namespace internal;

	// stores a name, and meta info of type T
	template <typename T>
	struct db_entry
	{
		fuzzy::string name;
		T meta;
	};

	template <typename T>
	using db_entry_reference = db_entry<T>*;

	// represents a search result
	// contains a reference to a database entry, along with a distance indicating similarity
	template <typename T>
	struct result
	{
		db_entry_reference<T> element;
		int distance;
		result(db_entry_reference<T> element, int distance)
			: element(element), distance(distance)
		{
		}
	};

	// a std::vector<result<T>> with added functionality
	template <typename T>
	class result_list : public std::vector<result<T>>
	{
		public:
		static constexpr auto length_sort_func = [](const result<T>& a, const result<T>& b)
		{ 
			return a.element->name.length() < b.element->name.length(); 
		};

		result_list<T>& length_sort()
		{
			std::sort(this->begin(), this->end(), length_sort_func);
			return *this;
		}
	};

	// a structured container for search results
	// results are sorted by their distance, allowing for retrieval of best matches
	template <typename T>
	class result_collection
	{
		std::map<int, result_list<T>> results_;
		size_t size_;

	public:
		void add(db_entry_reference<T> element, int distance)
		{
			results_[distance].emplace_back(element, distance);
			++size_;
		}

		bool empty() const
		{
			return results_.empty();
		}

		size_t size() const
		{
			return size_;
		}

		result_list<T> best()
		{
			return empty() ? result_list<T>{} : results_.begin()->second;
		}

		result_list<T> all()
		{
			result_list<T> extracted_results;
			for (const auto &[result_distance, results_at_that_distance] : results_)
			{
				for (result<T> result : results_at_that_distance)
				{
					extracted_results.push_back(result);
				}
			}
			return extracted_results;
		}

		result_list<T> extract(uint32_t min_count, uint32_t max_count = UINT32_MAX, bool length_sort = false, int distance_range = INT_MAX, int max_distance = INT_MAX)
		{
			result_list<T> extracted_results;
			long best_distance = INT_MAX;
			for (const auto &[result_distance, results_at_that_distance] : results_)
			{
				best_distance = std::min(long(result_distance), best_distance);

				if (long(result_distance) > best_distance + long(distance_range) && extracted_results.size() >= min_count)
				{
					// we already have min_count results, and all further results are too far away from be best result
					break;
				}
				if (result_distance > max_distance)
				{
					// all further results exceed the max_distance
					break;
				}
				const auto old_size = extracted_results.size();
				for (result<T> result : results_at_that_distance)
				{
					extracted_results.push_back(result);
				}
				if (length_sort)
				{
					std::sort(extracted_results.begin() + old_size, extracted_results.end(), result_list<T>::length_sort_func);
				}
				if (extracted_results.size() >= max_count)
				{
					// max_count has been reached
					extracted_results.erase(extracted_results.begin() + max_count, extracted_results.end());
					break;
				}
			}
			return extracted_results;
		}
	};

	template <typename T>
	class database
	{
	protected:

		// groups element ids by name length
		class element_bucket
		{
			// element count
			uint64_t elements_ = 0;
			// groups element ids by name length
			std::map<uint16_t, std::vector<id_type>> data_;

		public:
			void add(id_type id, uint16_t word_length)
			{
				data_[word_length].push_back(id);
				++elements_;
			}
			std::map<uint16_t, std::vector<id_type>> &get()
			{
				return data_;
			}
			uint64_t size() const
			{
				return elements_;
			}
		};

		// maps ngram tokens to element buckets
		std::unordered_map<ngram_token, element_bucket> inverted_index_;
		// all the database entries
		std::vector<db_entry<T>> data_;

		id_type id_counter_ = 0;
		bool ready_ = false;

		const struct {
			const int ngram_size;
			bool first_letter_opt;
			uint64_t max_bucket_size;
		} options_;

		void add_to_index(const fuzzy::string& name, id_type id)
		{
			const auto tokens = ngram_tokens(name, options_.ngram_size);
			for (auto token : tokens)
			{
				inverted_index_[token].add(id, name.length());
			}
		}

		void remove_overfull_buckets()
		{
			if (options_.max_bucket_size == UINT64_MAX)
			{
				return;
			}
			const auto max = options_.max_bucket_size;
			std::erase_if(database<T>::inverted_index_,
				[max](const std::pair<ngram_token, typename database<T>::element_bucket> &entry)
				{ return entry.second.size() > max; });
		}

		virtual void add(std::string_view name, T&& meta, id_type id)
		{
			if (name.empty())
			{
				return;
			}
			const fuzzy::string internal_name = to_ngram_string(name);
			
			add_to_index(internal_name, id);
			data_.resize(id + 1);
			data_[id].name = std::move(internal_name);
			data_[id].meta = meta;
			ready_ = false;
		}

	public:
		database(int ngram_size = 2, bool first_letter_opt = true, uint64_t max_bucket_size = UINT64_MAX)
			: options_(ngram_size, first_letter_opt, max_bucket_size)
		{
		}

		virtual void build()
		{
			remove_overfull_buckets();
			ready_ = true;
		}

		void add(std::string_view name, T meta)
		{
			add(name, std::move(meta), id_counter_++);
		}
		void add(const char *name, T meta)
		{
			add(name, std::move(meta), id_counter_++);
		}

		virtual result_collection<T> fuzzy_search(const std::string& query, size_t truncate = 0)
		{
			if (!ready_)
			{
				build();
			}

			// for an empty query, return an empty result
			if (query.empty())
			{
				return result_collection<T>();
			}

			const fuzzy::string query_internal = to_ngram_string(query);
			const std::vector<ngram_token> query_tokens = ngram_tokens(query_internal, options_.ngram_size);

			std::vector<element_bucket *> element_buckets;
			for (auto token : query_tokens)
			{
				auto bucket = inverted_index_.find(token);
				if (bucket != inverted_index_.end())
				{
					element_buckets.push_back(&bucket->second);
				}
			}

			// build list of word ids that share an ngram
			std::unordered_set<id_type> potential_matches;
			for (element_bucket *element_bucket : element_buckets)
			{
				for (const auto& [length, id_list] : element_bucket->get())
				{
					for (id_type id : id_list)
					{
						potential_matches.insert(id);
					}
				}
			}

			truncate = truncate ? truncate : SIZE_MAX;
			result_collection<T> results;
			for (id_type id : potential_matches)
			{
				// to speed things up, ignore words that dont start with the same letter
				if (options_.first_letter_opt && query_internal[0] != data_[id].name[0])
				{
					continue;
				}
				results.add(&data_[id],	osa_distance(query_internal,
					fuzzy::string_view(data_[id].name.c_str(), std::min(data_[id].name.length(), truncate))));
			}
			return results;
		}
	};

	template <typename T>
	class sorted_database : public database<T>
	{
	protected:

		const struct {
			size_t result_limit;
		} options_;


		void add(std::string_view name, T&& meta, id_type id) override
		{
			assert(!database<T>::ready_ || !"Inserting into a sorted database rebuilds everything, so dont do it.");
			if (name.empty())
			{
				return;
			}
			const fuzzy::string internal_name = internal::to_ngram_string(name);
			database<T>::ready_ = false;
			database<T>::data_.resize(id + 1);
			database<T>::data_[id].name = std::move(internal_name);
			database<T>::data_[id].meta = meta;
		}

		result_collection<T> extract_page(std::pair<typename std::vector<db_entry<T>>::iterator, typename std::vector<db_entry<T>>::iterator> range, size_t page_number, size_t page_size)
		{
			if (page_size == 0)
			{
				page_size = SIZE_MAX;
				page_number = 0;
			}
			page_size = std::min<size_t>(page_size, options_.result_limit);

			result_collection<T> results;
			size_t start_index = page_number * page_size;
			size_t end_index = start_index + page_size;
			if (size_t(range.second - range.first) < start_index)
			{
				return results;
			}
			auto start_iter = range.first + start_index;
			auto end_iter = (size_t(range.second - range.first) < end_index) ? range.second : range.first + end_index;
			for (auto iter = start_iter; iter != end_iter; ++iter)
			{
				results.add(&(*iter), 0);
			}
			return results;
		}

	public:
		using database<T>::add;

		sorted_database(int ngram_size = 2, size_t result_limit = 100, bool first_letter_opt = true, uint64_t max_bucket_size = UINT64_MAX)
			: database<T>(ngram_size, first_letter_opt, max_bucket_size), options_(result_limit)
		{}

		void build() override
		{
			// sort data
			std::sort(
				database<T>::data_.begin(), database<T>::data_.end(),
				[](const db_entry<T> &a, const db_entry<T> &b)
				{ return string_compare(a.name, b.name); });

			// build inverted index
			database<T>::inverted_index_.clear();
			for (size_t id = 0; id < database<T>::data_.size(); id++)
			{
				database<T>::add_to_index(database<T>::data_[id].name, id);
			}

			database<T>::remove_overfull_buckets();

			database<T>::ready_ = true;
		}

		result_collection<T> exact_search(const std::string& query, size_t page_number = 0, size_t page_size = 0)
		{
			if (!database<T>::ready_)
			{
				build();
			}
			const fuzzy::string query_internal = internal::to_ngram_string(query);
			auto range = std::ranges::equal_range(
				database<T>::data_, db_entry<T>{query_internal, T{}},
				[](const db_entry<T> &a, const db_entry<T> &b)
				{ return string_compare(a.name, b.name); });
			return extract_page(range, page_number, page_size);
		}

		result_collection<T> completion_search(const std::string& query, size_t page_number = 0, size_t page_size = 0)
		{
			if (!database<T>::ready_)
			{
				build();
			}
			const fuzzy::string query_internal = internal::to_ngram_string(query);
			auto range = std::ranges::equal_range(
				database<T>::data_, db_entry<T>{query_internal, T{}},
				[truncation_length = query.size()](const db_entry<T> &a, const db_entry<T> &b)
				{
					return string_compare(
						fuzzy::string_view(a.name.data(), std::min(a.name.size(), truncation_length)),
						fuzzy::string_view(b.name.data(), std::min(b.name.size(), truncation_length)));
				});
			return extract_page(range, page_number, page_size);
		}

		using database<T>::fuzzy_search;
	};

}
