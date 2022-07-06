#include "types.hpp"
#include "value.hpp"
#include "types_array.hpp"

namespace isis{
namespace util{

#define setName(type,name) template<> const char* _internal::name_visitor::operator()<type>(const type&)const{return name;}

setName( bool, "boolean" );

setName( int8_t, "s8bit" );
setName( uint8_t, "u8bit" );

setName( int16_t, "s16bit" );
setName( uint16_t, "u16bit" );

setName( int32_t, "s32bit" );
setName( uint32_t, "u32bit" );

setName( int64_t, "s64bit" );
setName( uint64_t, "u64bit" );

setName( float, "float" );
setName( double, "double" );

setName( color24, "color24" );
setName( color48, "color48" );

setName( fvector3, "fvector3" );
setName( fvector4, "fvector4" );
setName( dvector3, "dvector3" );
setName( dvector4, "dvector4" );
setName( ivector3, "ivector3" );
setName( ivector4, "ivector4" );
 
setName( ilist, "list<int32_t>" );
setName( dlist, "list<double>" );
setName( slist, "list<string>" );

setName( std::complex<float>, "complex<float>" );
setName( std::complex<double>, "complex<double>" );

setName( util::Selection, "Selection" );

setName( std::string, "string" );
// setName( Selection, "selection" );

setName( timestamp, "timestamp" );
setName( duration, "duration" );
setName( date, "date" );

#undef setName

namespace _internal{
template<KnownValueType TYPE> constexpr auto namer(){
	return std::make_pair(typeID<TYPE>(),util::typeName<TYPE>());
}
template<typename TYPE> constexpr auto namer() requires KnownValueType<typename TYPE::element_type>{
	return namer<typename TYPE::element_type>();
}
template<typename _First, typename... _Rest>
std::map<size_t, std::string> map_types(std::variant<_First, _Rest...>*)
{
	std::map<size_t, std::string> ret;
	ret.insert(namer<_First>());
	if constexpr(sizeof...(_Rest)) {
		ret.merge(map_types(static_cast<std::variant<_Rest...>*>(nullptr)));
	}
	return ret;
}

}

const std::map<size_t, std::string> &getTypeMap(bool arrayTypesOnly){
	if(arrayTypesOnly){
		static const auto ret = _internal::map_types(static_cast<data::ArrayTypes*>(nullptr));
		return ret;
	} else {
		static const auto ret = _internal::map_types(static_cast<util::ValueTypes*>(nullptr));
		return ret;
	}
}
}
}


isis::util::date& std::operator+=(isis::util::date& x, const isis::util::duration& y)
{
	return x+=chrono::duration_cast<chrono::days>(y);
}
isis::util::date& std::operator-=(isis::util::date& x, const isis::util::duration& y)
{
	return x-=chrono::duration_cast<chrono::days>(y);
}
std::ostream &std::operator<<(std::ostream &out, const isis::util::duration &s)
{
	return out<<std::chrono::duration_cast<std::chrono::milliseconds>(s).count()<<"ms";
}
std::ostream &std::operator<<(std::ostream &out, const isis::util::date &s)
{
	const time_t tme(chrono::duration_cast<chrono::seconds>(s.time_since_epoch()).count());
	return out<<std::put_time(std::localtime(&tme), "%x");
}
std::ostream &std::operator<<(std::ostream &out, const isis::util::timestamp &s)
{
	const chrono::seconds sec=std::chrono::duration_cast<chrono::seconds>(s.time_since_epoch());
	const time_t tme(sec.count());
	if(s>=(isis::util::timestamp()+std::chrono::hours(24))) // if we have a real timepoint (not just time)
		out<<std::put_time(std::localtime(&tme), "%c"); // write time and date
	else {
		out<<std::put_time(std::localtime(&tme), "%X"); // otherwise, write just the time
	}
	// and maybe with milliseconds

	auto msec = std::chrono::duration_cast<chrono::milliseconds>(s.time_since_epoch()-sec);
	assert(msec.count()<1000);
	if(msec.count()){
		if(msec.count()<0)
			msec+=chrono::seconds(1);
		out << "+" << msec;
	}
	return out;
}
