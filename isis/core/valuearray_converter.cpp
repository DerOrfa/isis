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

//TODO maybe use standard traits is_trivially_default_constructible and is_trivially_copy_constructible
#ifdef _MSC_VER
#pragma warning(disable:4800 4996)
#endif

#include "valuearray_converter.hpp"

#include "valuearray.hpp"
#include "numeric_convert.hpp"
#include "types.hpp"

#ifdef ISIS_USE_LIBOIL
#include <liboil/liboil.h>
#endif

/// @cond _internal
namespace isis
{
namespace data
{
API_EXCLUDE_BEGIN;
namespace _internal
{

size_t getConvertSize( const ValueArrayNew &src, const ValueArrayNew &dst )
{
	LOG_IF( src.getLength() > dst.getLength(), Runtime, error ) << "The " << src.getLength() << " elements of src wont fit into the destination. Will only convert " << dst.getLength() << " elements.";
	LOG_IF( src.getLength() < dst.getLength(), Debug, info ) << "Source is shorter than destination. Will only convert " << src.getLength() << " values";
	return std::min( src.getLength(), dst.getLength() );
}
static bool checkScale( const scaling_pair &scaling )
{
	const double scale = scaling.first.as<double>(), offset = scaling.second.as<double>();
	return ( scale != 1 || offset );
}

template<typename SRC, typename DST>
scaling_pair getScalingToColor( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )
{
	double scalMin, scalMax;

	if( min.isFloat() || min.isInteger() )scalMin = min.as<double>(); // if min is allready a scalar
	else { // of not, determine the scalar min from the elements
		const util::color48 minCol = min.as<util::color48>(); //use the "biggest" known color type
		scalMin = *std::min_element( &minCol.r, &minCol.b ); // take the lowest value
	}

	if( max.isFloat() || max.isInteger() )scalMax = max.as<double>(); // if max is allready a scalar
	else { // of not, determine the scalar min from the elements
		const util::color48 maxCol = max.as<util::color48>(); //use the "biggest" known color type
		scalMax = *std::max_element( &maxCol.r, &maxCol.b ); // take the lowest value
	}

	return getNumericScaling<SRC, DST>( scalMin, scalMax, scaleopt );;
}
template<typename SRC, typename DST>
scaling_pair getScalingToComplex( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )
{
	double scalMin, scalMax;

	if( min.isFloat() || min.isInteger() )scalMin = min.as<double>(); // if min is allready a scalar
	else { // of not, determine the scalar min from the elements
		const std::complex<double> minCpl = min.as<std::complex<double> >();
		scalMin = std::min( minCpl.real(), minCpl.imag() );
	}

	if( max.isFloat() || max.isInteger() )scalMax = max.as<double>(); // if max is allready a scalar
	else { // of not, determine the scalar min from the elements
		const std::complex<double> maxCpl = max.as<std::complex<double> >();
		scalMax = std::max( maxCpl.real(), maxCpl.imag() );
	}

	return getNumericScaling<SRC, DST>( scalMin, scalMax, scaleopt );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basic numeric conversion class
////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct NumConvImplBase {
	static scaling_pair getScaling( const util::ValueNew &/*min*/, const util::ValueNew &/*max*/, autoscaleOption /*scaleopt*/ ) {
		return scaling_pair( 1, 0 );
	}
};
// default generic conversion between numeric types
template<typename SRC, typename DST, bool SAME> struct NumConvImpl: NumConvImplBase {
	static void convert( const SRC *src, DST *dst, const scaling_pair &scaling, size_t size ) {
		const double scale = scaling.first.as<double>(), offset = scaling.second.as<double>();
		numeric_convert( src, dst, size, scale , offset );
	}
	static scaling_pair getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt ) {
		return getNumericScaling<SRC, DST>( min, max, scaleopt );
	}
};
// special generic conversion between equal numeric types (maybe we can copy / scaling will be 1/0)
template<typename T> struct NumConvImpl<T, T, true>: NumConvImplBase {
	static void convert( const T *src, T *dst, const scaling_pair &scaling, size_t size ) {
		if( checkScale( scaling ) ) {
			const double scale = scaling.first.as<double>(), offset = scaling.second.as<double>();
			numeric_convert( src, dst, size, scale , offset );
		} else { // if there is no scaling - we can copy
			numeric_copy( src, dst, size );
		}
	}
};

// specialisation for bool (anything that is "<=0" after the scaling results in false)
template<typename SRC> struct NumConvImpl<SRC, bool, false>: NumConvImplBase {
	static void convert( const SRC *src, bool *dst, const scaling_pair &scaling, size_t size ) {
		if( checkScale( scaling ) ) {
			const double scale = scaling.first.as<double>(), offset = scaling.second.as<double>();
			while( size-- )
				*( dst++ ) = ( (*( src++ ) * scale) > -offset);
		} else
			while( size-- )
				*( dst++ ) = ( *( src++ ) != 0 );
	}
};
template<typename DST> struct NumConvImpl<bool, DST, false>: NumConvImplBase {
	static void convert( const bool *src, DST *dst, const scaling_pair &/*scaling*/, size_t size ) {
		while( size-- )
			*( dst++ ) = *( src++ ) ? 1 : 0 ;
	}
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// scalar to container functors
////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename S, typename D> struct copy_op_base {
	typedef D dst_type;
	typedef const S* iter_type;
	iter_type s;
	copy_op_base( iter_type _s ): s( _s ) {}
	D getScal() {
		return round<D>( *( s++ ) );
	}
};
template<typename S, typename D> struct scaling_op_base : copy_op_base<S, D> {
	double scale, offset;
	scaling_op_base( typename copy_op_base<S, D>::iter_type s ): copy_op_base<S, D>( s ) {}
	void setScale( scaling_pair scaling ) {
		scale = scaling.first.as<double>();
		offset = scaling.second.as<double>();
	}
	D getScal() {
		return round<D>( *( copy_op_base<S, D>::s++ ) * scale + offset );
	}
};
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// OK, now the converter classes
////////////////////////////////////////////////////////////////////////////////////////////////////////////

//default implementation of ValueArrayConverterBase::getScaling - allways returns scaling of 1/0 - should be overridden by real converters if they do use a scaling
scaling_pair ValueArrayConverterBase::getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt ) const
{
	return _internal::NumConvImplBase::getScaling( min, max, scaleopt );
}
void ValueArrayConverterBase::convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &/*scaling*/ ) const
{
	LOG( Debug, error ) << "Empty conversion was called as conversion from " << src.typeName() << " to " << dst.typeName() << " this is most likely an error.";
}

namespace _internal{
//Define generator - this can be global because its using convert internally
template<typename SRC, typename DST> class ValueArrayGenerator: public ValueArrayConverterBase
{
public:
	ValueArrayNew create(const size_t len )const override {
		return ValueArrayNew( ( DST * )malloc( sizeof( DST )*len ), len );
	}
	ValueArrayNew generate( const ValueArrayNew &src, const scaling_pair &scaling )const override {
		//Create new "stuff" in memory
		ValueArrayNew dst=create(src.getLength() );
		convert( src, dst, scaling );//and convert into that
		return dst;
	}
};


/////////////////////////////////////////////////////////////////////////////
// general converter version -- does nothing and returns 1/0 as scaling
/////////////////////////////////////////////////////////////////////////////
template<bool SRC_NUM, bool DST_NUM, typename SRC, typename DST> class ValueArrayConverter : public ValueArrayGenerator<SRC, DST>
{
public:
	virtual ~ValueArrayConverter() {}
};

/////////////////////////////////////////////////////////////////////////////
// trivial version -- for conversion of the same non numeric type (scaling will fail)
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueArrayConverter<false, false, SRC, DST> : public ValueArrayGenerator<SRC, DST>
{
	ValueArrayConverter() {
		LOG( Debug, verbose_info )  << "Creating trivial copy converter for " << util::typeName<SRC>();
	};
public:
	static std::shared_ptr<const ValueArrayConverterBase> get() {
		auto ret = new ValueArrayConverter<false, false, SRC, DST>;
		return std::shared_ptr<const ValueArrayConverterBase>( ret );
	}
	void convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &scaling )const {
		SRC *dstPtr = dst.castTo<SRC>().get();
		const SRC *srcPtr = src.castTo<SRC>().get();
		LOG_IF( checkScale( scaling ),  Runtime, error )  << "Scaling is ignored when copying data of type "  << src.typeName() << " to " << dst.typeName() ;
		memcpy( dstPtr, srcPtr, getConvertSize( src, dst )*src.bytesPerElem() );
	}
	virtual ~ValueArrayConverter() {}
};


/////////////////////////////////////////////////////////////////////////////
// Numeric version -- uses numeric_convert
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueArrayConverter<true, true, SRC, DST> : public ValueArrayGenerator<SRC, DST>
{
	ValueArrayConverter() {
		LOG( Debug, verbose_info ) << "Creating numeric converter from " << util::typeName<SRC>() << " to " << util::typeName<DST>();
	};
public:
	static std::shared_ptr<const ValueArrayConverterBase> get() {
		auto ret = new ValueArrayConverter<true, true, SRC, DST>;
		return std::shared_ptr<const ValueArrayConverterBase>( ret );
	}
	void convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &scaling )const {
		DST *dstPtr = dst.castTo<DST>().get();
		const SRC *srcPtr = src.castTo<SRC>().get();
		NumConvImpl<SRC, DST, std::is_same<SRC, DST>::value>::convert( srcPtr, dstPtr, scaling, getConvertSize( src, dst ) );
	}
	scaling_pair getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )const {
		return NumConvImpl<SRC, DST, std::is_same<SRC, DST>::value >::getScaling( min, max, scaleopt );
	}
	virtual ~ValueArrayConverter() {}
};

/////////////////////////////////////////////////////////////////////////////
// complex to complex version
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueArrayConverter<false, false, std::complex<SRC>, std::complex<DST> > : public ValueArrayGenerator<std::complex<SRC>, std::complex<DST> >
{
	ValueArrayConverter() {
		LOG( Debug, verbose_info )
				<< "Creating complex converter from "
				<< util::typeName<std::complex<SRC>>() << " to " << util::typeName<std::complex<DST> >();
	};
public:
	static std::shared_ptr<const ValueArrayConverterBase> get() {
		ValueArrayConverter<false, false, std::complex<SRC>, std::complex<DST> > *ret = new ValueArrayConverter<false, false, std::complex<SRC>, std::complex<DST> >;
		return std::shared_ptr<const ValueArrayConverterBase>( ret );
	}
	void convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &scaling )const {
		//@todo we do an evil hack here assuming std::complex is POD - at least check if the size of std::complex is reasonable
		static_assert( sizeof( std::complex<SRC> ) == sizeof( SRC ) * 2, "complex type apparently not POD" );
		static_assert( sizeof( std::complex<DST> ) == sizeof( DST ) * 2, "complex type apparently not POD" );

		const SRC *sp = reinterpret_cast<const SRC*>(src.castTo<std::complex<SRC>>().get());
		      DST *dp = reinterpret_cast<      DST*>(dst.castTo<std::complex<DST>>().get());

		NumConvImpl<SRC, DST, std::is_same<SRC, DST>::value>::convert( sp, dp, scaling, getConvertSize( src, dst ) * 2 );
	}
	scaling_pair getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )const {
		return getScalingToComplex<SRC, DST>( min, max, scaleopt );
	}
	virtual ~ValueArrayConverter() {}
};

/////////////////////////////////////////////////////////////////////////////
// numeric to complex version
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueArrayConverter<true, false, SRC, std::complex<DST> > : public ValueArrayGenerator<std::complex<SRC>, std::complex<DST> >
{
	ValueArrayConverter() {
		LOG( Debug, verbose_info )
				<< "Creating converter from scalar "
				<< util::typeName<SRC>() << " complex to " << util::typeName<std::complex<DST> >();
	};
	template<typename BASE> struct num2complex: BASE {
		num2complex( typename BASE::iter_type s ): BASE( s ) {}
		std::complex<typename BASE::dst_type> operator()() {
			return BASE::getScal();
		}
	};
public:
	static std::shared_ptr<const ValueArrayConverterBase> get() {
		ValueArrayConverter<true, false, SRC, std::complex<DST> > *ret = new ValueArrayConverter<true, false, SRC, std::complex<DST> >;
		return std::shared_ptr<const ValueArrayConverterBase>( ret );
	}
	void convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &scaling )const {

		size_t size = getConvertSize( src, dst );
		auto s = src.beginTyped<SRC>();
		auto d = dst.beginTyped<std::complex<DST> >();

		if( checkScale( scaling ) ) {
			num2complex<scaling_op_base<SRC, DST> > op( s );
			op.setScale( scaling );
			std::generate_n( d, size, op );
		} else { // if there is no scaling - we can copy
			num2complex<copy_op_base<SRC, DST> > op( s );
			std::generate_n( d, size, op );
		}

	}
	scaling_pair getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )const {
		return getScalingToComplex<SRC, DST>( min, max, scaleopt );
	}
	virtual ~ValueArrayConverter() {}
};

/////////////////////////////////////////////////////////////////////////////
// color to color version - using numeric_convert on each color with a global scaling
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueArrayConverter<false, false, util::color<SRC>, util::color<DST> > : public ValueArrayGenerator<util::color<SRC>, util::color<DST> >
{
	ValueArrayConverter() {
		LOG( Debug, verbose_info )
				<< "Creating color converter from "
				<< util::typeName<util::color<SRC> >() << " to " << util::typeName<util::color<DST> >();
	};
public:
	static std::shared_ptr<const ValueArrayConverterBase> get() {
		ValueArrayConverter<false, false, util::color<SRC>, util::color<DST> > *ret = new ValueArrayConverter<false, false, util::color<SRC>, util::color<DST> >;
		return std::shared_ptr<const ValueArrayConverterBase>( ret );
	}
	void convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &scaling )const {
		const SRC *sp = &src.castTo<util::color<SRC> >().get()->r;
		DST *dp = &dst.castTo<util::color<DST> >().get()->r;
		NumConvImpl<SRC, DST, false>::convert( sp, dp, scaling, getConvertSize( src, dst ) * 3 );
	}
	scaling_pair getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )const {
		return getScalingToColor<SRC, DST>( min, max, scaleopt );
	}

	virtual ~ValueArrayConverter() {}
};
/////////////////////////////////////////////////////////////////////////////
// numeric to color version
/////////////////////////////////////////////////////////////////////////////
template<typename SRC, typename DST> class ValueArrayConverter<true, false, SRC, util::color<DST> > : public ValueArrayGenerator<util::color<SRC>, util::color<DST> >
{
	ValueArrayConverter() {
		LOG( Debug, verbose_info )
				<< "Creating converter from scalar "
				<< util::typeName<SRC>() << " to color " << util::typeName<util::color<DST>>();
	};
	template<typename BASE> struct num2color: BASE {
		num2color( typename BASE::iter_type s ): BASE( s ) {}
		util::color<typename BASE::dst_type> operator()() {
			const typename BASE::dst_type val = BASE::getScal();
			const util::color<typename BASE::dst_type> ret = {val, val, val};
			return ret;
		}
	};

public:
	static std::shared_ptr<const ValueArrayConverterBase> get() {
		ValueArrayConverter<true, false, SRC, util::color<DST> > *ret = new ValueArrayConverter<true, false, SRC, util::color<DST> >;
		return std::shared_ptr<const ValueArrayConverterBase>( ret );
	}
	void convert( const ValueArrayNew &src, ValueArrayNew &dst, const scaling_pair &scaling )const {
		size_t size = getConvertSize( src, dst );
		auto s = src.beginTyped<SRC>();
		auto d = dst.beginTyped<util::color<DST> >();

		if( checkScale( scaling ) ) {
			num2color<scaling_op_base<SRC, DST> > op( s );
			op.setScale( scaling );
			std::generate_n( d, size, op );
		} else { // if there is no scaling - we can copy
			num2color<copy_op_base<SRC, DST> > op( s );
			std::generate_n( d, size, op );
		}
	}
	scaling_pair getScaling( const util::ValueNew &min, const util::ValueNew &max, autoscaleOption scaleopt = autoscale )const {
		return getScalingToColor<SRC, DST>( min, max, scaleopt );
	}

	virtual ~ValueArrayConverter() {}
};


////////////////////////////////////////////////////////////////////////
//OK, that's it about the foreplay. Now we get to the dirty stuff.
////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<const ValueArrayConverterBase> ConverterPtr;
typedef std::map< int , std::map<int, ConverterPtr> > ConverterMap;

// inner visitor templates over the DST type ("visits" SRC)
template<typename DST_ARRAY> struct MakeConvVisitor{
    template<typename SRC_ARRAY> std::shared_ptr<const ValueArrayConverterBase> operator()(const SRC_ARRAY &)const{
		//create a converter based on the type traits and the types of SRC and DST
		typedef typename SRC_ARRAY::element_type SRC;
		typedef typename DST_ARRAY::element_type DST;
		return ValueArrayConverter<std::is_arithmetic_v<SRC>, std::is_arithmetic_v<DST>, SRC, DST>::get();
    }
};

// starting point for inner visitor recursion
template<int I=0> constexpr void makeInnerConv(const ArrayTypes &src,ConverterMap &m_map){
    MakeConvVisitor<ValueArrayNew::TypeByIndex<I>> vis;
	m_map[src.index()][I]= std::visit(vis,src);
    makeInnerConv<I+1>(src, m_map);
}
// terminator for inner visitor recursion
template<> constexpr void makeInnerConv<std::variant_size<ArrayTypes>::value>(const ArrayTypes &src,ConverterMap &){}

// starting point for outer visitor recursion
template<int I=0> constexpr void makeOuterConv(ConverterMap &m_map){
	typedef ValueArrayNew::TypeByIndex<I> outer_type;
    makeInnerConv(outer_type(),m_map);
	makeOuterConv<I+1>(m_map);
}
// terminator for outer visitor recursion
template<> constexpr void makeOuterConv<std::variant_size<ArrayTypes>::value>(ConverterMap &){}


ValueArrayConverterMap::ValueArrayConverterMap()
{
#ifdef ISIS_USE_LIBOIL
	LOG( Debug, info ) << "Initializing liboil";
	oil_init();
#endif // ISIS_USE_LIBOIL
	makeOuterConv( *this );
	LOG( Debug, info ) << "conversion map for " << size() << " array-types created";
}

}
API_EXCLUDE_END;
}
}
/// @endcond

