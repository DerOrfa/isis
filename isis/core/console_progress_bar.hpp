#pragma once
#include "progressfeedback.hpp"
#include <chrono>
#include <atomic>
#include <mutex>

namespace isis::util
{
class ConsoleProgressBar : public ProgressFeedback
{
	size_t max;
	std::atomic<size_t > current=0;
	std::chrono::system_clock::time_point start;
	std::string header,display_header;
	uint16_t prgs=0,avail_prgs_width=80;
	std::mutex mtx;

	void redraw();
	void updateScreenWidth();
	bool closed=true;
public:
	void show( size_t max, std::string header ) override;
	size_t progress(size_t step) override;
	void close() override;
	size_t getMax() override;
	size_t extend(size_t by) override;
	void restart(size_t new_max) override;
};
}
