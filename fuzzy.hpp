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
	using id_type = uint32_t;

	constexpr size_t bigram_limit = 6;
	constexpr size_t trigram_limit = 12;

	namespace internal
	{
		inline ngram_token make_token(char c1 = '\0', char c2 = '\0', char c3 = '\0', char c4 = '\0')
		{
			return ngram_token(c1) << 0 | ngram_token(c2) << 8 | ngram_token(c3) << 16 | ngram_token(c4) << 24;
		}

		inline char to_lower(const char c)
		{
			switch (c)
			{
			case 'A':
				return 'a';
			case 'B':
				return 'b';
			case 'C':
				return 'c';
			case 'D':
				return 'd';
			case 'E':
				return 'e';
			case 'F':
				return 'f';
			case 'G':
				return 'g';
			case 'H':
				return 'h';
			case 'I':
				return 'i';
			case 'J':
				return 'j';
			case 'K':
				return 'k';
			case 'L':
				return 'l';
			case 'M':
				return 'm';
			case 'N':
				return 'n';
			case 'O':
				return 'o';
			case 'P':
				return 'p';
			case 'Q':
				return 'q';
			case 'R':
				return 'r';
			case 'S':
				return 's';
			case 'T':
				return 't';
			case 'U':
				return 'u';
			case 'V':
				return 'v';
			case 'W':
				return 'w';
			case 'X':
				return 'x';
			case 'Y':
				return 'y';
			case 'Z':
				return 'z';
			default:
				return c;
			}
		}

		inline bool case_insensitive_compare(const std::string_view a, const std::string_view b)
		{
			for (uint16_t i = 0; i < std::min(a.size(), b.size()); ++i)
			{
				const char ac = to_lower(a[i]);
				const char bc = to_lower(b[i]);
				if (ac < bc)
					return true;
				if (ac > bc)
					return false;
			}
			return a.size() < b.size();
		}

		inline bool case_insensitive_equals(const std::string_view a, const std::string_view b)
		{
			if (a.size() != b.size())
				return false;
			for (uint16_t i = 0; i < a.size(); ++i)
			{
				if (to_lower(a[i]) != to_lower(b[i]))
					return false;
			}
			return true;
		}

		inline bool case_insensitive_starts_with(const std::string_view str, const std::string_view sub_str)
		{
			if (str.size() < sub_str.size())
				return false;
			for (uint16_t i = 0; i < sub_str.size(); ++i)
			{
				if (to_lower(str[i]) != to_lower(sub_str[i]))
					return false;
			}
			return true;
		}

		inline int osa_distance(std::string_view s1, std::string_view s2)
		{
			const int len_s1 = s1.size();
			const int len_s2 = s2.size();

			std::vector<std::vector<int>> dp(len_s1 + 1, std::vector<int>(len_s2 + 1));

			for (int i = 0; i <= len_s1; i++)
			{
				dp[i][0] = i;
			}
			for (int j = 0; j <= len_s2; j++)
			{
				dp[0][j] = j;
			}

			for (int i = 1; i <= len_s1; i++)
			{
				for (int j = 1; j <= len_s2; j++)
				{
					int cost = (to_lower(s1[i - 1]) == to_lower(s2[j - 1])) ? 0 : 1;
					dp[i][j] = std::min({
						dp[i - 1][j] + 1,		// deletion
						dp[i][j - 1] + 1,		// insertion
						dp[i - 1][j - 1] + cost // substitution
					});

					if (i > 1 && j > 1 && to_lower(s1[i - 1]) == to_lower(s2[j - 2]) && to_lower(s1[i - 2]) == to_lower(s2[j - 1]))
					{
						dp[i][j] = std::min(dp[i][j], dp[i - 2][j - 2] + 1); // transposition
					}
				}
			}
			return dp[len_s1][len_s2];
		}

	}

	using namespace internal;

	// stores a name, and meta info of type T
	template <typename T>
	struct db_entry
	{
		std::string name;
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

	// a structured container for search results
	// results are sorted by their distance, allowing for retrieval of best matches
	template <typename T>
	class result_collection
	{
		std::map<int, std::vector<result<T>>> results_;
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

		std::vector<result<T>> best()
		{
			return empty() ? std::vector<result<T>>{} : results_.begin()->second;
		}

		std::vector<result<T>> extract(uint32_t max_count = UINT32_MAX, int max_distance = INT_MAX)
		{
			std::vector<result<T>> extracted_results;
			for (const auto &[result_distance, results_at_that_distance] : results_)
			{
				if (result_distance > max_distance)
				{
					break;
				}
				for (result<T> result : results_at_that_distance)
				{
					extracted_results.push_back(result);
					if (extracted_results.size() >= max_count)
					{
						return extracted_results;
					}
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
		};

		// maps ngram tokens to element buckets
		std::unordered_map<ngram_token, element_bucket> inverted_index_;
		// all the database entries
		std::vector<db_entry<T>> data_;

		id_type id_counter_ = 0;

		const struct {
			const int ngram_size;
			bool first_letter_opt;
		} options_;

		void add_to_index(const std::string& name, id_type id)
		{
			switch (options_.ngram_size)
			{
			case 2:
				// bigrams
				for (size_t i = 0; i + 1 < name.length(); i++)
					inverted_index_[make_token(name[i], name[i + 1])].add(id, name.length());
				break;
			case 3:
				// trigrams
				for (size_t i = 0; i + 2 < name.length(); i++)
					inverted_index_[make_token(name[i], name[i + 1], name[i + 2])].add(id, name.length());
				// for short words, also do bigrams
				if (name.length() <= bigram_limit)
				{
					for (size_t i = 0; i + 1 < name.length(); i++)
						inverted_index_[make_token(name[i], name[i + 1])].add(id, name.length());
				}
				break;
			case 4:
				// tetragrams
				for (size_t i = 0; i + 3 < name.length(); i++)
					inverted_index_[make_token(name[i], name[i + 1], name[i + 2], name[i + 3])].add(id, name.length());
				// for short words, also do trigrams
				if (name.length() <= trigram_limit)
				{
					for (size_t i = 0; i + 2 < name.length(); i++)
						inverted_index_[make_token(name[i], name[i + 1], name[i + 2])].add(id, name.length());
				}
				// ...and bigrams
				if (name.length() <= bigram_limit)
				{
					for (size_t i = 0; i + 1 < name.length(); i++)
						inverted_index_[make_token(name[i], name[i + 1])].add(id, name.length());
				}
				break;
			default:
				abort();
			}
		}

		virtual void add(std::string name, T&& meta, id_type id)
		{
			if (name.empty())
			{
				return;
			}
			add_to_index(name, id);
			data_.resize(id + 1);
			data_[id].name = std::move(name);
			data_[id].meta = meta;
		}

	public:
		database(int ngram_size = 2, bool first_letter_opt = true)
			: options_(ngram_size, first_letter_opt)
		{
		}

		void add(std::string_view name, T meta)
		{
			add(std::string(name), std::move(meta), id_counter_++);
		}
		void add(const char *name, T meta)
		{
			add(std::string(name), std::move(meta), id_counter_++);
		}

		virtual result_collection<T> fuzzy_search(const std::string& query, size_t truncate = 0)
		{
			// for an empty query, return an empty result
			if (query.empty())
				return result_collection<T>();

			std::vector<ngram_token> query_tokens;
			switch (options_.ngram_size)
			{
			case 2:
				// bigrams
				for (size_t i = 0; i + 1 < query.length(); i++)
					query_tokens.push_back(make_token(query[i], query[i + 1]));
				break;
			case 3:
				// trigrams
				for (size_t i = 0; i + 2 < query.length(); i++)
					query_tokens.push_back(make_token(query[i], query[i + 1], query[i + 2]));
				// for short words, also do bigrams
				if (query.length() <= bigram_limit)
				{
					for (size_t i = 0; i + 1 < query.length(); i++)
						query_tokens.push_back(make_token(query[i], query[i + 1]));
				}
				break;
			case 4:
				// tetragrams
				for (size_t i = 0; i + 3 < query.length(); i++)
					query_tokens.push_back(make_token(query[i], query[i + 1], query[i + 2], query[i + 3]));
				// for short words, also do trigrams
				if (query.length() <= trigram_limit)
				{
					for (size_t i = 0; i + 2 < query.length(); i++)
						query_tokens.push_back(make_token(query[i], query[i + 1], query[i + 2]));
				}
				// ...and bigrams
				if (query.length() <= bigram_limit)
				{
					for (size_t i = 0; i + 1 < query.length(); i++)
						query_tokens.push_back(make_token(query[i], query[i + 1]));
				}
				break;
			default:
				abort();
			}

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
				if (options_.first_letter_opt && to_lower(query[0]) != to_lower(data_[id].name[0]))
				{
					continue;
				}
				results.add(&data_[id],	osa_distance(query, std::string_view(data_[id].name.c_str(), std::min(data_[id].name.length(), truncate))));
			}
			return results;
		}
	};

	template <typename T>
	class sorted_database : public database<T>
	{
	protected:
		bool ready_ = false;

		const struct {
			size_t result_limit;
		} options_;


		void add(std::string name, T&& meta, id_type id) override
		{
			assert(!ready_ || !"Inserting into a sorted database rebuilds everything, so dont do it.");
			if (name.empty())
			{
				return;
			}
			ready_ = false;
			database<T>::data_.resize(id + 1);
			database<T>::data_[id].name = std::move(name);
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

		sorted_database(int ngram_size = 2, size_t result_limit = 100, bool first_letter_opt = true)
			: database<T>(ngram_size, first_letter_opt), options_(result_limit)
		{}

		void build()
		{
			// sort data
			std::sort(
				database<T>::data_.begin(), database<T>::data_.end(),
				[](const db_entry<T> &a, const db_entry<T> &b)
				{ return case_insensitive_compare(a.name, b.name); });

			// build inverted index
			database<T>::inverted_index_.clear();
			for (size_t id = 0; id < database<T>::data_.size(); id++)
			{
				database<T>::add_to_index(database<T>::data_[id].name, id);
			}
			ready_ = true;
		}

		result_collection<T> exact_search(const std::string& query, size_t page_number = 0, size_t page_size = 0)
		{
			if (!ready_)
			{
				build();
			}
			auto range = std::ranges::equal_range(
				database<T>::data_, db_entry<T>{query, T{}},
				[](const db_entry<T> &a, const db_entry<T> &b)
				{ return case_insensitive_compare(a.name, b.name); });
			return extract_page(range, page_number, page_size);
		}

		result_collection<T> completion_search(const std::string& query, size_t page_number = 0, size_t page_size = 0)
		{
			if (!ready_)
			{
				build();
			}
			result_collection<T> results;
			auto range = std::ranges::equal_range(
				database<T>::data_, db_entry<T>{query, T{}},
				[truncation_length = query.size()](const db_entry<T> &a, const db_entry<T> &b)
				{
					return case_insensitive_compare(
						std::string_view(a.name.c_str(), std::min(a.name.size(), truncation_length)),
						std::string_view(b.name.c_str(), std::min(b.name.size(), truncation_length)));
				});
			return extract_page(range, page_number, page_size);
		}

		result_collection<T> fuzzy_search(const std::string& query, size_t truncate = 0) override
		{
			if (!ready_)
			{
				build();
			}
			return database<T>::fuzzy_search(query, truncate);
		}
	};

}
