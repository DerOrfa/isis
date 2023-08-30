//
// C++ Interface: vector
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//
#pragma once
#include "common.hpp"
#include <algorithm>
#include <ostream>
#include <numeric>
#include <cmath>
#include <type_traits>

#include <array>
#include <span>

template<typename T> concept scalar = std::is_scalar_v<T>;

namespace isis::util
{
	template<scalar T, size_t N> class vector : public std::array<T,N>
	{
		template<typename OP,scalar T2>	auto binaryVecOp(const std::array<T2,N> &rhs)const{
			typedef decltype(OP{}(T(),T2())) return_type;
			vector<return_type,N> ret;
			std::transform( this->begin(), this->end(), rhs.begin(), ret.begin(), OP{} );
			return ret;
		}
		template<typename OP,typename T2> auto binaryOp ( const T2 &rhs)const{
			typedef decltype(OP{}(T(),T2())) return_type;
			vector<return_type,N> ret;
			const OP op = OP{};
			auto src= this->begin();
			for (auto &x:ret)
				x = op( *(src++), rhs);
			return ret;
		}

	public:
		/**
		 * Get the inner product.
		 * \returns \f$ \overrightarrow{this} \cdot \overrightarrow{src}  = \sum_{i=0}^{SIZE-1} {this_i * src_i} \f$
		 */
		template<scalar T2> auto dot(const std::array<T2, N> &second)const
		{
			constexpr decltype(T()*T2()) init(0);
			return std::inner_product(this->begin(), this->end(), second.begin(), init);
		}
		/**
		 * Get the inner product with itself (aka squared length).
		 * \returns \f$ \overrightarrow{this} \cdot \overrightarrow{this} = \sum_{i=0}^{SIZE-1} this_i^2 \f$
		 */
		auto sqlen()const { return dot(*this); }
		/**
		 * Get the the length of the vector.
		 * \returns \f$ \sqrt{\sum_{i=0}^{SIZE-1} this_i^2} \f$
		 */
		auto len()const { return std::sqrt(sqlen()); }
		/**
		 * Normalize the vector (make len()==1).
		 * Applies scalar division with the result of len() to this.
		 *
		 * Equivalent to:
		 * \f[ \overrightarrow{this} = \overrightarrow{this} * {1 \over {\sqrt{\sum_{i=0}^{SIZE-1} this_i^2}}}  \f]
		 *
		 * If len() is equal to zero std::invalid_argument will be thrown, and this wont be changed.
		 * \returns the changed *this
		 */
		void normalize()
		{
			const auto d = len();

			if (d == 0)throw (std::invalid_argument("Trying to normalize a vector of length 0"));
			else *this /= d;
		}

		/**
		 * Fuzzy comparison for vectors.
		 * Does util::fuzzyEqual for the associated elements of the two vectors.
		 * @param other the "other" vector to compare to
		 * @param scale scaling factor forwarded to util::fuzzyEqual
		 * \returns true if util::fuzzyEqual for all elements
		 */
		template<scalar T2> bool fuzzyEqual(const std::array<T2, N> &second, unsigned short scale = 10)const
		{
			auto b = std::begin(second);

			for (auto a: *this) {
				if (!util::fuzzyEqual(a, *(b++), scale))
					return false;
			}

			return true;
		}

		/**
		 * Compute the product of all elements.
		 * \returns \f[ \prod_{i=0}^{SIZE-1} this_i \f]
		 */
		T product()const
		{
			T ret = 1;
			for (const auto &t: *this)
				ret *= t;
			return ret;
		}

		/**
		 * Compute the sum of all elements.
		 * \returns \f[ \sum_{i=0}^{SIZE-1} this_i \f]
		 */
		auto sum()const
		{
			return std::accumulate(this->begin(), this->end(), 0);
		}

		// @todo document me
		// @todo test me
		template<typename T2> bool lexical_less_reverse(const std::array<T2, N> &second)const
		{
			auto[me, they] = std::mismatch(this->rbegin(),this->rend(),second.rbegin());
			return me!=this->rend() && (*me < *they);
		}

		// operations with other vectors
		template<scalar T2> auto operator+(const std::array<T2,N> &rhs )const{ return this->binaryVecOp<std::plus<>>(rhs);}
		template<scalar T2> auto operator-(const std::array<T2,N> &rhs )const{ return this->binaryVecOp<std::minus<>>(rhs);}
		template<scalar T2> auto operator*(const std::array<T2,N> &rhs )const{ return this->binaryVecOp<std::multiplies<>>(rhs);}
		template<scalar T2> auto operator/(const std::array<T2,N> &rhs )const{ return this->binaryVecOp<std::divides<>>(rhs);}

		vector<T,N>& operator+=(const std::array<T,N> &rhs ){ return *this = *this + rhs;}
		vector<T,N>& operator-=(const std::array<T,N> &rhs ){ return *this = *this - rhs;}
		vector<T,N>& operator*=(const std::array<T,N> &rhs ){ return *this = *this * rhs;}
		vector<T,N>& operator/=(const std::array<T,N> &rhs ){ return *this = *this / rhs;}

		template<typename T2> auto operator<=>(const std::array<T2,N> &rhs)const requires (!std::same_as<T, T2>)
		{
			auto[me,they] = std::mismatch(this->begin(),this->end(),rhs.begin());
			if(me != this->end())
				return *me <=> *they;
			else
				return std::partial_ordering::equivalent;
		}
		template<typename T2> auto operator==(const std::array<T2,N> &rhs)const requires (!std::same_as<T, T2>)
		{
			return (*this <=> rhs) == 0;
		}

		// auto because the result type is not necessarily T
		auto operator+(const scalar auto &rhs)const {return this->binaryOp<std::plus<>>(rhs); }
		auto operator-(const scalar auto &rhs)const {return this->binaryOp<std::minus<>>(rhs); }
		auto operator*(const scalar auto &rhs)const {return this->binaryOp<std::multiplies<>>(rhs); }
		auto operator/(const scalar auto &rhs)const {return this->binaryOp<std::divides<>>(rhs); }

		auto operator+=(const T &rhs) {return *this = *this + rhs; }//frontload the need having the same type to assign
		auto operator-=(const T &rhs) {return *this = *this - rhs; }
		auto operator*=(const T &rhs) {return *this = *this * rhs; }
		auto operator/=(const T &rhs) {return *this = *this / rhs; }

		auto operator-()
		{
			auto ret=*this;
			std::transform( std::begin(ret), std::end(ret), std::begin(ret), std::negate<>{} );
			return ret;
		}
		template<typename T2> static vector<T,N> fromArray(const std::array<T2,N> &arr)requires std::convertible_to<T2,T>{
			vector<T,N> ret;
			std::copy(arr.begin(),arr.end(),ret.begin());
			return ret;
		}
	};
	template<typename T, size_t N> vector<T,N> vectorFromArray(const std::array<T,N> &src)
	{
		return vector<T,N>::fromArray(src);
	}

	template<scalar TYPE> using vector3 = vector<TYPE, 3>;
	template<scalar TYPE> using vector4 = vector<TYPE, 4>;

	typedef vector4<float> fvector4;
	typedef vector4<double> dvector4;
	typedef vector4<int32_t> ivector4;
	typedef vector3<float> fvector3;
	typedef vector3<double> dvector3;
	typedef vector3<int32_t> ivector3;
}


/// @cond _internal
namespace std
{

template<typename charT, typename traits, scalar TYPE, size_t SIZE> basic_ostream<charT, traits>&
operator<<( basic_ostream<charT, traits> &out, const std::array<TYPE, SIZE>& s )
{
	isis::util::listToOStream( std::begin(s), std::end(s), out, "|", "<", ">" );
	return out;
}
}
/// @endcond _internal

