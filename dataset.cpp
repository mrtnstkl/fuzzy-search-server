#include "dataset.h"

#include <iostream>

#include "util.h"


dataset::dataset(const char* file_path, bool in_memory, std::atomic_bool& abort_flag, std::function<void(element_id, const std::string&)> element_handler)
	: path_(file_path), in_memory_(in_memory), reader_(file_path)
{
	if (!reader_.is_open())
	{
		std::cerr << "could not open dataset \"" << file_path << '"' << std::endl;
		return;
	}
	std::string line;
	element_id line_count = 0;
	uint64_t offset = 0;
	while (std::getline(reader_, line), !line.empty() || !reader_.eof())
	{
		if (abort_flag)
		{
			return;
		}

		try
		{
			element_handler(line_count, line);
		}
		catch(...)
		{
		}

		if (in_memory)
		{
			elements_.push_back(line);
		}
		else
		{
			file_offsets_.push_back(offset);
			offset = reader_.tellg();
		}
		++line_count;
	}
	reader_.clear(std::fstream::eofbit);
	if (reader_.bad() || reader_.fail())
	{
		std::cerr << "file error" << std::endl; 
		return;
	}
	if (in_memory)
	{
		reader_.close();
	}
	ready_ = true;
}

dataset::~dataset()
{
	reader_.close();
}

std::string dataset::get_element(element_id id)
{
	std::string line;
	if (in_memory_)
	{
		line = elements_[id];
	}
	else
	{
		reader_.clear(std::fstream::eofbit);
		reader_.seekg(file_offsets_[id]);
		std::getline(reader_, line);
	}
	return line;
}

bool dataset::ready() const
{
	return ready_;
}
