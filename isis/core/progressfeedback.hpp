/*
    Copyright (C) 2010  reimer@cbs.mpg.de

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#pragma once

#include <stddef.h>
#include <string>
#include <memory>

namespace isis::util
{
/*
 * Basic class for any feedback given from longlasting processes about its progress (e.g. file loading)
 */
class ProgressFeedback // ProgressFeedback should not be copyable - would break the progress counting
{
public:
	ProgressFeedback(const ProgressFeedback &)=delete;
	ProgressFeedback()=default;
	/**
	 * Set the progress display to the given maximum value and "show" it.
	 * This will also extend already displayed progress bars.
	 */
	virtual void show( size_t max, std::string header = "" ) = 0;
	/**
	 * Set the actual "progress".
	 * Behavior is undefined if show was not called before.
	 * \param step increment of the progress (default: 1)
	 * \returns the actual amount of the "progress"
	 */
	virtual size_t progress(size_t step = 1) = 0;
	///Close/undisplay a progress display.
	virtual void close() = 0;
	/// \returns the current valued which represents 100%
	virtual size_t getMax() = 0;
	/// extend the progress bars maximum by the given value
	virtual size_t extend( size_t by ) = 0;
	/// reset the progress bars maximum to the given value and reset the progress to 0
	virtual void restart( size_t new_max ) = 0;
	/// Increment the "progress" by one
	ProgressFeedback &operator++();
};
}
