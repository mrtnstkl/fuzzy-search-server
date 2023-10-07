#pragma once

#include <chrono>

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
	timer& stop()
	{
		running_ = false;
		stop_ = std::chrono::steady_clock::now();
		return *this;
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