#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <atomic>
#include <functional>

class dataset
{
	std::vector<uint64_t> file_offsets_;
	std::vector<std::string> elements_;

	const char* path_;
	const bool in_memory_;

	std::ifstream reader_;
	bool ready_ = false;

public:
	using element_id = uint32_t;

	dataset(const char *file_path, bool in_memory, std::atomic_bool &abort_flag, std::function<void(element_id, const std::string &)> element_handler);
	~dataset();

	dataset(dataset &&) = delete;
	dataset(const dataset &) = delete;
	dataset &operator=(dataset &&) = delete;
	dataset &operator=(const dataset &) = delete;

	std::string get_element(element_id id);
	size_t size() const;

	bool ready() const;
};
