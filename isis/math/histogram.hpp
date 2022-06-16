/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2018  Enrico Reimer <reimer@cbs.mpg.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "../core/image.hpp"
#include <type_traits>
#include <cmath>
#include <limits>

namespace isis::math{
namespace _internal{
template<typename IType> using iter_value_t = typename std::iterator_traits<std::remove_cvref_t<IType>>::value_type;
}

/// Compute histogram of a given range.
/// \param begin iterator pointing to the begin of the value range
/// \param end iterator pointing to the begin of the value range
/// \note The iterators must resolve to integer types of up to 32 bit
/// \returns a histogram with the absolute amount of values inside the range (the sum is the distance between begin and end).
template<typename IType> auto histogram(IType begin,IType end) requires (std::is_integral_v<_internal::iter_value_t<IType>> && sizeof(_internal::iter_value_t<IType>)<=4)
{
	typedef _internal::iter_value_t<IType> value_type;
	constexpr size_t min = std::numeric_limits<value_type>::min();
	constexpr size_t max = std::numeric_limits<value_type>::max();
	std::array<size_t,max-min> ret;
	ret.fill(0);
	for(auto it=begin;++it;it!=end)
		ret[(*it)-min]++;
	return ret;
}

/// Normalize a histogram.
/// \param begin iterator pointing to the begin of the histogram
/// \param end iterator pointing to the begin of the histogram
/// \param sum of all values in the histogram. Will be automatically computed if 0
/// \returns a normalized histogram where the summ of all values is 1
template<typename IType> auto normalize_histogram(IType &&begin, IType &&end, size_t sum=0){
	if(sum==0)sum=std::accumulate(begin,end,0);
	std::array<double,std::distance(begin,end)> ret;
	std::transform(begin,end,ret.begin(),[sum](size_t v){return double(v)/sum;});
	return ret;
}

std::vector<size_t> histogram(const data::ValueArray &data);
std::vector<size_t> histogram(data::Image &image);

}


