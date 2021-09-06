//
// C++ Interface: image
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

#include "chunk.hpp"

#include <set>
#include <memory>
#include <vector>
#include <stack>
#include "sortedchunklist.hpp"
#include "common.hpp"
#include "progressfeedback.hpp"
#include "image_iterator.hpp"
#include "valuearray_typed.hpp"

namespace isis
{
namespace data
{
/// Main class for generic 4D-images
class Image : public NDimensional<4>, public util::PropertyMap
{
	dimensions minIndexingDim;
public:
	/**
	 * Enforce indexing to start at a given dimension.
	 * Normally indexing starts at the dimensionality of the inserted chunks.
	 * So, an Image of 2D-Chunks (slices) will start indexing at the 3rd dimension.
	 * If the dimension given here is bigger than the dimensionality of the chunks reindexing will override that and start indexing at the given dimension.
	 * E.g. setIndexingDim(timeDim) will enforce indexing of a Image of 10 30x30-slices at the time dimension resulting in an 30x30x1x10 image instead of an 30x30x10x1 image.
	 * If the indexing dimension is set after the Image was indexed it will be indexed again.
	 * \param d the minimal indexing dimension to be used
	 */
	void setIndexingDim ( dimensions d = rowDim );
	enum orientation {axial, reversed_axial, sagittal, reversed_sagittal, coronal, reversed_coronal};

	typedef _internal::ImageIteratorTemplate<ValueArray, false> iterator;
	typedef _internal::ImageIteratorTemplate<ValueArray, true> const_iterator;
	typedef iterator::reference reference;
	typedef const_iterator::reference const_reference;
	static std::list<PropPath> neededProperties;
protected:
	_internal::SortedChunkList set;
	std::vector<std::shared_ptr<Chunk> > lookup;
private:
	size_t chunkVolume;

	void deduplicateProperties();

	/**
	 * Get the pointer to the chunk in the internal lookup-table at position at.
	 * The Chunk will only have metadata which are unique to it - so it might be invalid
	 * (run join on it using the image as parameter to insert all non-unique-metadata).
	 */
	const std::shared_ptr<Chunk> &chunkPtrAt ( size_t at ) const;

	/**
	 * Computes chunk- and voxel- indices.
	 * The returned chunk-index applies to the lookup-table (chunkAt), and the voxel-index to this chunk.
	 * Behaviour will be undefined if:
	 * - the image is not clean (not indexed)
	 * - the image is empty
	 * - the coordinates are not in the image
	 *
	 * Additionally an error will be sent if Debug is enabled.
	 * \returns a std::pair\<chunk-index,voxel-index\>
	 */
	inline std::pair<size_t, size_t> commonGet ( size_t first, size_t second, size_t third, size_t fourth ) const {
		LOG_IF ( ! clean, Debug, error )
		        << "Getting data from a non indexed image will result in undefined behavior. Run reIndex first.";
		LOG_IF ( set.isEmpty(), Debug, error )
		        << "Getting data from a empty image will result in undefined behavior.";
		LOG_IF ( !isInRange ( {first, second, third, fourth} ), Debug, isis::error )
		        << "Index " << util::vector4<size_t> ( {first, second, third, fourth} ) << " is out of range (" << getSizeAsString() << ")";
		const size_t index = getLinearIndex ( {first, second, third, fourth} );
		return std::make_pair ( index / chunkVolume, index % chunkVolume );
	}


protected:
	bool clean;
	static std::list<isis::util::PropertyMap::PropPath> defaultChunkEqualitySet;

	/**
	 * Search for a dimensional break in all stored chunks.
	 * This function searches for two chunks whose (geometrical) distance is more than twice
	 * the distance between the first and the second chunk. It wll assume a dimensional break
	 * at this position.
	 *
	 * Normally chunks are beneath each other (like characters in a text) so their distance is
	 * more or less constant. But if there is a dimensional break (analogous to the linebreak
	 * in a text) the distance between this particular chunks/characters is bigger than twice
	 * the normal distance
	 *
	 * For example for an image of 2D-chunks (slices) getChunkStride(1) will
	 * get the number of slices (size of third dim) and  getChunkStride(slices)
	 * will get the number of timesteps
	 * \param base_stride the base_stride for the iteration between chunks (1 for the first
	 * dimension, one "line" for the second and soon...)
	 * \returns the length of this chunk-"line" / the stride
	 */
	size_t getChunkStride ( size_t base_stride = 1 );
	/**
	 * Access a chunk via index (and the lookup table)
	 * The Chunk will only have metadata which are unique to it - so it might be invalid
	 * (run join on it using the image as parameter to insert all non-unique-metadata).
	 */
	Chunk &chunkAt ( size_t at );
	/// Creates an empty Image object.
	Image();
	std::pair<ValueArray, ValueArray> getChunksMinMax()const;
public:
	/**
	 * Copy constructor.
	 * Copies all elements, only the voxel-data (in the chunks) are referenced.
	 */
	Image ( const Image &ref );

	/**
	 * Create image from a list of Chunks or objects with the base Chunk.
	 * Removes used chunks from the given list. So afterwards the list consists of the rejected chunks.
	 */
	//@todo why template
	//@todo unify with concepts (or std::span)
	template<typename T> Image ( std::list<T> &chunks, dimensions min_dim = rowDim, util::slist* rejected=nullptr ) : Image()
	{
		minIndexingDim = min_dim;
		insertChunksFromList ( chunks, rejected );
	}
	/**
	 * Create image from a vector of Chunks or objects with the base Chunk.
	 * Removes used chunks from the given list. So afterwards the list consists of the rejected chunks.
	 */
	template<typename T> Image ( std::vector<T> &chunks, dimensions min_dim = rowDim,util::slist* rejected=nullptr  ) : Image()
	{
		minIndexingDim = min_dim;
		std::list<T> tmp( chunks.begin(), chunks.end() );
		insertChunksFromList ( tmp, rejected );
		chunks.assign( tmp.begin(), tmp.end() );
	}

	/**
	 * Insert Chunks or objects with the base Chunk from a sequence container into the Image.
	 * Removes used chunks from the given sequence container. So afterwards the container consists of the rejected chunks.
	 * \returns amount of successfully inserted chunks
	 */
	template<typename T> size_t insertChunksFromList ( std::list<T> &chunks, util::slist* rejected=nullptr )
	{
		static_assert( std::is_base_of<Chunk, T>::value, "Can only insert objects derived from Chunks" );
		size_t cnt = 0;

		for ( typename std::list<T>::iterator i = chunks.begin(); i != chunks.end(); ) { // for all remaining chunks
			if ( insertChunk ( *i ) ) {
				chunks.erase ( i++ );
				cnt++;
			} else {
				i++;
			}
		}

		if ( ! isEmpty() ) {
			LOG ( Debug, info ) << "Reindexing image with " << cnt << " chunks.";

			if ( !reIndex(rejected) ) {
				LOG ( Runtime, error ) << "Failed to create image from " << cnt << " chunks.";
			} else {
				LOG_IF ( !getMissing().empty(), Debug, warning )
				        << "The created image is missing some properties: " << util::MSubject( getMissing() ) << ". It will be invalid.";
			}
		} else {
			LOG ( Debug, warning ) << "Image is empty after inserting chunks.";
		}

		return cnt;
	}

	/**
	 * Create image from a single chunk.
	 */
	Image ( const Chunk &chunk, dimensions min_dim = rowDim );

	/**
	 * Copy operator.
	 * Copies all elements, only the voxel-data (in the chunks) are referenced.
	 */
	Image &operator= ( const Image &ref );

	bool checkMakeClean();
	bool isClean() const;
	/**
	 * This method returns a reference to the voxel value at the given coordinates.
	 *
	 * The voxel reference provides reading and writing access to the refered
	 * value.
	 *
	 * If the image is not clean, reIndex will be run.
	 * If the requested voxel is not of type T, an error will be raised.
	 *
	 * \param first The first coordinate in voxel space. Usually the x value / the read-encoded position..
	 * \param second The second coordinate in voxel space. Usually the y value / the column-encoded position.
	 * \param third The third coordinate in voxel space. Ususally the z value / the time-encoded position.
	 * \param fourth The fourth coordinate in voxel space. Usually the time value.
	 *
	 * \returns A reference to the addressed voxel value. Reading and writing access
	 * is provided.
	 */
	template <typename T> T &voxel ( size_t first, size_t second = 0, size_t third = 0, size_t fourth = 0 )
	{
		checkMakeClean();
		static_assert(util::knownType<T>());
		const std::pair<size_t, size_t> index = commonGet ( first, second, third, fourth );
		auto &data = chunkAt ( index.first ).castTo<T>();
		return *(data.get()+index.second);
	}

	void swapDim( unsigned short dim_a, unsigned short dim_b, std::shared_ptr<util::ProgressFeedback> feedback=std::shared_ptr<util::ProgressFeedback>() );

	/**
	 * Get a const reference to the voxel value at the given coordinates.
	 *
	 * \param first The first coordinate in voxel space. Usually the x value / the read-encoded position..
	 * \param second The second coordinate in voxel space. Usually the y value / the column-encoded position.
	 * \param third The third coordinate in voxel space. Ususally the z value / the time-encoded position.
	 * \param fourth The fourth coordinate in voxel space. Usually the time value.
	 *
	 * If the requested voxel is not of type T, an error will be raised.
	 *
	 * \returns A reference to the addressed voxel value. Only reading access is provided
	 */
	template <typename T> const T &voxel ( size_t first, size_t second = 0, size_t third = 0, size_t fourth = 0 ) const
	{
		LOG_IF(!clean,  Debug, warning )  << "Accessing voxels of a not-clean image. Pleas run reIndex first";

		static_assert(util::knownType<T,ArrayTypes>());
		const std::pair<size_t, size_t> index = commonGet ( first, second, third, fourth );
		const auto &data = chunkPtrAt ( index.first )->castTo<T>();
		return data.get()+index.second;
	}

	const util::Value getVoxelValue (size_t nrOfColumns, size_t nrOfRows = 0, size_t nrOfSlices = 0, size_t nrOfTimesteps = 0 ) const;
	void setVoxelValue (const util::Value &val, size_t nrOfColumns, size_t nrOfRows = 0, size_t nrOfSlices = 0, size_t nrOfTimesteps = 0 );

	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;

	/**
	 * Get the type of the chunk with "biggest" type.
	 * Determines the minimum and maximum of the image, (and with that the types of these limits).
	 * If they are not the same, the type which can store the other type is selected.
	 * E.g. if min is "-5(int8_t)" and max is "1000(int16_t)" "int16_t" is selected.
	 * Warning1: this will fail if min is "-5(int8_t)" and max is "70000(uint16_t)"
	 * Warning2: the cost of this is O(n) while Chunk::getTypeID is O(1) - so do not use it in loops
	 * Warning3: the result is not exact - so never use it to determine the type for Image::voxel (Use TypedImage to get an image with an guaranteed type)
	 * \returns a number which is equal to the ValueArray::staticID() of the selected type.
	 */
	size_t getMajorTypeID() const;
	/// \returns the typename correspondig to the result of typeID
	std::string getMajorTypeName() const;

	/**
	 * Get a chunk via index (and the lookup table).
	 * The returned chunk will be a cheap copy of the original chunk.
	 * If copy_metadata is true the metadata of the image is copied into the chunk.
	 */
	Chunk getChunkAt ( size_t at, bool copy_metadata = true ) const;

	/**
	 * Get the chunk that contains the voxel at the given coordinates.
	 *
	 * If the image is not clean, behaviour is undefined. (See Image::commonGet).
	 *
	 * \param first The first coordinate in voxel space. Usually the x value / the read-encoded position.
	 * \param second The second coordinate in voxel space. Usually the y value / the column-encoded position.
	 * \param third The third coordinate in voxel space. Ususally the z value / the slice-encoded position.
	 * \param fourth The fourth coordinate in voxel space. Usually the time value.
	 * \param copy_metadata if true the metadata of the image are merged into the returned chunk
	 * \returns a copy of the chunk that contains the voxel at the given coordinates.
	 * (Reminder: Chunk-copies are cheap, so the image data are NOT copied but referenced)
	 */
	const Chunk getChunk ( size_t first, size_t second = 0, size_t third = 0, size_t fourth = 0, bool copy_metadata = true ) const;

	/**
	 * Get the chunk that contains the voxel at the given coordinates.
	 * If the image is not clean Image::reIndex() will be run.
	 *
	 * \param first The first coordinate in voxel space. Usually the x value.
	 * \param second The second coordinate in voxel space. Usually the y value.
	 * \param third The third coordinate in voxel space. Ususally the z value.
	 * \param fourth The fourth coordinate in voxel space. Usually the time value.
	 * \param copy_metadata if true the metadata of the image are merged into the returned chunk
	 * \returns a copy of the chunk that contains the voxel at the given coordinates.
	 * (Reminder: Chunk-copies are cheap, so the image data are NOT copied but referenced)
	 */
	Chunk getChunk ( size_t first, size_t second = 0, size_t third = 0, size_t fourth = 0, bool copy_metadata = true );

	/**
	 * Get the chunk that contains the voxel at the given coordinates in the given type.
	 * If the accordant chunk has type T a cheap copy is returned.
	 * Otherwise a MemChunk-copy of the requested type is created from it.
	 * In this case the minimum and maximum values of the image are computed and used for the MemChunk constructor.
	 *
	 * \param first The first coordinate in voxel space. Usually the x value.
	 * \param second The second coordinate in voxel space. Usually the y value.
	 * \param third The third coordinate in voxel space. Ususally the z value.
	 * \param fourth The fourth coordinate in voxel space. Usually the time value.
	 * \param copy_metadata if true the metadata of the image are merged into the returned chunk
	 * \returns a (maybe converted) chunk containing the voxel value at the given coordinates.
	 */
	template<typename TYPE> TypedChunk<TYPE> getChunkAs ( size_t first, size_t second = 0, size_t third = 0, size_t fourth = 0, bool copy_metadata = true ) const {
		return getChunkAs<TYPE> ( getScalingTo ( util::typeID<TYPE>() ), first, second, third, fourth, copy_metadata );
	}
	/**
	 * Get the chunk that contains the voxel at the given coordinates in the given type (fast version).
	 * \copydetails getChunkAs
	 * This version does not compute the scaling, and thus is much faster.
	 * \param scaling the scaling (scale and offset) to be used if a conversion to the requested type is neccessary.
	 * \param first The first coordinate in voxel space. Usually the x value.
	 * \param second The second coordinate in voxel space. Usually the y value.
	 * \param third The third coordinate in voxel space. Ususally the z value.
	 * \param fourth The fourth coordinate in voxel space. Usually the time value.
	 * \param copy_metadata if true the metadata of the image are merged into the returned chunk
	 * \returns a (maybe converted) chunk containing the voxel value at the given coordinates.
	 */
	template<typename TYPE> TypedChunk<TYPE> getChunkAs ( const scaling_pair &scaling, size_t first, size_t second = 0, size_t third = 0, size_t fourth = 0, bool copy_metadata = true ) const {
		const Chunk& chunk=getChunk ( first, second, third, fourth, copy_metadata );
		return chunk.as<TYPE>(scaling); //return that
	}

	///for each chunk get the scaling (and offset) which would be used in an conversion to the given type
	scaling_pair getScalingTo ( unsigned short typeID ) const;


	/**
	 * Insert a Chunk into the Image.
	 * The insertion is sorted and unique. So the Chunk will be inserted behind a geometrically "lower" Chunk if there is one.
	 * If there is allready a Chunk at the proposed position this Chunk wont be inserted.
	 *
	 * \param chunk The Chunk to be inserted
	 * \returns true if the Chunk was inserted, false otherwise.
	 */
	bool insertChunk ( const Chunk &chunk );
	/**
	 * (Re)computes the image layout and metadata.
	 * The image will be "clean" on success.
	 * \returns true if the image was successfully reindexed and is valid, false otherwise.
	 */
	bool reIndex(util::slist* rejected=nullptr);

	/// \returns true if there is no chunk in the image
	bool isEmpty() const;

	/**
	 * Get a list of the properties of the chunks for the given key.
	 * Retrieves the named property from each chunk and returnes them as a list.
	 * Not existing or empty properties will be inserted as empty properties. Thus the length of the list equals the amount of chunks.
	 * If unique is true those will be skipped. So the list might be shorter.
	 * \note Image properties (aka common properties) won't  be added
	 * \param key the name of the property to search for
	 * \param unique when true empty, non existing or consecutive duplicates won't be added
	 */
	std::list<util::PropertyValue> getChunksProperties ( const util::PropertyMap::PropPath &key, bool unique = false ) const;

	/**
	 * Get a list of the properties of the chunks for the given key.
	 * Retrieves the named property from each chunk and returnes them as a list.
	 * Not existing or empty properties will be inserted as empty properties. Thus the length of the list equals the amount of chunks.
	 * If unique is true those will be skipped. So the list might be shorter.
	 * \note Image properties (aka common properties) won't  be added
	 * \param key the name of the property to search for
	 * \param unique when true empty, non existing or consecutive duplicates won't be added
	 */
	template<typename T> std::list<T> getChunksValuesAs ( const util::PropertyMap::key_type &key, bool unique = false ) const{
		std::list<T> ret;

		if( clean ) {
			for( const std::shared_ptr<const Chunk> &ref :  lookup ) {
				const auto prop = ref->queryProperty( key );

				if(unique){ // if unique
					if( ( prop && !ret.empty() &&  *prop == ret.back() ) || // if there is prop, skip if its equal
					    !prop //if there is none skip anyway
					)
						continue;
				}
				ret.push_back(
				    prop ? prop->as<T>() : T()
				);
			}
		} else {
			LOG( Runtime, error ) << "Cannot get chunk-properties from non clean images. Run reIndex first";
		}

		return ret;
	}

	/**
	 * Get the size (in bytes) for the voxels in the image
	 * \warning As each Chunk of the image can have a different type (and thus bytesize),
	 * this does not neccesarly return the correct size for all voxels in the image. It
	 * will rather return the biggest size.
	 * \note The easiest (and most expensive) way to make sure you have the same bytesize accross the whole Image, is to run:
	 * \code
	 * image.convertToType(image.getMajorTypeID());
	 * \endcode
	 * assuming "image" is your image.
	 * \returns Get the biggest size (in bytes) accross all voxels in the image.
	 */
	size_t getMaxBytesPerVoxel() const;

	/**
	 * Get the maximum and the minimum voxel value of the image.
	 * The results are converted to T. If they don't fit an error ist send.
	 * \note If the datatype is a vector the minimum/maximum across all elements is computed
	 * \returns a pair of T storing the minimum and maximum values of the image.
	 */
	template<typename T> std::pair<T, T> getMinMaxAs() const {
		static_assert(util::knownType<T>(),"invalid type");// works only for T from _internal::types
		auto minmax = getMinMax();
		return std::make_pair ( minmax.first.as<T>(), minmax.second.as<T>() );
	}

	/**
	 * Get the maximum and the minimum voxel value of the image.
	 * The results are converted to T. If they don't fit an error ist send.
	 * \note If the datatype is a vector the minimum/maximum across all elements is computed
	 * \returns a pair storing the minimum and maximum Values of the image.
	 */
	std::pair<util::Value, util::Value> getMinMax(bool unify=true) const;

	/**
	 * Compares the voxel-values of this image to the given.
	 * \returns the amount of the different voxels
	 */
	size_t compare ( const Image &comp ) const;

	orientation getMainOrientation() const;

	/**
	 * Copy all voxel data of the image into memory.
	 * If neccessary a conversion into T is done using min/max of the image.
	 * \param dst c-pointer for the memory to copy into
	 * \param len the allocated size of that memory in elements
	 * \param scaling the scaling to be used when converting the data (will be determined automatically if not given)
	 */
	template<typename T> void copyToMem ( T *dst, size_t len) const {
		if ( clean ) {
			const scaling_pair scaling = getScalingTo ( util::typeName<T>() );

			// we could do this using convertToType - but this solution does not need any additional temporary memory
			for( const std::shared_ptr<Chunk> &ref :  lookup ) {
				const size_t cSize = ref->getVolume();

				if ( !ref->copyToMem<T> ( dst, len, scaling ) ) {
					LOG ( Runtime, error ) << "Failed to copy raw data of type " << ref->typeName() << " from image into memory of type " << util::typeName<T>();
				} else {
					if ( len < cSize ) {
						LOG ( Runtime, error ) << "Aborting copy, because there is no space left in the target";
						break;
					}

					len -= cSize;
				}

				dst += ref->getVolume(); // increment the cursor
			}
		} else {
			LOG ( Runtime, error ) << "Cannot copy from non clean images. Run reIndex first";
		}
	}

	/**
	 * Copy all voxel data into a new MemChunk.
	 * This creates a MemChunk of the requested type and the same size as the Image and then copies all voxeldata of the image into that Chunk.
	 *
	 * If neccessary a conversion into T is done using min/max of the image.
	 *
	 * Also the properties of the first chunk are \link util::PropertyMap::join join\endlink-ed with those of the image and copied.
	 * \note This is a deep copy, no data will be shared between the Image and the MemChunk. It will waste a lot of memory, use it wisely.
	 * \returns a MemChunk containing the voxeldata and the properties of the Image
	 */
	template<typename T> MemChunk<T> copyAsMemChunk() const {
		const util::vector4<size_t> size = getSizeAsVector();
		data::MemChunk<T> ret ( size[0], size[1], size[2], size[3] );
		copyToMem<T> ( &ret.template voxel<T>( 0, 0 ), ret.getVolume() );
		static_cast<util::PropertyMap &>( ret ) = static_cast<const util::PropertyMap &>( getChunkAt( 0 ) );
		return ret;
	}

	/**
	 * Copy all voxel data into a new ValueArray.
	 * This creates a ValueArray of the requested type and the same length as the images volume and then copies all voxeldata of the image into that ValueArray.
	 *
	 * If neccessary a conversion into T is done using min/max of the image.
	 * \note This is a deep copy, no data will be shared between the Image and the ValueArray. It will waste a lot of memory, use it wisely.
	 * \returns a ValueArray containing the voxeldata of the Image (but not its Properties)
	 */
	ValueArray copyAsValueArray() const; //@todo have as copyAsMemChunk too (maybe get rid of copyAsMemChunk, and maye MemChunk alltogether)
	/**
	 * Copy all voxel data of the image into an existing ValueArray using its type.
	 * If neccessary a conversion into the datatype of the target is done using min/max of the image.
	 * \param dst ValueArray to copy into
	 * \param scaling the scaling to be used when converting the data (will be determined automatically if not given)
	 */
	void copyToValueArray (data::ValueArray &dst, scaling_pair scaling = scaling_pair() ) const;

	/**
	 * Create a new Image of consisting of deep copied chunks.
	 * No conversion done, all chunks keep their type.
	 * \return a new deep copied Image of the same size
	 */
	Image copy()const;

	/**
	 * Create a new Image of consisting of deep copied chunks.
	 * If neccessary a conversion into the requested type is done using the given scale.
	 * \param ID the ID of the requested type (type of the respective source chunk is used if not given)
	 * \param scaling the scaling to be used when converting the data (will be determined automatically if not given)
	 * \return a new deep copied Image of the same size
	 */
	Image copyByID( unsigned short ID, const scaling_pair &scaling = scaling_pair() )const;


	/**
	* Get a sorted list of the chunks of the image.
	* \param copy_metadata set to false to prevent the metadata of the image to be copied into the results. This will improve performance, but the chunks may lack important properties.
	* \note These chunks will be cheap copies, so changing their voxels will change the voxels of the image. But you can for example use \code
	* std::vector< data::Chunk > cheapchunks=img.copyChunksToVector(); //this is a cheap copy
	* std::vector< data::MemChunk<float> > memchunks(cheapchunks.begin(),cheapchunks.end()); // this is not not
	* \endcode to get deep copies.
	*/
	std::vector<isis::data::Chunk> copyChunksToVector ( bool copy_metadata = true ) const;

	/**
	 * Ensure, the image has the type with the requested ID.
	 * If the typeID of any chunk is not equal to the requested ID, the data of the chunk is replaced by an converted version.
	 * The conversion is done using the value range of the image.
	 * \returns false if there was an error
	 */
	bool convertToType ( short unsigned int ID, scaling_pair scaling=scaling_pair() );

	/**
	 * Automatically spliceAt the given dimension and all dimensions above.
	 * e.g. spliceDownTo(sliceDim) will result in an image made of slices (aka 2d-chunks).
	 */
	size_t spliceDownTo ( dimensions dim );

	/// \returns the number of rows of the image
	size_t getNrOfRows() const;
	/// \returns the number of columns of the image
	size_t getNrOfColumns() const;
	/// \returns the number of slices of the image
	size_t getNrOfSlices() const;
	/// \returns the number of timesteps of the image
	size_t getNrOfTimesteps() const;

	util::fvector3 getFoV() const;

	/**
	 * Run a function on every Chunk in the image.
	 */
	void foreachChunk( std::function<void(Chunk &ch, util::vector4<size_t> posInImage)> func );
	void foreachChunk( std::function<void(Chunk &ch)> func );
	void foreachChunk( std::function<void(const Chunk &ch, util::vector4<size_t> posInImage)> func )const;
	void foreachChunk( std::function<void(const Chunk &ch)> func )const;

	/**
	 * Run a function on every Voxel in the image.
	 * This will try to instantiate func for all valid Chunk-datatypes.
	 * So this will only compile if func would be valid for all types.
	 * You might want to look into TypedImage::foreachVoxel.
	 * And remember TypedImage is a cheap copy if all chunks can be cheap-copied. So \code
	 * TypedImage<uint32_t>(img).foreachVoxel([]((uint32_t &vox, const util::vector4<size_t> &pos)){});
	 * \endcode is perfectly fine if the type of img is already uint32_t.
	 * \param v a reference to the voxel
	 * \param offset the position of the voxel inside the chunk
	 * \returns amount of operations which returned false - so 0 is good!
	 */
	template <typename TYPE> void foreachVoxel(std::function<void(TYPE &vox, util::vector4<size_t> pos)> func )const
	{
		foreachChunk([func](const Chunk &ch, util::vector4<size_t> posInImage){
			ch.foreachVoxel([posInImage,func](auto &vox, const util::vector4<size_t> &pos){//capsule the func, so we can add posInImage
				func(vox,pos+posInImage);
			});
		});
	}
	template <typename TYPE> void foreachVoxel(std::function<void(TYPE &vox)> func )const
	{
		foreachChunk([func](const Chunk &ch){
			ch.foreachVoxel(func);
		});
	}

	/**
	 * Generate a string identifying the image
	 * The identifier is made of
	 * - sequenceNumber
	 * - sequenceDescription if available
	 * - the common path of all chunk-sources (or the source file, if there is only one) if withpath is true
	 * - sequenceStart if available
	 * \param withpath add the common path of all sources to the identifying string
	 */
	std::string identify( bool withpath = true, bool withdate=true )const;
};

/**
 * An Image where all chunks are guaranteed to have a specific type.
 * This not necessarily means, that all chunks in this image are a deep copy of their origin.
 */
template<typename T> class TypedImage: public Image
{
protected:
	TypedImage ():Image(){}
public:
	using iterator=_internal::ImageIteratorTemplate<TypedArray<T>,false>;
	using const_iterator=_internal::ImageIteratorTemplate<TypedArray<T>,true>;
	using reference=typename iterator::reference;
	using const_reference=typename const_iterator::reference ;

	/// cheap copy another Image and make sure all chunks have type T
	TypedImage ( const Image &src, const scaling_pair &scaling=scaling_pair() ) : Image ( src ) { // ok we just copied the whole image
		//but we want it to be of type T
		bool result=convertToType ( util::typeID<T>(), scaling );
		LOG_IF(result==false, Runtime, error) << "Conversion to " << util::MSubject( util::typeName<T>() ) << " failed.";
	}
	/// cheap copy another TypedImage
	TypedImage &operator= ( const TypedImage &ref ) { //it's already of the given type - so just copy it
		Image::operator= ( ref );
		return *this;
	}
	/// cheap copy another Image and make sure all chunks have type T
	TypedImage &operator= ( const Image &ref ) { // copy the image, and make sure it's of the given type
		Image::operator= ( ref );
		convertToType ( util::typeID<T>() );
		return *this;
	}
	void copyToMem ( void *dst ) {
		Image::copyToMem<T> ( ( T * ) dst );
	}
	void copyToMem ( void *dst ) const {
		Image::copyToMem<T> ( ( T * ) dst );
	}
	iterator begin() {
		if ( !checkMakeClean() ) {
			LOG ( Debug, error )  << "Image is not clean. Returning empty iterator ...";
		}
		std::vector<TypedArray<T>> buff(lookup.size());
		for(size_t i=0;i<lookup.size();i++){
			assert(lookup[i]->template is<T>());//it's a typed image, so all chunks should be T
			buff[i]=*lookup[i];//there is a cheap-copy-constructor for TypedArray from ValueArray (if its actually T)
		}
		return iterator( buff );
	}
	iterator end() {
		return begin() + getVolume();
	};
	const_iterator begin() const {
		if ( !isClean() ) {
			LOG ( Debug, error )  << "Image is not clean. Returning invalid iterator ...";
		}
		std::vector<TypedArray<T>> buff(lookup.size());
		for(size_t i=0;i<lookup.size();i++){
			assert(lookup[i]->template is<T>());//it's a typed image, so all chunks should be T
			buff[i]=*lookup[i];//there is a cheap-copy-constructor for TypedArray from ValueArray (if its actually T)
			assert((float*)buff[i].begin()==lookup[i]->template beginTyped<T>()); //make sure it was a cheap copy
		}
		return const_iterator( buff );
	}
	const_iterator end() const {
		return begin() + getVolume();
	};

	/**
	 * Run a function on every Chunk in the image.
	 */
	void foreachChunk( std::function<void(TypedChunk<T> &ch, util::vector4<size_t> posInImage)> func )
	{
		checkMakeClean();
		const size_t chunkSize = lookup.front()->getVolume();
		size_t index=0;

		for(std::shared_ptr<Chunk> &ch:lookup){
			assert(ch->is<T>());
			func(*ch,getCoordsFromLinIndex(index));
			index+=chunkSize;
		}
	}
	void foreachChunk( std::function<void(TypedChunk<T> &ch)> func )
	{
		checkMakeClean();
		for(std::shared_ptr<Chunk> &ch:lookup){
			assert(ch->is<T>());
			func(*ch);
		}
	}
	void foreachChunk( std::function<void(const TypedChunk<T> &ch, util::vector4<size_t> posInImage)> func )const
	{
		if(clean){
			const size_t chunkSize = lookup.front()->getVolume();
			size_t index=0;
			for(const std::shared_ptr<Chunk> &ch:lookup){
				assert(ch->is<T>());
				func(*ch,getCoordsFromLinIndex(index));
				index+=chunkSize;
			}
		} else {
			LOG(Runtime,error) << "Trying to run foreachChunk on an unclean image. Won't do anything ..";
		}
	}
	void foreachChunk( std::function<void(const TypedChunk<T> &ch)> func )const
	{
		if(clean){
			for(const std::shared_ptr<Chunk> &ch:lookup){
				assert(ch->is<T>());
				func(*ch);
			}
		} else {
			LOG(Runtime,error) << "Trying to run foreachChunk on an unclean image. Won't do anything ..";
		}
	}

	/**
	 * Run a function on every Voxel in the chunk.
	 * \note This always has writing access even if called from a const object. If you want it to be read-only make vox "const".
	 * \param v a reference to the voxel
	 * \param offset the position of the voxel inside the chunk
	 */
	void foreachVoxel( std::function<void(T &vox, const util::vector4<size_t> &pos)> func )const
	{
		foreachChunk([func](const TypedChunk<T> &ch, util::vector4<size_t> posInImage){
			ch.foreachVoxel([posInImage,func](T &vox, const util::vector4<size_t> &pos){//capsule the func, so we can add posInImage
				func(vox,pos+posInImage);
			});
		});
	}
	void foreachVoxel( std::function<void(T &vox)> func )const
	{
		foreachChunk([func](const TypedChunk<T> &ch){
			ch.foreachVoxel(func);
		});
	}

};

/**
 * An Image which always uses its own memory and a specific type.
 * Thus, creating this image from another Image always does a deep copy (and maybe a conversion).
 */
template<typename T> class MemImage: public TypedImage<T>
{
public:
	/**
	 * Copy contructor.
	 * This makes a deep copy of the given image.
	 * The image data are converted to T if necessary.
	 */
	MemImage ( const Image &src ) {
		operator= ( src );
	}

	/**
	 * Copy operator.
	 * This makes a deep copy of the given image.
	 * The image data are converted to T if necessary.
	 */
	MemImage &operator= ( const Image &ref ) { // copy the image, and make sure it's of the given type

		Image::operator= ( ref ); // ok we just copied the whole image

		//we want deep copies of the chunks, and we want them to be of type T
		struct : _internal::SortedChunkList::chunkPtrOperator {
			scaling_pair scale;
			std::shared_ptr<Chunk> operator() ( const std::shared_ptr< Chunk >& ptr ) {
				return std::shared_ptr<Chunk> ( new MemChunk<T> ( *ptr, scale ) );
			}
		} conv_op;
		conv_op.scale = ref.getScalingTo ( util::typeID<T>() );
		LOG ( Debug, info ) << "Computed scaling for conversion from source image: [" << conv_op.scale << "]";

		this->set.transform ( conv_op );

		if ( ref.isClean() ) {
			this->lookup = this->set.getLookup(); // the lookup table still points to the old chunks
		} else {
			LOG ( Debug, info ) << "Copied unclean image. Running reIndex on the copy.";
			this->reIndex();
		}

		return *this;
	}
};

}
}


