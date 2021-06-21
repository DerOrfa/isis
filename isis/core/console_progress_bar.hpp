#pragma once
#include <isis/core/progressfeedback.hpp>
#include <chrono>

namespace isis::util
{
class ConsoleProgressBar : public ProgressFeedback
{
	size_t max,current=0;
	std::chrono::system_clock::time_point start;
	std::string header,display_header;
	static constexpr uint16_t default_screen_width = 80;
	uint16_t prgs=0,avail_prgs_width;

	void redraw();
	void updateScreenWidth();
	bool closed=true;
public:
	void show( size_t max, std::string header ) override;
	size_t progress(const std::string &message, size_t step) override;
	void close() override;
	size_t getMax() override;
	size_t extend(size_t by) override;
	void restart(size_t new_max) override;
};
}
