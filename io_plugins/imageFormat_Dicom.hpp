/*
	<one line to give the program's name and a brief idea of what it does.>
	Copyright (C) <year>  <name of author>

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

#ifndef IMAGEFORMAT_DICOM_HPP
#define IMAGEFORMAT_DICOM_HPP

#include <isis/core/io_interface.h>
#include <boost/endian/buffers.hpp>
#include <functional>

namespace isis
{
namespace image_io
{
namespace _internal{
util::istring id2Name( const uint16_t group, const uint16_t element );
util::istring id2Name( const uint32_t id32 );

#ifdef HAVE_OPENJPEG
data::Chunk getj2k(data::ByteArray bytes);
#endif //HAVE_OPENJPEG

template <boost::endian::order Order> struct Tag{
	boost::endian::endian_buffer<Order, int_least16_t, 16> group,element;
	uint32_t getID32()const{
		const uint32_t grp=group.value();
		const uint16_t elm=element.value();
		return (grp<<16)+elm;
	}
};
template <boost::endian::order Order> struct ImplicitVrTag:Tag<Order>{
	boost::endian::endian_buffer<Order, int_least32_t, 32> length;
};
template <boost::endian::order Order> struct ExplicitVrTag:Tag<Order>{
	char vr[2];
	boost::endian::endian_buffer<Order, int_least16_t, 16> length;
};

extern std::map<uint32_t,std::pair<std::string,util::PropertyMap::PropPath>> dicom_dict;

class DicomElement{
	typedef std::function<util::ValueReference(const DicomElement *e)> value_generator;
	const data::ByteArray &source;
	size_t position;
	boost::endian::order endian;
	typedef boost::variant<
	    ExplicitVrTag<boost::endian::order::big> *,
	    ExplicitVrTag<boost::endian::order::little> *,
	    ImplicitVrTag<boost::endian::order::big> *,
	    ImplicitVrTag<boost::endian::order::little> *
	> tag_types;
	tag_types tag;
	struct generator{value_generator scalar,list;uint8_t value_size;};
	static std::map<std::string,generator> generator_map;
	template<boost::endian::order Order> tag_types makeTag(){
		tag_types ret;
		if(implicit_vr){
			ret=(ImplicitVrTag<Order>*)&source[position];
		} else {
			Tag<Order> *probe=(Tag<Order>*)&source[position];
			const auto id=probe->getID32();
			switch(id){
			case 0xfffee000: //sequence start
			case 0xfffee00d: //sequence end
				ret=(ImplicitVrTag<Order>*)&source[position];break;
			default:
				ret=(ExplicitVrTag<Order>*)&source[position];break;
			}
		}
		return ret;
	}
	bool extendedLength()const;
	uint_fast8_t tagLength()const;
public:
	bool implicit_vr=false;
	bool next(size_t position);
	bool next();
	bool endian_swap()const;
	template<typename T> data::ValueArray<T> dataAs()const{
		return dataAs<T>(getLength()/sizeof(T));
	}
	template<typename T> data::ValueArray<T> dataAs(size_t len)const{
		return source.at<T>(position+tagLength(),len,endian_swap());
	}
	const uint8_t *data()const;
	util::istring getIDString()const;
	uint32_t getID32()const;
	size_t getLength()const;
	size_t getPosition()const;
	std::string getVR()const;
	util::PropertyMap::PropPath getName()const;
	DicomElement(const data::ByteArray &_source, size_t _position, boost::endian::order endian,bool _implicit_vr);
	util::ValueReference getValue();
	util::ValueReference getValue(const std::string vr);
	DicomElement next(boost::endian::order endian)const;
};
}

class ImageFormat_Dicom: public FileFormat
{
	static size_t parseCSAEntry( const uint8_t *at, isis::util::PropertyMap &map, std::list<util::istring> dialects );
	static bool parseCSAValue( const std::string &val, const util::PropertyMap::PropPath &name, const util::istring &vr, isis::util::PropertyMap &map );
	static bool parseCSAValueList( const isis::util::slist &val, const util::PropertyMap::PropPath &name, const util::istring &vr, isis::util::PropertyMap &map );
	static data::Chunk readMosaic( data::Chunk source );
protected:
	util::istring suffixes( io_modes modes = both )const override;
public:
	ImageFormat_Dicom();
	static const char dicomTagTreeName[];
	static const char unknownTagName[];
	static void parseCSA( const data::ValueArray<uint8_t> &data, isis::util::PropertyMap &map, std::list<util::istring> dialects );
	static void sanitise( util::PropertyMap &object, std::list<util::istring> dialect );
	std::string getName()const override;
	std::list<util::istring> dialects()const override;

	std::list<data::Chunk> load(std::streambuf *source, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback ) override;
	std::list<data::Chunk> load(const data::ByteArray source, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback ) override;
	void write( const data::Image &image,     const std::string &filename, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> progress )override;
};
}
}

#endif // IMAGEFORMAT_DICOM_HPP
