/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2011  <copyright holder> <email>

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

#include "imageFormat_Vista_sa.hpp"
#include "boost/spirit.hpp"

#include "DataStorage/fileptr.hpp"


namespace isis {

namespace image_io {


ImageFormat_VistaSa::ImageFormat_VistaSa()
{

}

std::string ImageFormat_VistaSa::getName() const { return std::string("Vista standalone"); }
std::string ImageFormat_VistaSa::suffixes(FileFormat::io_modes mode) const { return std::string( ".v" ); }


int ImageFormat_VistaSa::load(std::list< data::Chunk >& chunks, const std::string& filename, const std::string& ) throw( std::runtime_error & )
{
	data::FilePtr mfile( filename );
	if( !mfile.good() ) {
		if( errno ) {
			throwSystemError( errno, filename + " could not be opened" );
			errno = 0;
		} else
			throwGenericError( filename + " could not be opened" );
	}

}




void ImageFormat_VistaSa::write(const data::Image& image, const std::string& filename, const std::string& dialect) throw( std::runtime_error & )
{

}



}

}

isis::image_io::FileFormat *factory()
{
    return new isis::image_io::ImageFormat_VistaSa();
}