#include "types.hpp"
#include "value.hpp"
#include "types_array.hpp"

namespace isis{
namespace util{

#define setName(type,name) template<> std::string _internal::name_visitor::operator()<type>(const type&)const{return name;}

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

setName( std::string, "string" );
// setName( Selection, "selection" );

setName( timestamp, "timestamp" );
setName( duration, "duration" );
setName( date, "date" );

#undef setName

namespace _internal{
template<std::size_t index = std::variant_size_v<ValueTypes>-1> void insert_types(std::map<size_t, std::string> &map,bool arrayTypesOnly) {
	typedef util::ValueNew::TypeByIndex<index> v_type;
	if(
		!arrayTypesOnly || 
		_internal::variant_index<data::ArrayTypes,std::remove_cv_t<std::shared_ptr<v_type>>>()!=std::variant_npos
	) //if either we don't want "arrayTypesOnly" or v_type is an array type
		map[index]=util::typeName<v_type>(); //notice we always use the values type list to map type ids to type names
	insert_types<index-1>(map,arrayTypesOnly);
}
template<> void insert_types<0>(std::map<size_t, std::string> &map,bool arrayTypesOnly){
	typedef util::ValueNew::TypeByIndex<0> v_type;
	if(
		!arrayTypesOnly || 
		_internal::variant_index<data::ArrayTypes,std::remove_cv_t<std::shared_ptr<v_type>>>()!=std::variant_npos
	) //if either we don't want "arrayTypesOnly" or v_type is an array type
		map[0]=util::typeName<v_type>(); //notice we always use the values type list to map type ids to type names
}
}

}
}

std::map<size_t, std::string> isis::util::getTypeMap(bool arrayTypesOnly){
	std::map<size_t, std::string> types;
	_internal::insert_types(types,arrayTypesOnly);
	return types;
}

isis::util::date& std::operator+=(isis::util::date& x, const isis::util::duration& y)
{
	return x+=chrono::duration_cast<chrono::days>(y);
}
isis::util::date& std::operator-=(isis::util::date& x, const isis::util::duration& y)
{
	return x-=chrono::duration_cast<chrono::days>(y);
}
