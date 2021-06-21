#include "console_progress_bar.hpp"
#include "common.hpp"
#include <unistd.h>
#include <termcap.h>

void isis::util::ConsoleProgressBar::show(size_t _max, std::string _header)
{
	header=_header;
	start = std::chrono::system_clock::now();
	closed=false;
	restart(_max);
}
size_t isis::util::ConsoleProgressBar::progress(size_t step)
{
	if(closed) {
		LOG(Debug, warning) << "Ignoring progress on a closed progress bar";
	} else {
		updateScreenWidth();//@todo doing this all the time is probably not a good idea
		current += step;
		uint16_t new_prgs = current * avail_prgs_width / max;
		if (new_prgs != prgs) {
			prgs = new_prgs;
			redraw();
		}
	}
	return current;
}
void isis::util::ConsoleProgressBar::close()
{
	if(!closed) {
		putc('\n', stdout);
		max = current = 0;
		prgs = 0;
		header.clear();
	}
	closed=true;
}
size_t isis::util::ConsoleProgressBar::getMax()
{
	return max;
}
size_t isis::util::ConsoleProgressBar::extend(size_t by)
{
	max+=by;
	return max;
}
void isis::util::ConsoleProgressBar::restart(size_t new_max)
{
	//reset status
	max=new_max;
	current=0;
	//trigger recalculation and redraw
	progress(0);
}

void isis::util::ConsoleProgressBar::updateScreenWidth()
{
	char termbuf[2048];
	if (tgetent(termbuf, getenv("TERM")) >= 0) {
		avail_prgs_width = tgetnum("co") /* -2 */;
	} else{
		avail_prgs_width = default_screen_width;
	}

	//if display header is to long let it use at most half the witdh
	display_header=header;
	if(display_header.length()+10>avail_prgs_width){
		display_header.erase(avail_prgs_width / 2 - 2);
		display_header+="..";
	}

	avail_prgs_width-=display_header.length();
	avail_prgs_width-= 2 /*and of progress bar */ + 1 /*spaces*/;
}
void isis::util::ConsoleProgressBar::redraw()
{
	if(isatty(fileno(stdout))){ // only draw progress bars into ttys
		printf("\r%s |", display_header.c_str());
//	printf("w:%d/%d\n",prgs,avail_prgs_width);
		for(uint16_t i = 0; i < prgs; i++)
			fputc('=', stdout);
		for(uint16_t i = prgs; i < avail_prgs_width; i++)
			fputc(' ', stdout);
		putc('|', stdout);
		fflush(stdout);
	}
}
