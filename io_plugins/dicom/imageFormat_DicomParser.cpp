#include "imageFormat_Dicom.hpp"
#include "imageFormat_DicomDictionary.hpp"
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/numeric/conversion/converter.hpp>
#include <boost/numeric/conversion/conversion_traits.hpp>
#include <boost/endian/buffers.hpp>
#include <isis/core/common.hpp>


namespace isis::image_io
{

namespace _internal
{
struct tag_length_visitor
{
	template<boost::endian::order Order> size_t operator()(const ExplicitVrTag<Order> *_tag)const{
		return _tag->length.value();
	}
	template<boost::endian::order Order> size_t operator()(const ImplicitVrTag<Order> *_tag)const{
		return _tag->length.value();
	}
};
struct tag_vr_visitor
{
	template<boost::endian::order Order> std::string operator()(const ExplicitVrTag<Order> *_tag)const{
		assert(isalpha(_tag->vr[0]) && isalpha(_tag->vr[1]));
		return std::string(_tag->vr,_tag->vr+2);
	}
	template<boost::endian::order Order> std::string operator()(const ImplicitVrTag<Order> *_tag)const{
		const auto id=_tag->getID32();
		if(id==0x7fe00010)
			return "OW"; //implicit pixel data are OW (http://dicom.nema.org/dicom/2013/output/chtml/part05/chapter_A.html)
		else {
			auto found=query_tag(id);
			if(!found){
				LOG(Runtime,warning) << "Could not find tag " << id2Name(_tag->getID32()) << " in the dictionary and thus don't know its implicit VR";
				return "--";
			} else
				return found->first;
		}
	}
};
struct tag_id_visitor
{
	template<boost::endian::order Order> uint32_t operator()(const Tag<Order> *_tag)const{
		return _tag->getID32();
	}
};
bool DicomElement::extendedLength()const{
	if(implicit_vr)return false;//there is no extendedLength on implicit vr
	const std::string vr=getVR();
	// those vr have an additional 32bit length after a 0x0000 where the length normally is supposed to be
	return vr=="OB" || vr=="OW" || vr== "OF" || vr== "SQ" || vr== "UT" || vr== "UN";
}
uint_fast8_t DicomElement::tagLength()const{ //the actual length of the tag itself
	return extendedLength()?8+4:8;
}
size_t DicomElement::getLength()const{
	if(getID32()==0xFFFEE000){
		return source.at<uint32_t>(position+4,1,endian_swap())[0];
	} else if(extendedLength()){
		assert(std::visit(tag_length_visitor(),tag)==0);
		return source.at<uint32_t>(position+8,1,endian_swap())[0];
	}
	else
		return std::visit(tag_length_visitor(),tag);
}
size_t DicomElement::getPosition()const{
	return position;
}
util::istring DicomElement::getIDString()const{
	return id2Name(getID32());
}

uint32_t DicomElement::getID32()const{
	return std::visit(tag_id_visitor(),tag);
}

std::string DicomElement::getVR()const{
	return std::visit(tag_vr_visitor(),tag);
}
util::PropertyMap::PropPath DicomElement::getName()const{
	auto found=query_tag(getID32());
	if(found)
		return found->second.empty()?
			util::istring( ImageFormat_Dicom::unknownTagName ) + getIDString().c_str():
			found->second;
	else{
		return util::istring( ImageFormat_Dicom::unknownTagName ) + getIDString().c_str();
	}
}

DicomElement::DicomElement(const data::ByteArray &_source, size_t _position, boost::endian::order _endian,bool _implicit_vr):source(_source),position(_position),endian(_endian),implicit_vr(_implicit_vr){
	next(position);//trigger read by calling next without moving
}
bool DicomElement::next(){
	const size_t len=getLength();
	LOG_IF(len==0xffffffffffffffff,Debug,error) << "Doing next on " << getName() << " at " << position << " with an undefined length";
	return next(position+len+tagLength());
}
bool DicomElement::next(size_t _position){
	position=_position;
	if(source.getLength()<_position+8) {
	    is_eof=true;
        return false;
    }
	else {
		switch(endian){
			case boost::endian::order::big:
				tag = makeTag<boost::endian::order::big>();
				break;
			case boost::endian::order::little:
				tag = makeTag<boost::endian::order::little>();
				break;
		}
		return true;
	}
}


const uint8_t *DicomElement::data()const{
	return &source[position+2+2+2+2]; //offset by group-id, element-id, vr and length
}
std::optional<util::Value> DicomElement::getValue(){
	return getValue(getVR());
}
std::optional<util::Value> DicomElement::getValue(std::string vr){
	std::optional<util::Value> ret;
	auto found_generator=generator_map.find(vr);
	if(found_generator!=generator_map.end()){
		auto generator=found_generator->second;;
		size_t mult=generator.value_size?getLength()/generator.value_size:1;

		if(mult==1)
			ret=generator.scalar(this);
		else if(generator.list)
			ret=generator.list(this);
		else { // fallback for non- supported lists @todo
			assert(false);
		}

		LOG_IF(ret,Debug,verbose_info) << "Parsed " << vr << "-tag " << getName() << " "  << getIDString() << " at position " << position << " as "  << *ret;
		LOG_IF(!ret,Runtime,verbose_info) << "Failed to parse " << vr << "-tag " << getName() << " "  << getIDString() << " at position " << position;
	} else {
		LOG_IF(vr.empty(),Debug,error) << "Could not find an interpreter for the VR " << vr << " of " << getName() << "/" << getIDString() << " at " << position ;
		LOG_IF(!vr.empty(),Debug,error) << "Ignoring entry " << getName() << "/" << getIDString() << " at " << position << " because of invalid VR";
	}

	return ret;
}
DicomElement DicomElement::next(boost::endian::order endian)const{
	//@todo handle end of stream
	size_t nextpos=position+2+2+2+2+getLength();
	return DicomElement(source,nextpos,endian,implicit_vr);
}
bool DicomElement::endian_swap()const{
	switch(endian){
	    case boost::endian::order::big:   return (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__);
	    case boost::endian::order::little:return (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);
	}
	return false;//should never be reached
}


template <typename S, typename V> void arrayToVecPropImp( S *array, util::PropertyMap &dest, const util::PropertyMap::PropPath &name, size_t len )
{
	V vector;
	vector.copyFrom( array, array + len );
	dest.touchProperty(name) = vector; //if Float32 is float its fine, if not we will get an linker error here
}
template <typename S> void arrayToVecProp( S *array, util::PropertyMap &dest, const util::PropertyMap::PropPath &name, size_t len )
{
	if( len <= 3 )arrayToVecPropImp<S, util::vector3<S> >( array, dest, name, len );
	else arrayToVecPropImp<S, util::vector4<S> >( array, dest, name, len );
}

template<typename T> bool noLatin( const T &t ) {return t >= 127;}
}

void ImageFormat_Dicom::parseCSA(const data::ByteArray &data, util::PropertyMap &map, std::list<util::istring> dialects )
{
	const size_t len = data.getLength();

	for ( std::string::size_type pos = 0x10; pos < ( len - sizeof( int32_t ) ); ) {
		pos += parseCSAEntry( &data[pos], len - sizeof( int32_t ), map, dialects );
	}
}
size_t ImageFormat_Dicom::parseCSAEntry(const uint8_t *at, size_t data_len, util::PropertyMap &map, std::list<util::istring> dialects )
{
	size_t pos = 0;
	const char *const name = ( char * )at + pos;
	pos += 0x40;
	if(name[0]==0)
		throw std::logic_error("empty CSA entry name");
	/*int32_t &vm=*((int32_t*)array+pos);*/
	pos += sizeof( int32_t );
	const char *const vr = ( char * )at + pos;
	pos += 0x4;
	/*int32_t syngodt=endian<Uint8,Uint32>(array+pos);*/
	pos += sizeof( int32_t );
	const int32_t nitems = ((boost::endian::little_int32_buf_t*)( at + pos ))->value();
	pos += sizeof( int32_t );
	static const std::string whitespaces( " \t\f\v\n\r" );

	if ( nitems ) {
		pos += sizeof( int32_t ); //77
		util::slist ret;

		for ( unsigned short n = 0; n < nitems; n++ ) {
			int32_t len = ((boost::endian::little_int32_buf_t*)( at + pos ))->value();
			pos += sizeof( int32_t );//the length of this element
			pos += 3 * sizeof( int32_t ); //whatever

			if ( !len )continue;
			if( (
			        std::string( "MrPhoenixProtocol" ) != name  && std::string( "MrEvaProtocol" ) != name && std::string( "MrProtocol" ) != name
			    ) || checkDialect(dialects, "withExtProtocols") ) {
				const std::string insert( ( char * )at + pos );
				const std::string::size_type start = insert.find_first_not_of( whitespaces );

				if ( insert.empty() || start == std::string::npos ) {
					LOG( Runtime, verbose_info ) << "Skipping empty string for CSA entry " << name;
				} else {
					const std::string::size_type end = insert.find_last_not_of( whitespaces ); //strip spaces

					if( end == std::string::npos )
						ret.push_back( insert.substr( start, insert.size() - start ) ); //store the text if there is some
					else
						ret.push_back( insert.substr( start, end + 1 - start ) );//store the text if there is some
				}
			} else {
				LOG( Runtime, verbose_info ) << "Skipping " << name << " as its not requested by the dialect (use dialect \"withExtProtocols\" to get it)";
			}

			pos += (
			           ( len + sizeof( int32_t ) - 1 ) / sizeof( int32_t )
			       ) *
			       sizeof( int32_t );//increment pos by len aligned to sizeof(int32_t)*/
			if(pos >= data_len)
				throwGenericError(std::string("CSA Parser went beyond the element length ") + std::to_string(pos) + ">=" + std::to_string(data_len));
		}

		try {
			util::PropertyMap::PropPath path;
			path.push_back( name );

			if ( ret.size() == 1 ) {
				if ( parseCSAValue( ret.front(), path , vr, map ) ) {
					LOG( Debug, verbose_info ) << "Found scalar entry " << path << ":" << map.property( path ) << " in CSA header";
				}
			} else if ( ret.size() > 1 ) {
				if ( parseCSAValueList( ret, path, vr, map ) ) {
					LOG( Debug, verbose_info ) << "Found list entry " << path << ":" << map.property( path ) << " in CSA header";
				}
			}
		} catch ( std::exception &e ) {
			LOG( Runtime, warning ) << "Failed to parse CSA entry " << std::make_pair( name, ret ) << " as " << vr << " (" << e.what() << ")";
		}
	} else {
		LOG( Debug, verbose_info ) << "Skipping empty CSA entry " << name;
		pos += sizeof( int32_t );
	}

	return pos;
}

bool ImageFormat_Dicom::parseCSAValue( const std::string &val, const util::PropertyMap::PropPath &name, const util::istring &vr, util::PropertyMap &map )
{
	if ( vr == "IS" or vr == "SL" ) {
		map.setValueAs( name, std::stoi( val ));
	} else if ( vr == "UL" ) {
		util::stringTo( val, map.refValueAsOr( name, uint32_t()) );
	} else if ( vr == "CS" or vr == "LO" or vr == "SH" or vr == "UN" or vr == "ST" or vr == "UT" or vr == "LT") {
		map.setValueAs( name, val );
	} else if ( vr == "DS" or vr == "FD" ) {
		map.setValueAs( name, std::stod( val ));
	} else if ( vr == "US" ) {
		util::stringTo( val, map.refValueAsOr( name, uint16_t()) );
	} else if ( vr == "SS" ) {
		util::stringTo( val, map.refValueAsOr( name, int16_t()) );
	} else {
		LOG( Runtime, error ) << "Don't know how to parse CSA entry " << std::make_pair( name, val ) << " type is " << util::MSubject( vr );
		return false;
	}

	return true;
}
bool ImageFormat_Dicom::parseCSAValueList( const util::slist &val, const util::PropertyMap::PropPath &name, const util::istring &vr, util::PropertyMap &map )
{
	if ( vr == "IS" or vr == "SL" or vr == "US" or vr == "SS" ) {
		auto &target=map.refValueAsOr(name,util::ilist());
		for(const std::string &ref:val)
			util::stringTo(ref,target.emplace_back());
	} else if ( vr == "UL" ) {
		map.setValueAs( name, val); // @todo we dont have an unsigned int list
	} else if ( vr == "CS" or vr == "LO" or vr == "SH" or vr == "UN" or vr == "ST" or vr == "SL" ) {
		map.setValueAs( name, val );
	} else if ( vr == "DS" or vr == "FD" ) {
		auto &target=map.refValueAsOr(name,util::dlist());
		for(const std::string &ref:val)
			util::stringTo(ref,target.emplace_back());
	} else if ( vr == "CS" ) {
		map.setValueAs( name, val );
	} else {
		LOG( Runtime, error ) << "Don't know how to parse CSA entry list " << std::make_pair( name, val ) << " type is " << util::MSubject( vr );
		return false;
	}

	return true;
}

namespace _internal{
    template<typename T, typename LT> util::Value list_generate(const DicomElement *e){
	    size_t mult=e->getLength()/sizeof(T);
		assert(float(mult)*sizeof(T) == e->getLength());
		auto wrap=e->dataAs<T>(mult);
		return std::list<LT>(wrap.begin(),wrap.end());
    }
    template<typename T> util::Value scalar_generate(const DicomElement *e){
	    assert(e->getLength()==sizeof(T));
		T *v=(T*)e->data();
		return e->endian_swap() ? data::endianSwap(*v):*v;
    }
	util::Value tag_generate(const DicomElement *e){
		assert(e->getLength()==4);
		auto numbers=list_generate<uint16_t, int32_t>(e).as<util::ilist>();
		std::ostringstream buffer;
		buffer << std::hex << "(" << numbers.front() << "," << numbers.back() << ")";
		return buffer.str();
	}
    std::optional<util::Value> string_generate(const DicomElement *e){
		//@todo http://dicom.nema.org/Dicom/2013/output/chtml/part05/sect_6.2.html#note_6.1-2-1
		if(e->getLength()){
			const uint8_t *start=e->data();
			const uint8_t *end=start+e->getLength()-1;
			while(end>=start && (*end==' '|| *end==0)) //cut of trailing spaces and zeros
				--end;
			std::string ret_s(start,end+1);
			const std::string VR = e->getVR();
			if(VR == "LT" or VR == "ST" or VR == "UT") { //"LT", "ST" or "UT" are allowed to contain "\", hence not separator
				return ret_s;
			} else {
				util::slist ret_list=util::stringToList<std::string>(ret_s,'\\');

				if(ret_list.size()>1)
					return ret_list;
				else
					return ret_s;
			}
		}
		return std::optional<util::Value>();
	}
	std::optional<util::Value> bytes_as_strings(const DicomElement *e){
		//@todo http://dicom.nema.org/Dicom/2013/output/chtml/part05/sect_6.2.html#note_6.1-2-1
		if(e->getLength()){
			const uint8_t *pos=e->data();
			const uint8_t *end=pos+e->getLength();
			util::slist ret_list;

			for(;pos<end;pos++){
				std::stringstream stream;
				stream << std::setfill ('0') << std::setw(2) << std::hex << *pos;
				ret_list.push_back(stream.str());
			}


			if(ret_list.size()>1)
				return ret_list;
			else
				return ret_list.front();
		}
		return std::optional<util::Value>();
	}
	std::optional<util::Value> parse_AS(const _internal::DicomElement *e){
		std::optional<util::Value> ret;
		uint16_t duration = 0;
		auto as=string_generate(e);
		if(!as)
			return {}; //gracefully abort if reading the tag failed

		assert(as->typeID()==util::typeID<std::string>());//AS must always be one string
		std::string buff=std::get<std::string>(*as);

		static boost::numeric::converter <
		uint16_t, double,
		        boost::numeric::conversion_traits<uint16_t, double>,
		        boost::numeric::def_overflow_handler,
		        boost::numeric::RoundEven<double>
		        > double2uint16;

		if ( util::stringTo( buff.substr( 0, buff.find_last_of( "0123456789" ) + 1 ), duration ) ) {
			switch ( buff.at( buff.size() - 1 ) ) {
			case 'D':
			case 'd':
				break;
			case 'W':
			case 'w':
				duration *= 7;
				break;
			case 'M':
			case 'm':
				duration = double2uint16( 30.436875 * duration ); // year/12
				break;
			case 'Y':
			case 'y':
				duration = double2uint16( 365.2425 * duration ); //mean length of a year
				break;
			default:
				LOG( Runtime, warning )
				        << "Missing age-type-letter, assuming days";
			}
			LOG( Debug, verbose_info )
			        << "Parsed age for " << e->getName() << "(" <<  buff << ")" << " as " << duration << " days";
			ret=duration;
		} else
			LOG( Runtime, warning )
			        << "Cannot parse age string \"" << buff << "\" in the field \"" << e->getName() << "\"";
		return ret;
	}

	std::map<std::string,DicomElement::generator> DicomElement::generator_map={
	    //"trivial" conversions
	    {"FL",{scalar_generate<float>,   list_generate<float,   double>, sizeof(float )}},
	    {"FD",{scalar_generate<double>,  list_generate<double,  double>, sizeof(double)}},
	    {"SS",{scalar_generate<int16_t>, list_generate<int16_t, int32_t>,sizeof(int16_t)}},
	    {"SL",{scalar_generate<int32_t>, list_generate<int32_t, int32_t>,sizeof(int32_t)}},
	    {"US",{scalar_generate<uint16_t>,list_generate<uint16_t,int32_t>,sizeof(uint16_t)}},
	    {"UL",{scalar_generate<uint32_t>,list_generate<uint32_t,int32_t>,sizeof(uint32_t)}}, //@todo deal with potential overflow
	    //"normal" string types
	    {"LT",{string_generate,nullptr,0}},
	    {"UT",{string_generate,nullptr,0}},
	    {"LO",{string_generate,nullptr,0}},
	    {"UI",{string_generate,nullptr,0}},
	    {"ST",{string_generate,nullptr,0}},
	    {"SH",{string_generate,nullptr,0}},
	    {"CS",{string_generate,nullptr,0}},
	    {"PN",{string_generate,nullptr,0}},
	    {"AE",{string_generate,nullptr,0}},
	    //number strings (keep them as string, type-system will take care of the conversion if neccesary)
	    {"IS",{string_generate,nullptr,0}},
	    {"DS",{string_generate,nullptr,0}},
	    //time strings
	    {"DA",{
			[](const _internal::DicomElement *e){auto prop=string_generate(e); return prop?std::make_optional(prop->copyByID(util::typeID<util::date>()))     :prop;},
			string_generate,
			8}
		},
	    {"TM",{[](const _internal::DicomElement *e){auto prop=string_generate(e); return prop?std::make_optional(prop->copyByID(util::typeID<util::timestamp>())):prop;},nullptr,0}},
	    {"DT",{[](const _internal::DicomElement *e){auto prop=string_generate(e); return prop?std::make_optional(prop->copyByID(util::typeID<util::timestamp>())):prop;},nullptr,0}},
	    {"AS",{parse_AS,nullptr,0}},
		{"AT",{tag_generate, nullptr,2*sizeof(uint16_t)}},
	    //fallback for unknown
	    {"--",{bytes_as_strings,nullptr,0}}
	};
}
}

