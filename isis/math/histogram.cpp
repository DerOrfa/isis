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

#include "histogram.hpp"
#include "common.hpp"

namespace isis::math{

std::vector<size_t> histogram(const data::ValueArray &data)
{
	auto do_hist = [len=data.getLength()](auto ptr)->std::vector<size_t>{
		typedef decltype(ptr) ptr_type;
		if constexpr(
			std::is_integral_v<typename ptr_type::element_type> &&
			sizeof(typename ptr_type::element_type)<=4
		){
			auto hist = histogram(ptr.get(),ptr.get()+len);
			return {hist.begin(),hist.end()};
		} else {
			LOG(Runtime,error) << "Can only compute histograms of integer datatypes with 32bit or less";
			throw std::domain_error("Unsupported datatype");
		}
	};
	return data.visit(do_hist);
}
std::vector<size_t> histogram(data::Image image)
{
	//make sure we have a consistent type
	image.convertToType(image.getMajorTypeID());
	std::vector<data::Chunk> chks = image.copyChunksToVector(false);
	if(chks[0].isInteger() && chks[0].bytesPerElem()<=4) {
		LOG(Runtime,info) << "Creating histogram of " << chks.size() << " Chunk(s) of " << image.getMajorTypeName() << " image.";
		std::vector<size_t> hist = histogram(chks[0]);//get histogram of first chunk
		for (size_t i = 1; i < chks.size(); ++i) { // if there are more chunks
			auto new_hist = histogram(chks[i]); // get their histograms
			assert(hist.size() == new_hist.size());  // they have the same type, so the histogram is guaranteed to be of the same size
			std::transform(hist.begin(),  hist.end(), new_hist.begin(), hist.begin(), std::plus<>());// and add them to the first histogram
		}
		return hist;
	} else {
		LOG(Runtime,error) << "Can only compute histograms of integer datatypes with 32bit or less";
		throw std::domain_error("Unsupported datatype");
	}
}

}

