//
// C++ Interface: chunk
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

#include "vector.hpp"
#include "valuearray.hpp"
#include "log.hpp"
#include "propmap.hpp"
#include "common.hpp"
#include <cstring>
#include <list>
#include <ostream>
#include "ndimensional.hpp"

namespace isis::data
{

class Chunk;
template<typename TYPE> class TypedChunk;

/**
 * Main class for four-dimensional random-access data blocks.
 * Like in ValueArray, the copy of a Chunk will reference the same data. (cheap copy)
 * (If you want to make a memory based deep copy of a Chunk create a MemChunk from it)
 */
class Chunk : public NDimensional<4>, public util::PropertyMap, public ValueArray
{
	friend class Image;
	friend class std::vector<Chunk>;
protected:
	/** Create a Chunk "Husk" without any data
	 */
	Chunk(bool fakeValid=false);
public:
	static std::list<PropPath> neededProperties;
	typedef ValueArray::iterator iterator;
	typedef ValueArray::const_iterator const_iterator;

	static Chunk makeByID(unsigned short typeID, size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1, bool fakeValid = false);

	Chunk(const ValueArray &src, size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1, bool fakeValid = false );

	/**
	 * Gets a reference to the element at a given index.
	 * If index is invalid, behaviour is undefined. Most probably it will crash.
	 * If _ENABLE_DATA_DEBUG is true an error message will be send (but it will _not_ stop).
	 */
	template<typename TYPE> TYPE &voxel( size_t nrOfColumns, size_t nrOfRows = 0, size_t nrOfSlices = 0, size_t nrOfTimesteps = 0 ) {
		return voxel<TYPE>({nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps});
	}
	template<typename TYPE> TYPE &voxel( const std::array<size_t,4> &pos) {
		LOG_IF( ! isInRange( pos ), Debug, isis::error )
		        << "Index " << util::vector4<size_t>( pos ) << " is out of range (" << getSizeAsString() << ")";
		return at<TYPE>(getLinearIndex( pos ));
	}

	const util::Value getVoxelValue(size_t nrOfColumns, size_t nrOfRows = 0, size_t nrOfSlices = 0, size_t nrOfTimesteps = 0 )const;
	void setVoxelValue(const util::Value &val, size_t nrOfColumns, size_t nrOfRows = 0, size_t nrOfSlices = 0, size_t nrOfTimesteps = 0 );

	/**
	 * Gets a const reference of the element at a given index.
	 * \copydetails Chunk::voxel
	 */
	template<typename TYPE> const TYPE &voxel( size_t nrOfColumns, size_t nrOfRows = 0, size_t nrOfSlices = 0, size_t nrOfTimesteps = 0 )const {
		return voxel<TYPE>({nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps});
	}
	template<typename TYPE> const TYPE &voxel( const std::array<size_t,4> &pos )const {
		LOG_IF(!isInRange( pos ), Debug, isis::error )
		    << "Index " << util::vector4<size_t>( pos ) << " is out of range (" << getSizeAsString() << ")";

		return at<TYPE>(getLinearIndex( pos ));
	}
	void copySlice( size_t thirdDimS, size_t fourthDimS, Chunk &dst, size_t thirdDimD, size_t fourthDimD ) const;

	/**
	 * Run a function on every Voxel in the chunk.
	 * This will try to instantiate func for all valid Chunk-datatypes. 
	 * So this will only compile if func would be valid for all types.
	 * You might want to look into TypedChunk::foreachVoxel.
	 * And remember TypedChunk is a cheap copy if possible. So \code
	 * TypedChunk<uint32_t>(ch).foreachVoxel([](uint32_t &vox, const util::vector4<size_t> &pos){});
	 * \endcode is perfectly fine if the type of ch is already uint32_t.
	 * \param v a reference to the voxel
	 * \param offset the position of the voxel inside the chunk
	 * \returns amount of operations which returned false - so 0 is good!
	 */
	template <typename TYPE> void foreachVoxel( std::function<void(TYPE &vox, util::vector4<size_t> pos)> func )
	{
		std::visit([&](auto ptr){
			auto vox_ptr = ptr.get();
			const util::vector4<size_t> imagesize = getSizeAsVector();
			util::vector4<size_t> pos;

			for( pos[timeDim] = 0; pos[timeDim] < imagesize[timeDim]; pos[timeDim]++ )
				for( pos[sliceDim] = 0; pos[sliceDim] < imagesize[sliceDim]; pos[sliceDim]++ )
					for( pos[columnDim] = 0; pos[columnDim] < imagesize[columnDim]; pos[columnDim]++ )
						for( pos[rowDim] = 0; pos[rowDim] < imagesize[rowDim]; pos[rowDim]++ ) {
							func( *( vox_ptr++ ), pos );
						}
		},static_cast<ArrayTypes&>(*this));
	}

	template <typename TYPE> void foreachVoxel( std::function<void(TYPE &vox)> func )
	{
		std::visit([&](auto ptr){
			std::for_each(ptr.get(),ptr.get()+getLength(),func);
		},static_cast<ArrayTypes&>(*this));
	}

	/// \returns the number of cheap-copy-chunks using the same memory as this
	size_t useCount()const;

	/// Creates a new empty Chunk of different size and without properties, but of the same datatype as this.
	Chunk cloneToNew( size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1 )const;

	/// Creates a new empty Chunk without properties but of specified type and specified size.
	static Chunk createByID( size_t ID, size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1, bool fakeValid = false );

	/**
	 * Ensure, the chunk has the type with the requested ID.
	 * If the typeID of the chunk is not equal to the requested ID, the data of the chunk is replaced by an converted version.
	 * \returns false if there was an error
	 */
	bool convertToType( size_t ID, const scaling_pair &scaling = scaling_pair() );

	/**
	 * Copy all voxel data of the chunk into memory.
	 * If neccessary a conversion into T is done using min/max of the image.
	 * \param dst c-pointer for the memory to copy into
	 * \param len the allocated size of that memory in elements
	 * \param scaling the scaling to be used when converting the data (will be determined automatically if not given)
	 * \return true if copying was (at least partly) successful
	 */
	template<typename T> bool copyToMem( T *dst, size_t len, const scaling_pair &scaling = scaling_pair() )const {
		return ValueArray::copyToMem<T>(dst, len, scaling ); // use copyToMem of ValueArrayBase
	}
	/**
	 * Create a new Chunk of the requested type and copy all voxel data of the chunk into it.
	 * If neccessary a conversion into the requested type is done using the given scale.
	 * \param ID the ID of the requested type (type of the source is used if not given)
	 * \param scaling the scaling to be used when converting the data (will be determined automatically if not given)
	 * \return a new deep copied Chunk of the same size
	 */
	Chunk copyByID( size_t ID, const scaling_pair &scaling = scaling_pair() )const;
	template<typename T> TypedChunk<T> as(const scaling_pair &scaling = scaling_pair())const;

	/**
	 * Copy data from a (smaller) chunk and insert it as a tile at a specified position.
	 * If the data would not fit at the given position (aka would got beyound the images size) it will be clipped and a warning will be sent if enabled.
	 * \note the size of the copied tile is defined by the size of the source. In other words the whole source is copied as a tile.
	 * \param src the source for the tile-data
	 * \param pos the position where to insert the data
	 * \param allow_capping if clipping is considered ok (aka switch of the warning about the copied tile being to big)
	 */
	void copyFromTile(const Chunk &src, std::array<size_t,4> pos, bool allow_capping=true);

	/**
	 * Copy a tile data to a (smaller) chunk.
	 * The size of the copied tile is defined by the size of the destination. In other words the whole destination is filled.
	 * If the data would not fit at the given position (aka the destination tile would got beyound the images size) it will be clipped and a warning will be sent if enabled.
	 * \param dst the destination for the tile-data
	 * \param pos the position where to insert the data
	 * \param allow_capping if clipping is considered ok (aka switch of the warning about the copied tile being to big)
	 */
	void copyTileTo(Chunk &dst, std::array<size_t,4> pos, bool allow_capping=false);

	size_t getBytesPerVoxel()const;

	/**
	 * Get a string describing the shape of the chunk.
	 * \param upper make the first letter uppercase
	 * \returns "slice" for 2D chunks
	 * \returns "volume" for 3D chunks
	 * \returns "volset" for 4D chunks
	 * \returns "chunk" otherwise
	 */
	std::string getShapeString( bool upper=false )const;

	template<typename T> bool is()const {return ValueArray::is<T>();}

	void copyRange( const std::array< size_t, 4 >& source_start, const std::array< size_t, 4 >& source_end, isis::data::Chunk& dst, const std::array< size_t, 4 >& destination=std::array< size_t, 4 >({0,0,0,0}) )const;

	size_t compare( const Chunk &dst )const;
	size_t compareRange( const std::array<size_t,4> &source_start, const std::array<size_t,4> &source_end, Chunk &dst, const std::array<size_t,4> &destination )const;

	/**
	 * Splices the chunk at the uppermost dimension and automatically sets indexOrigin and acquisitionNumber appropriately.
	 * This automatically selects the uppermost dimension of the chunk to be spliced and will compute the correct offsets
	 * for indexOrigin and acquisitionNumberOffset which will be applied to the resulting splices.
	 *
	 * E.g. autoSplice() on a chunk of the size 512x512x128, with rowVec 1,0,0, columnVec 0,1,0 and indexOrigin 0,0,0
	 * will result in 128 chunks of the size 512x512x1, with constant rowVec's 1,0,0, and columnVec's 0,1,0  while the indexOrigin will be going from 0,0,0 to 0,0,128
	 * (If voxelSize is 1,1,1 and voxelGap is 0,0,0). The acquisitionNumber will be reset to a simple incrementing counter starting at acquisitionNumberOffset.
	 * \attention As this will also move all properties into the "splinters" this chunk will be invalid afterwards
	 */
	std::list<Chunk> autoSplice( uint32_t acquisitionNumberStride = 0 )const;

	/**
	 * Splices the chunk at the given dimension and all dimensions above.
	 * E.g. spliceAt\(columnDim\) on a chunk of the size 512x512x128 will result in 512*128 chunks of the size 512x1x1
	 * \attention As this will also move alle properties into the "splinters" this chunk will be invalid afterwards
	 */
	std::list<Chunk> spliceAt(isis::data::dimensions atDim )const;

	/**
	  * Flips the chunk along a dimension dim in image space.
	  */
	void flipAlong( const dimensions dim );

	//http://en.wikipedia.org/wiki/In-place_matrix_transposition#Non-square_matrices%3a_Following_the_cycles
	void swapDim(unsigned short dim_a,unsigned short dim_b,std::shared_ptr<util::ProgressFeedback> feedback=std::shared_ptr<util::ProgressFeedback>());

	bool isValid()const{return ValueArray::isValid() && util::PropertyMap::isValid();}

	friend std::ostream &operator<<(std::ostream &os, const Chunk &chunk);
};

template<typename TYPE> class TypedChunk : public Chunk{
protected:
	const std::shared_ptr<TYPE> &me;
public:
	/**
	 * Run a function on every Voxel in the chunk.
	 * \note This always has writing access even if called from a const object. If you want it to be read-only make vox "const".
	 * \param v a reference to the voxel
	 * \param offset the position of the voxel inside the chunk
	 */
	void foreachVoxel( std::function<void(TYPE &vox, util::vector4<size_t> pos)> func )const 
	{
		auto vox_ptr = me.get();
		const util::vector4<size_t> imagesize = getSizeAsVector();
		util::vector4<size_t> pos;

		for( pos[timeDim] = 0; pos[timeDim] < imagesize[timeDim]; pos[timeDim]++ )
			for( pos[sliceDim] = 0; pos[sliceDim] < imagesize[sliceDim]; pos[sliceDim]++ )
				for( pos[columnDim] = 0; pos[columnDim] < imagesize[columnDim]; pos[columnDim]++ )
					for( pos[rowDim] = 0; pos[rowDim] < imagesize[rowDim]; pos[rowDim]++ ) {
						func( *( vox_ptr++ ), pos );
					}
	}
	void foreachVoxel( std::function<void(TYPE &vox)> func )const
	{
		std::for_each(me.get(),me.get()+getLength(),func);
	}

	//empty constructor making sure underlying ValueArray has correct type
	TypedChunk(): Chunk(ValueArray(std::add_pointer_t<TYPE>(), 0), 0, 0, 0, 0), me(castTo<TYPE>()){}
	
	TypedChunk( const TypedChunk<TYPE> &ref):TypedChunk(){
		//copy shape
		NDimensional<4>::operator=(ref);
		//copy properties
		util::PropertyMap::operator=(ref);
		//copy array
		ValueArray::operator=(ref.as<TYPE>());//will call std::shared_ptr<TYPE>::operator=() as underlying stored types are the same (and thus "me" will still be valid)
	}

	TypedChunk(const ValueArray &ref, size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1, bool fakeValid = false, const scaling_pair &scaling = scaling_pair()  )
	: Chunk( ref.as<TYPE>(scaling), nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps, fakeValid ),me(castTo<TYPE>()) {}
	
	TypedChunk( const Chunk &ref, scaling_pair scaling = scaling_pair() ) : TypedChunk( ref.as<TYPE>(scaling) ) {}
	
	TYPE* begin(){
		return me.get();
	}
	TYPE* end(){
		return me.get()+getLength();
	}
	const TYPE* begin()const{
		return me.get();
	}
	const TYPE* end()const{
		return me.get()+getLength();
	}
};

template<typename T> TypedChunk<T> Chunk::as(const scaling_pair &scaling)const
{
	//make a chunk from a typed ValueArray
	const auto size=getSizeAsVector();
	TypedChunk<T> ret(ValueArray::as<T>(scaling), size[0], size[1], size[2], size[3], false, scaling);
	//and copy properties over
	static_cast<util::PropertyMap&>(ret)=*this;
	return ret;
}


///// Chunk class for memory-based buffers
template<typename TYPE> class MemChunk : public TypedChunk<TYPE>
{
public:
 	MemChunk(const MemChunk<TYPE> &ref, const scaling_pair &scaling = scaling_pair()):MemChunk<TYPE>(static_cast<const Chunk&>(ref),scaling){}//enforce deep copy
 	//@todo get rid of overloading with concepts e.g. template<Chunklike T> MemChunk(const T &ref, const scaling_pair &scaling ...)
	
	/// Create a MemChunk with the given size
	MemChunk( size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1, bool fakeValid = false )
	:TypedChunk<TYPE>(
		ValueArray::make<TYPE>(nrOfColumns*nrOfRows*nrOfSlices*nrOfTimesteps),
		nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps,
		fakeValid,
		scaling_pair(1,0) // we just made that thing, no scaling needed
	)
	{}
	/**
	 * Create a MemChunk as copy of a given raw memory block
	 * This will create a MemChunk of the given size and fill it with the data at the given address.
	 * No range check will be done.
	 * An automatic conversion will be done if necessary.
	 * \param org pointer to the raw data which shall be copied
	 * \param nrOfColumns
	 * \param nrOfRows
	 * \param nrOfSlices
	 * \param nrOfTimesteps size of the resulting image
	 * \param fakeValid set all needed properties to usefull values to make the Chunk a valid one
	 */
	template<typename T> MemChunk( const T *const org, size_t nrOfColumns, size_t nrOfRows = 1, size_t nrOfSlices = 1, size_t nrOfTimesteps = 1, bool fakeValid = false  ):
	    MemChunk<TYPE>( nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps, fakeValid )
	{
		static_assert(util::knownType<T>(),"invalid type");
		this->copyFromMem( org, this->getVolume() );
	}
	/**
	 * Create a deep copy of a given Chunk.
	 * An automatic conversion is used if datatype does not fit
	 * \param ref the source chunk
	 * \param scaling the scaling (scale and offset) to be used if a conversion to the requested type is neccessary.
	 */
	MemChunk( const Chunk &ref, const scaling_pair &scaling = scaling_pair() )//initialize as an empty Chunk made of an empty ValueArray
	:TypedChunk<TYPE>(
		static_cast<const ValueArray&>(ref).copyByID(util::typeID<TYPE>(), scaling ),//deep copy data of ref
		ref.getSizeAsVector()[0],ref.getSizeAsVector()[1],ref.getSizeAsVector()[2],ref.getSizeAsVector()[3],//copy shape of ref
		false, //we're going to copy the properties from ref, no need to fake "it"
		scaling_pair(1,0)//we already scaled the data
	)
	{
		static_cast<util::PropertyMap&>(*this)=ref; // copy properties
	}
 	//@todo get rid of overloading with concepts e.g. template<Chunklike T> operator=(const T &ref, const scaling_pair &scaling ...)
	MemChunk<TYPE> &operator=( const MemChunk<TYPE> &ref ){return operator=(static_cast<const Chunk&>(ref));}
	MemChunk<TYPE> &operator=( const TypedChunk<TYPE> &ref ){return operator=(static_cast<const Chunk&>(ref));}
	MemChunk<TYPE> &operator=( const Chunk &ref ){
		//copy shape
		NDimensional<4>::operator=(ref);
		//copy properties
		util::PropertyMap::operator=(ref);
		//copy data ()
		ValueArray newdata=ref.copyByID(util::typeID<TYPE>());
		assert(newdata.is<TYPE>());
		ValueArray::operator=(std::move(newdata));//will call std::shared_ptr<TYPE>::operator=() as underlying stored types are the same (and thus "me" will still be valid)
		return *this;
	}
};

}
