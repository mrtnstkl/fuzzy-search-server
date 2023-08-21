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

	namespace internal
	{

		ngram_token make_token(char c1 = '\0', char c2 = '\0', char c3 = '\0', char c4 = '\0')
		{
			return ngram_token(c1) << 0 | ngram_token(c2) << 8 | ngram_token(c3) << 16 | ngram_token(c4) << 24;
		}

		char to_lower(const char c)
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

		bool case_insensitive_compare(const std::string_view a, const std::string_view b)
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

		bool case_insensitive_equals(const std::string_view a, const std::string_view b)
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

		bool case_insensitive_starts_with(const std::string_view str, const std::string_view sub_str)
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

		int osa_distance(const std::string &s1, const std::string &s2)
		{
			int len_s1 = s1.size();
			int len_s2 = s2.size();

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

	template <typename T>
	struct entry
	{
		std::string name;
		T meta;
	};

	template <typename T>
	struct result
	{
		entry<T> &element;
		const int distance;
		result(entry<T> &element, int distance)
			: element(element), distance(distance)
		{
		}
	};

	template <typename T>
	class results
	{
		std::map<int, std::vector<entry<T> *>> results_;

	public:
		void add(entry<T> &element, int distance)
		{
			results_[distance].push_back(&element);
		}
		bool empty() const
		{
			return results_.empty();
		}
		std::vector<result<T>> best()
		{
			std::vector<result<T>> results;
			auto best_bucket = results_.begin();
			for (auto &res : best_bucket->second)
			{
				results.emplace_back(*res, best_bucket->first);
			}
			return results;
		}

		std::vector<result<T>> extract(uint32_t max_count = UINT32_MAX, int max_distance = INT_MAX)
		{
			std::vector<result<T>> results;
			for (auto &distance_bucket : results_)
			{
				if (distance_bucket.first > max_distance)
				{
					break;
				}
				for (auto &res : distance_bucket.second)
				{
					results.emplace_back(*res, distance_bucket.first);
					if (results.size() >= max_count)
					{
						return results;
					}
				}
			}
			return results;
		}
	};

	template <typename T>
	class database
	{
	protected:
		class ngram_bucket
		{
			static constexpr uint64_t max_bucket_size = UINT64_MAX; // breaks shit

			uint16_t length_limit_ = UINT16_MAX;
			uint64_t elements_ = 0;
			std::map<uint16_t, std::vector<id_type>> data_;

		public:
			void add(id_type id, uint16_t length)
			{
				if (length >= length_limit_)
				{
					return;
				}
				data_[length].push_back(id);
				++elements_;
				if (elements_ > max_bucket_size)
				{
					const auto &longest_elements = *data_.crbegin();
					elements_ -= longest_elements.second.size();
					length_limit_ = longest_elements.first;
					data_.erase(longest_elements.first);
				}
			}
			std::map<uint16_t, std::vector<id_type>> &get()
			{
				return data_;
			}
		};

		std::unordered_map<ngram_token, ngram_bucket> inverted_index_;
		std::vector<entry<T>> data_;

		id_type id_counter_ = 0;

		virtual void add(std::string_view name, T meta, id_type id)
		{
			if (name.empty())
			{
				return;
			}

			for (size_t i = 0; i + 1 < name.length(); i++)
			{
				const auto ngram = make_token(name[i], name[i + 1]);
				inverted_index_[ngram].add(id, name.length());
			}
			data_.resize(id + 1);
			data_[id] = entry<T>{std::string(name), meta};
		}

	public:
		void add(std::string_view name, T meta)
		{
			add(name, meta, id_counter_++);
		}
		void add(const char *name, T meta)
		{
			add(std::string_view(name), meta, id_counter_++);
		}

		virtual results<T> fuzzy_search(std::string query)
		{
			if (query.empty())
				return results<T>();

			std::vector<ngram_bucket *> ngram_buckets;
			for (size_t i = 0; i + 1 < query.length(); i++)
			{
				auto bucket = inverted_index_.find(make_token(query[i], query[i + 1]));
				if (bucket != inverted_index_.end())
				{
					ngram_buckets.push_back(&bucket->second);
				}
			}

			std::unordered_set<id_type> potential_matches;
			for (ngram_bucket *ngram_bucket : ngram_buckets)
			{
				for (const auto &length_bucket : ngram_bucket->get())
				{
					for (id_type id : length_bucket.second)
					{
						potential_matches.insert(id);
					}
				}
			}

			results<T> res;
			for (auto id : potential_matches)
			{
				if (to_lower(query[0]) == to_lower(data_[id].name[0]))
				{
					res.add(data_[id], osa_distance(query, data_[id].name));
				}
			}
			return res;
		}
	};

	template <typename T>
	class sorted_database : public database<T>
	{
	protected:
		bool ready_ = false;

		void add(std::string_view name, T meta, id_type id) override
		{
			assert(!ready_ || !"Inserting into a sorted database rebuilds everything.");
			if (name.empty())
			{
				return;
			}
			ready_ = false;
			database<T>::data_.resize(id + 1);
			database<T>::data_[id] = entry<T>{std::string(name), meta};
		}

	public:
		using database<T>::add;

		void build()
		{
			// sort data
			std::sort(
				database<T>::data_.begin(), database<T>::data_.end(),
				[](const entry<T> &a, const entry<T> &b)
				{ return case_insensitive_compare(a.name, b.name); });

			// build inverted index
			for (size_t id = 0; id < database<T>::data_.size(); id++)
			{
				const auto &entry = database<T>::data_[id];
				for (size_t i = 0; i + 1 < entry.name.length(); i++)
				{
					const auto ngram = make_token(entry.name[i], entry.name[i + 1]);
					database<T>::inverted_index_[ngram].add(id, entry.name.length());
				}
			}
			ready_ = true;
		}

		results<T> exact_search(std::string query)
		{
			if (!ready_)
			{
				build();
			}
			results<T> results;
			auto range = std::ranges::equal_range(
				database<T>::data_, entry<T>{query, T{}},
				[](const entry<T> &a, const entry<T> &b)
				{ return case_insensitive_compare(a.name, b.name); });
			for (auto &element : range)
			{
				results.add(element, 0);
			}
			return results;
		}
		results<T> completion_search(std::string query)
		{
			if (!ready_)
			{
				build();
			}
			results<T> results;
			auto range = std::ranges::equal_range(
				database<T>::data_, entry<T>{query, T{}},
				[truncation_length = query.size()](const entry<T> &a, const entry<T> &b)
				{
					return case_insensitive_compare(
						std::string_view(a.name.c_str(), std::min(a.name.size(), truncation_length)),
						std::string_view(b.name.c_str(), std::min(b.name.size(), truncation_length)));
				});
			for (auto &element : range)
			{
				results.add(element, 0);
			}
			return results;
		}

		results<T> fuzzy_search(std::string query) override
		{
			if (!ready_)
			{
				build();
			}
			return database<T>::fuzzy_search(query);
		}
	};

}
