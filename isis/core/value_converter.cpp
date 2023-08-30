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

#ifdef _MSC_VER
#pragma warning(disable:4800 4996)
#endif

#include "value_converter.hpp"
#include "value.hpp"
#include "stringop.hpp"
#include <type_traits>

#include <complex>
#include <iomanip>
#include <boost/numeric/conversion/converter.hpp>

/// @cond _internal
namespace isis::util
{
API_EXCLUDE_BEGIN;
namespace _internal
{

template<typename SRC,typename DST> constexpr bool is_num(){return std::is_arithmetic_v<SRC> && std::is_arithmetic_v<DST>;}

template<typename T_DUR> bool parseTimeString(const std::string &src, const char *format,std::chrono::time_point<std::chrono::system_clock,T_DUR> &dst){
	std::tm t = {0,0,0,1,0,70,0,0,-1,0,nullptr};
	std::istringstream ss(src);
	ss >> std::get_time(&t, format);
	if(!ss.fail()) {
		time_t tt = mktime(&t);
		dst=std::chrono::round<T_DUR>(std::chrono::system_clock::from_time_t(tt));
		if(ss.peek()=='.'){
			float frac=0;
			ss >> frac;
			dst+=std::chrono::duration_cast<T_DUR>(std::chrono::duration<float>(frac));
		}
		LOG_IF(!ss.eof(),Debug,info) << ss.str() << "remained after parsing timestamp" << src;
		return true;
	} else 
		return false;
}
	
//Define generator - this can be global because it's using convert internally
template<typename SRC, typename DST> class ValueGenerator: public ValueConverterBase
{
public:
	[[nodiscard]] Value create()const override {
		return DST();
	}
	range_check_result generate(const Value &src, Value& dst )const override {
		dst=create();
		assert(dst.is<DST>());
		const range_check_result result = convert( src, dst );
		return result;
	}
};

/////////////////////////////////////////////////////////////////////////////
// general converter version -- does nothing
/////////////////////////////////////////////////////////////////////////////
template<bool NUMERIC, bool SAME, typename SRC, typename DST> class ValueConverter : public ValueGenerator<SRC, DST>
{
public:
	static std::shared_ptr<const ValueConverterBase> get() {
//uncomment this to see which conversions are not generated - be careful, that's that's a LOT
// 		std::clog
// 			<< "There will be no " << (SAME?"copy":NUMERIC?"numeric":"non-numeric") <<  " conversion for " 
// 			<< typeName<SRC>() << " to " <<typeName<DST>() << std::endl;
		return {};
	}
};
/////////////////////////////////////////////////////////////////////////////
// trivial version -- for conversion of the same type
/////////////////////////////////////////////////////////////////////////////
template<bool NUMERIC, typename SRC, typename DST> class ValueConverter<NUMERIC, true, SRC, DST> : public ValueGenerator<SRC, DST>
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating trivial copy converter for " <<typeName<SRC>();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<NUMERIC, true, SRC, DST>;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		dst=src;
		return cInRange;
	}
};

/////////////////////////////////////////////////////////////////////////////
// some helper
/////////////////////////////////////////////////////////////////////////////

// global numeric overflow handler @todo this is NOT thread-safe
struct NumericOverflowHandler {
	static range_check_result result;
	void operator() ( boost::numeric::range_check_result r ) { // throw bad_numeric_conversion derived
		result = (range_check_result)r;
	}
};
range_check_result NumericOverflowHandler::result = cInRange;

// basic numeric to numeric conversion (does rounding and handles overflow)
template<typename SRC, typename DST> range_check_result num2num( const SRC &src, DST &dst )
{
	typedef boost::numeric::converter <
	DST, SRC,
	   boost::numeric::conversion_traits<DST, SRC>,
	   NumericOverflowHandler,
	   boost::numeric::RoundEven<SRC>
	   > converter;
	NumericOverflowHandler::result = cInRange;
	dst = converter::convert( src );
	return NumericOverflowHandler::result;
}

template<typename DST> range_check_result str2scalar( const std::string &src, DST &dst )
{
	if constexpr(std::is_same_v<DST,std::string>){ //trivial copy
		dst = src;
		return cInRange;
	} else if constexpr(std::is_same_v<DST,Selection>){
		if ( dst.set( src.c_str() ) )
			return cInRange;
		else //if the string is not "part" of the selection we count this as positive overflow
			return cPosOverflow;
	} else if constexpr(std::is_arithmetic_v<DST>){// if a converter from double is available first map to double and then convert that into DST
		return num2num<double, DST>( std::stod( src ), dst ); //@todo so we need an overflow-check here
	} else {// otherwise, try direct mapping (rounding will fail)

		LOG( Debug, warning ) << "using stringTo to convert from string to " << typeName<DST>() << " no rounding can be done.";
		if(!stringTo(src,dst)) {
			dst = DST();
			LOG( Runtime, error ) << "Miserably failed to interpret " << MSubject( src ) << " as " <<typeName<DST>() << " returning " << MSubject( DST() );
		}
		return cInRange;
	}
}
// needs special handling
template<> range_check_result str2scalar<date>( const std::string &src, date &dst )
{
	// see http://dicom.nema.org/dicom/2013/output/chtml/part05/sect_6.2.html for dicom VRs
	//@todo support other formats and parse offset part of the DICOM VR "DT"
	//@todo %c and %x are broken in gcc 
	static const char *formats[]={
		"%Y%m%d",
		"%Y-%m-%d",
		"%y%m%d",
		"%d.%m.%Y"
	};

	for(const char *format:formats){
		if(parseTimeString(src,format,dst))
			return cInRange;
	};

	dst=date();
	
	LOG(Runtime, error ) // if it's still broken at least tell the user
		<< "Miserably failed to interpret " << MSubject( src ) << " as " <<typeName<date>() << " returning " << MSubject( dst );
	return cInRange;
}
// needs special handling
template<> range_check_result str2scalar<timestamp>( const std::string &src, timestamp &dst )
{
	// see http://dicom.nema.org/dicom/2013/output/chtml/part05/sect_6.2.html for dicom VRs
	//@todo support other formats and parse offset part of the DICOM VR "DT"
	//@todo %c and %x are broken in gcc 
	static const char *formats[]={"%Y%m%d%H%M%S","%Y%m%dT%H%M%S","%d.%m.%Y %H:%M:%S","%H%M%S","%H:%M:%S"};

	for(const char *format:formats){
		if(parseTimeString(src,format,dst))
			return cInRange;
	};

	dst=timestamp();
	
	LOG(Runtime, error ) // if it's still broken at least tell the user
		<< "Miserably failed to interpret " << MSubject( src ) << " as " <<typeName<timestamp>() << " returning " << MSubject( dst );
	return cInRange;
}
// needs special handling
template<> range_check_result str2scalar<duration>(const std::string &src, duration &dst)
{
	// try to parse time format
	static const char *formats[]={"%H:%M:%S"};
	timestamp dummy;

	for(const char *format:formats){
		if(parseTimeString(src,format,dummy)){
			dst=dummy-timestamp();
			return cInRange;
		}
	};
	
	// ok just use whatever number we can manage to parse
	//@todo support other ratios as milli
	duration::rep count;
	const range_check_result result=str2scalar(src,count);
	dst=duration(count);
	return result;
}
// this as well (interpret everything like true/false yes/no y/n)
template<> range_check_result str2scalar<bool>( const std::string &src, bool &dst )
{
	const std::basic_string_view<char, _internal::ichar_traits> srcVal( src.c_str(), src.length() );

	if (  srcVal == "true" || srcVal == "y" || srcVal == "yes" ) {
		dst = true;
	} else if ( srcVal == "false" || srcVal == "n" || srcVal == "no" ) {
		dst = false;
	} else {
		LOG( Runtime, warning ) << util::MSubject( src ) << " is ambiguous while converting to " <<typeName<bool>();
		return cPosOverflow;
	}

	return cInRange;
}

template<bool IS_NUM> struct Tokenizer { //jump from number to number in the string ignoring anything else
	static std::list<std::string> run( const std::string &src ) {
		std::list<std::string> ret;
		const char *mask = "0123456789-eE.";
		const char *start_mask = "0123456789-";

		for( size_t i = src.find_first_of( start_mask ), end; i < std::string::npos; i = src.find_first_of( start_mask, end ) ) {
			end = src.find_first_not_of( mask, i );
			const std::string numstr = src.substr( i, end - i );
			ret.push_back( numstr );
		}

		return ret;
	}
};
template<> struct Tokenizer<false> { // not for numbers / tokenize string at spaces,"," and ";"
	static std::list<std::string> run( const std::string &src ) {
		return util::stringToList<std::string>( src );
	}
};

template<typename DST> struct StrTransformer {
	range_check_result range_ok;
	StrTransformer(): range_ok( cInRange ) {}
	DST operator()( const std::string &src ) {
		DST ret;
		const range_check_result result = str2scalar( src, ret );

		if( result != cInRange )
			range_ok = result; // keep the last error

		return ret;
	}
};

//helper to convert strings to FixedVectors
template<typename DST, int NUM> range_check_result convertStr2Vector(const Value &src, std::array<DST, NUM> &dstList )
{
	const std::list<std::string> srcList = Tokenizer<std::is_arithmetic_v<DST>>::run( std::get<std::string>(src) ); // tokenize the string based on the target type
	auto end = srcList.begin();
	std::advance( end, std::min<size_t>( srcList.size(), NUM ) ); // use a max of NUM tokens
	StrTransformer<DST> transformer; // create a transformer from string to DST
	std::transform( srcList.begin(), end, dstList.begin(), transformer ); // transform the found strings to the destination
	return transformer.range_ok;
}

// additional base for converters which use another converter
template<typename SRC, typename DST> struct SubValueConv {
	std::shared_ptr<const ValueConverterBase> sub_conv;
};
template<typename SRC, typename DST> struct IterableSubValueConv: SubValueConv<SRC, DST> {
	template<typename SRC_LST, typename DST_LST> range_check_result
	convertIter2Iter( const SRC_LST &srcLst, DST_LST &dstLst )const {
		range_check_result ret = cInRange;

		auto srcAt = srcLst.begin(), srcEnd = srcLst.end();
		auto dstBegin = dstLst.begin(), dstEnd = dstLst.end();

		Value elem_dst=DST();
		while( srcAt != srcEnd ) { //slow and ugly, but flexible

			if( dstBegin != dstEnd ) {
				const range_check_result result = SubValueConv<SRC, DST>::sub_conv->convert(Value(*srcAt ), elem_dst );

				if ( ret == cInRange && result != cInRange )
					ret = result;

				*( dstBegin++ ) = std::get<DST>(elem_dst);
			} else if( *srcAt != SRC() )
				return cPosOverflow; // abort and send positive overflow if source won't fit into destination

			srcAt++;
		}

		return ret;
	}

};
template<typename CLASS, typename SRC, typename DST> static std::shared_ptr<const ValueConverterBase> getFor()
{
	std::shared_ptr<const ValueConverterBase> sub_conv = ValueConverter<is_num<SRC,DST>(), std::is_same_v<SRC, DST>, SRC, DST>::get();

	if (sub_conv) {
		std::shared_ptr<CLASS> ret(new CLASS);
		ret->sub_conv = sub_conv;
		return ret;
	} else
		return {};
}

// special to string conversions
template<typename T> std::string toStringConv( const T &src, std::false_type )
{
	std::stringstream s;
	s << std::boolalpha << src; // bool will be converted to true/false
	return s.str();
}
template<typename T> std::string toStringConv( const T &src, std::true_type ) {return std::to_string( src );}


/////////////////////////////////////////////////////////////////////////////
// Numeric version -- uses num2num
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueConverter<true, false, SRC, DST> : public ValueGenerator<SRC, DST>
{
	ValueConverter() {
		LOG( Debug, verbose_info )
				<< "Creating numeric converter from "
				<<typeName<SRC>() << " to " <<typeName<DST>();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<true, false, SRC, DST>;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		assert(src.is<SRC>());
		assert(dst.is<DST>());
		return num2num( std::get<SRC>(src), std::get<DST>(dst) );
	}
};

///////////////////////////////////////////////////////////////////////////////
// Conversion between complex numbers -- uses num2num
/////////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueConverter<false, false, std::complex<SRC>, std::complex<DST> > : public ValueGenerator<std::complex<SRC>, std::complex<DST> >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating complex-complex converter from " <<typeName<std::complex<SRC> >() << " to " <<typeName<std::complex<DST> >();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, std::complex<SRC>, std::complex<DST> >;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return num2num( std::get<std::complex<SRC>>(src), std::get<std::complex<DST> >(dst) );
	}
};

///////////////////////////////////////////////////////////////////////////////
// Conversion between color -- uses num2num
/////////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueConverter<false, false, color<SRC>, color<DST> > : public ValueGenerator<color<SRC>, color<DST> >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating color converter from " <<typeName<color<SRC> >() << " to " <<typeName<color<DST> >();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, color<SRC>, color<DST>  >;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		range_check_result res = cInRange;
		const SRC *srcVal = &std::get<color<SRC> >(src).r;
		DST *dstVal = &std::get<color<DST> >(dst).r;

		for( uint_fast8_t i = 0; i < 3; i++ ) {
			const range_check_result result = num2num( srcVal[i], dstVal[i] );

			if( result != cInRange )res = result;
		}

		return res;
	}
};

///////////////////////////////////////////////////////////////////////////////
// Conversion for "all" to complex numbers -- uses ValueConverter on the real part
/////////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueConverter<false, false, SRC, std::complex<DST> > :
	public ValueGenerator<SRC, std::complex<DST> >, private SubValueConv<SRC, DST >
{
	ValueConverter( ) {
		LOG( Debug, verbose_info )
				<< "Creating number-complex converter from "
				<<typeName<SRC>() << " to " <<typeName<std::complex<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, SRC, std::complex<DST> >, SRC, DST >();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, SRC, std::complex<DST> >, SRC, DST >();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstVal = std::get<std::complex<DST> >(dst);
		Value real=DST();
		range_check_result ret = this->sub_conv->convert( src, real );

		if( ret == cInRange )
			dstVal = std::complex<DST>( std::get<DST>(real), DST() );

		return ret;
	}
};


/////////////////////////////////////////////////////////////////////////////
// vectorX to vectorX version -- uses IterableSubValueConv::convertIter2Iter
/////////////////////////////////////////////////////////////////////////////
// vector4 => vector4
template<typename SRC, typename DST > class ValueConverter<false, false, vector4<SRC>, vector4<DST> >:
	public ValueGenerator<vector4<SRC>, vector4<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter( ) {
		LOG( Debug, verbose_info ) << "Creating vector converter from " <<typeName<vector4<SRC> >() << " to " <<typeName<vector4<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, vector4<SRC>, vector4<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, vector4<SRC>, vector4<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<vector4<SRC> >(src), std::get<vector4<DST> >(dst) );
	}
};
// vector3 => vector4
template<typename SRC, typename DST > class ValueConverter<false, false, vector3<SRC>, vector4<DST> >:
	public ValueGenerator<vector3<SRC>, vector4<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter( ) {
		LOG( Debug, verbose_info ) << "Creating vector converter from " <<typeName<vector3<SRC> >() << " to " <<typeName<vector4<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, vector3<SRC>, vector4<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, vector3<SRC>, vector4<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<vector3<SRC> >(src), std::get<vector4<DST> >(dst) );
	}
};
// vector4 => vector3
template<typename SRC, typename DST > class ValueConverter<false, false, vector4<SRC>, vector3<DST> >:
	public ValueGenerator<vector4<SRC>, vector3<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter( ) {
		LOG( Debug, verbose_info ) << "Creating vector converter from " <<typeName<vector4<SRC> >() << " to " <<typeName<vector3<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, vector4<SRC>, vector3<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, vector4<SRC>, vector3<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<vector4<SRC> >(src), std::get<vector3<DST> >(dst) );
	}
};
// vector3 => vector3
template<typename SRC, typename DST > class ValueConverter<false, false, vector3<SRC>, vector3<DST> >:
	public ValueGenerator<vector3<SRC>, vector3<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter( ) {
		LOG( Debug, verbose_info ) << "Creating vector converter from " <<typeName<vector3<SRC> >() << " to " <<typeName<vector3<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, vector3<SRC>, vector3<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, vector3<SRC>, vector3<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<vector3<SRC> >(src), std::get<vector3<DST> >(dst) );
	}
};

/////////////////////////////////////////////////////////////////////////////
// list to list version -- uses IterableSubValueConv::convertIter2Iter
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST > class ValueConverter<false, false, std::list<SRC>, std::list<DST> >:
	public ValueGenerator<std::list<SRC>, std::list<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating list converter from " <<typeName<std::list<SRC> >() << " to " <<typeName<std::list<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, std::list<SRC>, std::list<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, std::list<SRC>, std::list<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstVal = std::get<std::list<DST> >(dst);
		LOG_IF( ! dstVal.empty(), CoreLog, warning )
				<< "Storing into non empty list while conversion from "
				<<typeName<std::list<SRC> >() << " to " <<typeName<std::list<DST> >();
		const auto &srcVal = std::get<std::list<SRC> >(src);
		dstVal.resize( srcVal.size() );

		return IterableSubValueConv<SRC, DST >::convertIter2Iter( srcVal, dstVal );
	}
};
/////////////////////////////////////////////////////////////////////////////
// vectorX to list version -- uses IterableSubValueConv::convertIter2Iter
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST > class ValueConverter<false, false, util::vector3<SRC>, std::list<DST> >:
	public ValueGenerator<util::vector3<SRC>, std::list<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating list converter from " <<typeName<util::vector3<SRC> >() << " to " <<typeName<std::list<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, util::vector3<SRC>, std::list<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, util::vector3<SRC>, std::list<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstVal = std::get<std::list<DST> >(dst);
		LOG_IF( ! dstVal.empty(), CoreLog, warning ) << "Storing into non empty list while conversion from " <<typeName<util::vector3<SRC> >() << " to " <<typeName<std::list<DST> >();
		dstVal.resize( 3 );

		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<util::vector3<SRC> >(src), dstVal );
	}
};
template<typename SRC, typename DST > class ValueConverter<false, false, util::vector4<SRC>, std::list<DST> >:
	public ValueGenerator<util::vector4<SRC>, std::list<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating list converter from " <<typeName<util::vector4<SRC> >() << " to " <<typeName<std::list<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, util::vector4<SRC>, std::list<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, util::vector4<SRC>, std::list<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstVal = std::get<std::list<DST> >(dst);
		LOG_IF( ! dstVal.empty(), CoreLog, warning ) << "Storing into non empty list while conversion from " <<typeName<util::vector4<SRC> >() << " to " <<typeName<std::list<DST> >();
		dstVal.resize( 4 );
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<util::vector4<SRC> >(src), dstVal );
	}
};
/////////////////////////////////////////////////////////////////////////////
// list to vectorX version -- uses IterableSubValueConv::convertIter2Iter
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST > class ValueConverter<false, false, std::list<SRC>, vector3<DST> >:
	public ValueGenerator<std::list<SRC>, vector3<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating list converter from " <<typeName<std::list<SRC> >() << " to " <<typeName<vector3<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, std::list<SRC>, vector3<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, std::list<SRC>, vector3<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<std::list<SRC> >(src), std::get<vector3<DST> >(dst) );
	}
};
template<typename SRC, typename DST > class ValueConverter<false, false, std::list<SRC>, vector4<DST> >:
	public ValueGenerator<std::list<SRC>, vector4<DST> >, private IterableSubValueConv<SRC, DST >
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating list converter from " <<typeName<std::list<SRC> >() << " to " <<typeName<vector4<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, std::list<SRC>, vector4<DST> >, SRC, DST>();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, std::list<SRC>, vector4<DST> >, SRC, DST>();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return IterableSubValueConv<SRC, DST >::convertIter2Iter( std::get<std::list<SRC> >(src), std::get<vector4<DST> >(dst) );
	}
};


/////////////////////////////////////////////////////////////////////////////////////////////////////
// string to scalar -- uses stringTo (and in some cases numeric conversion) to convert from string
/////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename DST> class ValueConverter<false, false, std::string, DST> : public ValueGenerator<std::string, DST>
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating from-string converter for " <<typeName<DST>();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, std::string, DST>;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return str2scalar( std::get<std::string>(src), std::get<DST>(dst) );
	}
};
// cannot use the general str to all because that would be ambiguous with "all to complex"
template<typename DST> class ValueConverter<false, false, std::string, std::complex<DST> > :
	public ValueGenerator<std::string, std::complex<DST> >, private SubValueConv<std::complex<double>, std::complex<DST> >
{
	ValueConverter() {
		LOG( Debug, verbose_info )
				<< "Creating from-string converter for " <<typeName<std::complex<DST> >();
	};
	friend std::shared_ptr<const ValueConverterBase> getFor<ValueConverter<false, false, std::string, std::complex<DST> >, std::complex<double>, std::complex<DST> >();
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		return getFor<ValueConverter<false, false, std::string, std::complex<DST> >, std::complex<double>, std::complex<DST> >();
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		std::complex<double> buffer;
		if(stringTo(std::get<std::string>(src),buffer)){// make a double from the string
			return num2num( buffer, std::get<std::complex<DST> >(dst) );
		} else {
			dst = DST();
			LOG( Runtime, error ) << "Miserably failed to interpret " << MSubject( src ) << " as " <<typeName<DST>() << " returning " << MSubject( DST() );
		}

		return cInRange;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
// all to string -- just use formatted print into a string buffer
/////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename SRC> class ValueConverter<false, false, SRC, std::string> : public ValueGenerator<SRC, std::string>
{
	ValueConverter() {
		LOG( Debug, verbose_info )
				<< "Creating to-string converter for " <<typeName<SRC>();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, SRC, std::string>;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		dst = toStringConv( std::get<SRC>(src), std::is_integral<SRC>() );
		return cInRange; // this should always be ok
	}
};

/////////////////////////////////////////////////////////////////////////////
// string => list/vector/color version -- uses util::stringToList
/////////////////////////////////////////////////////////////////////////////
template<typename DST> class ValueConverter<false, false, std::string, std::list<DST> >:
	public ValueGenerator<std::string, std::list<DST> >
{
	ValueConverter() {
		LOG( Debug, verbose_info )
				<< "Creating from-string converter for " <<typeName<std::list<DST> >();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, std::string, std::list<DST> >;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstList = std::get<std::list<DST> >(dst);
		const std::list<std::string> srcList = Tokenizer<std::is_arithmetic_v<DST>>::run( std::get<std::string>(src) ); // tokenize the string based on the target type
		dstList.resize( srcList.size() ); // resize target to the amount of found tokens
		StrTransformer<DST> transformer; // create a transformer from string to DST
		std::transform( srcList.begin(), srcList.end(), dstList.begin(), transformer ); // transform the found strings to the destination
		return transformer.range_ok;
	}
};
template<typename DST> class ValueConverter<false, false, std::string, vector4<DST> >: public ValueGenerator<std::string, vector4<DST> >  //string => vector4
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating from-string converter for " <<typeName<vector4<DST> >();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, std::string, vector4<DST> >;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return _internal::convertStr2Vector<DST, 4>( src, std::get<vector4<DST> >(dst) );
	}
};

template<typename DST> class ValueConverter<false, false, std::string, vector3<DST> >: public ValueGenerator<std::string, vector3<DST> >  //string => vector3
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating from-string converter for " <<typeName<vector3<DST> >();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, std::string, vector3<DST> >;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		return _internal::convertStr2Vector<DST, 3>( src, std::get<vector3<DST> >(dst) );
	}
};

template<typename T> class ValueConverter<false, false, std::string, color<T> >: public ValueGenerator<std::string, color<T> >  //string => color
{
	ValueConverter() {
		LOG( Debug, verbose_info )
				<< "Creating from-string converter for " <<typeName<color<T> >();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, std::string, color<T> >;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstVal = std::get<color<T> >(dst);
		const std::list<std::string> srcList = Tokenizer<std::is_arithmetic_v<T>>::run( std::get<std::string>(src) ); // tokenize the string based on the target type
		auto end = srcList.begin();
		std::advance( end, std::min<size_t>( srcList.size(), 3 ) ); // use a max of 3 tokens
		StrTransformer<T> transformer; // create a transformer from string to DST
		std::transform( srcList.begin(), end, &dstVal.r, transformer ); // transform the found strings to the destination
		return transformer.range_ok;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// Conversion timepoint => date (date is just a timestamp with days as resolution, so it equals a rounding
//////////////////////////////////////////////////////////////////////////////////////////////////////////
template<> class ValueConverter<false, false, util::timestamp, util::date > : public ValueGenerator<util::timestamp, util::date>
{
	ValueConverter() {
		LOG( Debug, verbose_info ) << "Creating time converter from " <<typeName<util::timestamp>() << " to " <<typeName<util::date>();
	};
public:
	static std::shared_ptr<const ValueConverterBase> get() {
		auto *ret = new ValueConverter<false, false, util::timestamp, util::date>;
		return std::shared_ptr<const ValueConverterBase>( ret );
	}
	range_check_result convert(const Value &src, Value &dst )const override {
		auto &dstVal = std::get<util::date>(dst);
		dstVal=std::chrono::time_point_cast<std::chrono::days>(std::get<util::timestamp>(src));

		return cInRange;
	}
};


////////////////////////////////////////////////////////////////////////
//OK, that's about the foreplay. Now we get to the dirty stuff.
////////////////////////////////////////////////////////////////////////
typedef std::shared_ptr<const ValueConverterBase> ConverterPtr;
typedef std::map< size_t , std::map<size_t, ConverterPtr> > ConverterMap;

template<typename DST> struct MakeConvVisitor{
    template<typename SRC> std::shared_ptr<const ValueConverterBase> operator()(const SRC &)const{
		//create a converter based on the type traits and the types of SRC and DST
		return ValueConverter<is_num<SRC,DST>(), std::is_same_v<SRC, DST>, SRC, DST>::get();
    }
};

template<int I=0> constexpr void makeInnerConv(const ValueTypes &src,ConverterMap &m_map){
	if constexpr (I<util::Value::NumOfTypes) {
		MakeConvVisitor<Value::TypeByIndex<I>> vis;
		m_map[src.index()][I] = std::visit(vis, src);
		makeInnerConv<I + 1>(src, m_map);
	}
}

template<int I=0> constexpr void makeOuterConv(ConverterMap &m_map){
	if constexpr (I<util::Value::NumOfTypes) {
		makeInnerConv(ValueTypes{Value::TypeByIndex<I>()}, m_map);
		makeOuterConv<I + 1>(m_map);
	}
}

ValueConverterMap::ValueConverterMap()
{
	makeOuterConv( *this );
	LOG( Debug, info ) << "conversion map for " << size() << " types created";
}

}
API_EXCLUDE_END;
}
/// @endcond
