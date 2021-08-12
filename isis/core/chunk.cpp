//
// C++ Implementation: chunk
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "chunk.hpp"

namespace isis
{
namespace data
{
/// @cond _internal
/// @endcond _internal

Chunk::Chunk(bool fakeValid )
{
	util::Singletons::get<NeededsList<Chunk>, 0>().applyTo( *this );
	if( fakeValid ) {
		setValueAs( "indexOrigin", util::fvector3() );
		setValueAs( "acquisitionNumber", 0 );
		setValueAs( "voxelSize", util::fvector3({ 1, 1, 1 }) );
		setValueAs( "rowVec", util::fvector3({1, 0} ));
		setValueAs( "columnVec", util::fvector3({0, 1}) );
		setValueAs( "sequenceNumber", ( uint16_t )0 );
	}
}

Chunk::Chunk(const ValueArray &src, size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps, bool fakeValid): Chunk(fakeValid)
{
	const util::vector4<size_t> init_size={nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps};
	init( init_size );
	if(!(src.isValid() || ValueArray::getLength())){//creation of a zero-sized chunk from a zero sized Array was probably intentional
		LOG(Debug,info) << "Creating a zero sized Chunk. Make sure you change that before using it";
	} else {
		LOG_IF(!src.isValid(),Runtime,error) << "Creating a chunk from an invalid ValueArray, that's not going to end well ...";
		LOG_IF( NDimensional<4>::getVolume() == 0, Debug, warning ) << "Size " << init_size << " is invalid";
	}
	ValueArray::operator=(src);
	assert(ValueArray::getLength() == getVolume() );
}

Chunk Chunk::cloneToNew( size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps )const
{
	return createByID( getTypeID(), nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps );
}

Chunk Chunk::createByID ( size_t ID, size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps, bool fakeValid )
{
	const util::vector4<size_t> newSize( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} );
	assert( util::product(newSize) != 0);
	const ValueArray created(ValueArray::createByID(ID, util::product(newSize) ) );
	return  Chunk( created, nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps, fakeValid );
}

bool Chunk::convertToType( size_t ID, const scaling_pair &scaling )
{
	//get a converted ValueArray (will be a cheap copy if no conv was needed)
	ValueArray newPtr = ValueArray::convertByID(ID, scaling );

	if( newPtr.isValid() ){ // if the conversion is ok
		ValueArray::operator=(newPtr);
		return true;
	} else
		return false;
}

Chunk Chunk::copyByID( size_t ID, const scaling_pair &scaling ) const
{
	Chunk ret = *this; //make copy of the chunk
	static_cast<ValueArray&>( ret ) = ValueArray::copyByID(ID, scaling ); // replace its data by the copy
	return ret;
}

void Chunk::copyFromTile(const Chunk &src, std::array<size_t,4> pos, bool allow_capping){
	auto tilesize=src.getSizeAsVector(), size=getSizeAsVector();
	size_t scanline_width=std::min(tilesize[rowDim],size[rowDim]-pos[0]);

	LOG_IF(scanline_width<tilesize[rowDim] && !allow_capping, Runtime, warning)
	    << "Capping tile of size " << src.getSizeAsString() << " as putting it at " << pos << " in image of size " << getSizeAsString();


	for(size_t l=0;l<std::min(tilesize[1],size[1]-pos[1]);l++){
		size_t dst_scanline_idx= getLinearIndex({pos[0],pos[1]+l,0,0});
		size_t src_scanline_idx= src.getLinearIndex({0,l,0,0});
		static_cast<const ValueArray&>( src ).copyRange(src_scanline_idx, src_scanline_idx+scanline_width-1, *this, dst_scanline_idx);
	}
}
void Chunk::copyTileTo(Chunk &dst, std::array<size_t,4> pos, bool allow_capping){
	auto tilesize=dst.getSizeAsVector(), size=getSizeAsVector();
	size_t scanline_width=std::min(tilesize[rowDim],size[rowDim]-pos[0]);

	LOG_IF(tilesize[rowDim]<scanline_width && !allow_capping, Runtime, warning)
	    << "Capping tile of size " << dst.getSizeAsString() << " as putting it at " << pos << " in image of size " << getSizeAsString();


	for(size_t l=0;l<std::min(tilesize[1],size[1]-pos[1]);l++){
		size_t dst_scanline_idx= dst.getLinearIndex({0,l,0,0});
		size_t src_scanline_idx= getLinearIndex({pos[0],pos[1]+l,0,0});
		ValueArray::copyRange(src_scanline_idx, src_scanline_idx+scanline_width-1, dst, dst_scanline_idx);
	}
}

size_t Chunk::getBytesPerVoxel()const
{
	return bytesPerElem();
}

std::string Chunk::getShapeString(bool upper) const
{
	switch(getRelevantDims()){
	    case 2:return upper ? "Slice":"slice";
	    case 3:return upper ? "Volume":"volume";
	    case 4:return upper ? "Volset":"volset";
	}
	return upper ? "Chunk":"chunk";
}

void Chunk::copySlice( size_t thirdDimS, size_t fourthDimS, Chunk &dst, size_t thirdDimD, size_t fourthDimD ) const
{
	copyRange(
	    {0, 0, thirdDimS, fourthDimS},
	    {getSizeAsVector()[0] - 1, getSizeAsVector()[1] - 1, thirdDimS, fourthDimS},
	    dst,
	    {0, 0, thirdDimD, fourthDimD}
	);
}

void Chunk::copyRange( const std::array<size_t,4> &source_start, const std::array<size_t,4> &source_end, Chunk &dst, const std::array<size_t,4> &destination ) const
{
	LOG_IF( ! isInRange( source_start ), Debug, error )
	        << "Copy start " << util::vector4<size_t>( source_start )
	        << " is out of range (" << getSizeAsString() << ") at the source chunk";
	LOG_IF( ! isInRange( source_end ), Debug, error )
	        << "Copy end " << util::vector4<size_t>( source_end )
	        << " is out of range (" << getSizeAsString() << ") at the source chunk";
	LOG_IF( ! dst.isInRange( destination ), Debug, error )
	        << "Index " << util::vector4<size_t>( destination )
	        << " is out of range (" << getSizeAsString() << ") at the destination chunk";
	const size_t sstart = getLinearIndex( source_start );
	const size_t send = getLinearIndex( source_end );
	const size_t dstart = dst.getLinearIndex( destination );
	ValueArray::copyRange(sstart, send, dst, dstart );
}

size_t Chunk::compareRange( const std::array<size_t,4> &source_start, const std::array<size_t,4> &source_end, Chunk &dst, const std::array<size_t,4> &destination ) const
{
	LOG_IF( ! isInRange( source_start ), Debug, error )
	        << "memcmp start " << util::vector4<size_t>( source_start )
	        << " is out of range (" << getSizeAsString() << ") at the first chunk";
	LOG_IF( ! isInRange( source_end ), Debug, error )
	        << "memcmp end " << util::vector4<size_t>( source_end )
	        << " is out of range (" << getSizeAsString() << ") at the first chunk";
	LOG_IF( ! dst.isInRange( destination ), Debug, error )
	        << "Index " << util::vector4<size_t>( destination )
	        << " is out of range (" << getSizeAsString() << ") at the second chunk";
	LOG( Debug, verbose_info )
	        << "Comparing range from " << util::vector4<size_t>( source_start ) << " to " << util::vector4<size_t>( source_end )
	        << " and " << util::vector4<size_t>( destination );
	const size_t sstart = getLinearIndex( source_start );
	const size_t send = getLinearIndex( source_end );
	const size_t dstart = dst.getLinearIndex( destination );
	return ValueArray::compare(sstart, send, dst, dstart );
}
size_t Chunk::compare( const isis::data::Chunk &dst ) const
{
	if( getSizeAsVector() == dst.getSizeAsVector() )
		return ValueArray::compare(0, getVolume() - 1, dst, 0 );
	else
		return std::max( getVolume(), dst.getVolume() );
}

std::list<Chunk> Chunk::autoSplice ()const{
	return autoSplice(static_cast<dimensions>(getRelevantDims()-1));
}
std::list<Chunk> Chunk::autoSplice ( dimensions atDim )const
{
	if ( !PropertyMap::isValid() ) {
		LOG( Runtime, error ) << "Cannot spliceAt invalid Chunk (missing properties are " << this->getMissing() << ")";
		return {};
	}

	const auto distance = getValueAs<util::fvector3>( "voxelSize" ) + getValueAsOr( "voxelGap",util::fvector3());

	LOG_IF( atDim < data::timeDim && distance[atDim] == 0, Runtime, error ) << "The voxel distance (voxelSize + voxelGap) at the splicing direction (" << atDim << ") is zero. This will likely cause errors in the Images structure.";

	const auto orgsize=getSizeAsVector();

	util::PropertyMap spliceMap = *this;
	auto &indexOrigin = spliceMap.touchProperty("indexOrigin");
	auto &acquisitionNumber = spliceMap.touchProperty("acquisitionNumber");
	if(indexOrigin.isEmpty()){
		indexOrigin = util::dvector3{0,0,0};
		LOG(Runtime,error) << "Trying to splice a chunk without indexOrigin. Assuming " << indexOrigin;
	}
	if(acquisitionNumber.isEmpty()){
		acquisitionNumber = 0u;
		LOG(Runtime,error) << "Trying to splice a chunk without acquisitionNumber. Assuming " << acquisitionNumber;
	}

	std::vector vecs{
		getValueAs<util::dvector3>("rowVec"),
		getValueAs<util::dvector3>("columnVec")
	};
	if(auto svec=queryProperty( "sliceVec" ); svec ) {
		vecs.push_back(svec->as<util::dvector3>());
	} else { // if sliceVec isn't given, compute it as Normal on rowVec and columnVec
		assert( util::fuzzyEqual<float>( util::sqlen(vecs[0]), 1 ) );
		assert( util::fuzzyEqual<float>( util::sqlen(vecs[1]), 1 ) );
		vecs.push_back({
			vecs[0][1] * vecs[1][2] - vecs[0][2] * vecs[1][1],
			vecs[0][2] * vecs[1][0] - vecs[0][0] * vecs[1][2],
			vecs[0][0] * vecs[1][1] - vecs[0][1] * vecs[1][0]
		});
	}

	// go through each to-splice dimension and explode indexOrigin and acquisitionNumber with the size of the given
	// dimension if they aren't already
	size_t stepsize=1;
	for(auto dim=getRelevantDims()-1;dim>=atDim;--dim){
		stepsize*=orgsize[dim];//expected size of the property grow with every processed dimension by this dimension
		if(indexOrigin.size()<stepsize){
			LOG_IF(stepsize%indexOrigin.size(),Runtime,error)
			<< "The splicing stepsize " << stepsize
			<< " is not a multiple of the amount of values in indexOrigin ("
			<< indexOrigin.size() << "). This splice is most likely going to fail.";
			if(dim<timeDim){
				auto offset = vecs[dim] * distance[dim]; //direction and length to the next splice
				auto op = [offset,dimsize=orgsize[dim], cnt=0](const util::Value &v)mutable->util::Value{
					return v.as<util::dvector3>() + offset * (cnt++ % dimsize);
				};
				LOG(Debug,verbose_info) << "Stretching indexOrigin " << indexOrigin << " by " << orgsize[dim];
				indexOrigin.explode(orgsize[dim],op);
			} else // on timeDim indexOrigin is constant
				indexOrigin.explode(orgsize[dim],[](const util::Value &v){return v;});
			assert(indexOrigin.size()==stepsize);
		}
		if(acquisitionNumber.size()<stepsize){
			LOG_IF(stepsize%acquisitionNumber.size(),Runtime,error)
				<< "The splicing stepsize " << stepsize
				<< " is not a multiple of the amount of values in acquisitionNumber ("
				<< acquisitionNumber.size() << "). This splice is most likely going to fail.";
			auto op = [dimsize=orgsize[dim], cnt=0](const util::Value &v)mutable->util::Value{
				const auto iv = v.as<uint32_t>();
				return util::Value(iv * dimsize + (cnt++ % dimsize));
			};
			LOG(Debug,verbose_info) << "Stretching acquisitionNumber " << acquisitionNumber << " by " << orgsize[dim];
			acquisitionNumber.explode(orgsize[dim],op);
			assert(acquisitionNumber.size()==stepsize);
		}
	}

	LOG(Debug, info ) << "Splicing chunk at dimension " << atDim + 1;
	LOG(Debug, info) << "indexOrigin-list is " << indexOrigin;
	LOG(Debug,info) << "acquisitionNumber list is " << acquisitionNumber;

	// do the actual splicing with the modified PropertyMap
	return spliceAt(atDim,std::move(spliceMap));
}

std::list<Chunk> Chunk::spliceAt (dimensions atDim )const{
	return Chunk::spliceAt(atDim,util::PropertyMap(*this));
}
std::list<Chunk> Chunk::spliceAt (dimensions atDim, util::PropertyMap &&propSource )const
{
	std::list<Chunk> ret;

	//@todo should be locking
	const std::array<size_t, dims> wholesize = getSizeAsVector();
	std::array<size_t, dims> spliceSize{1,1,1,1};
	//copy the relevant dimensional sizes from wholesize (in case of sliceDim we copy only the first two elements of wholesize - making slices)
	std::copy(std::begin(wholesize),std::begin(wholesize)+atDim,std::begin(spliceSize));
	//get the spliced ValueArray's (the volume of the requested dims is the split-size - in case of sliceDim it is rows*columns)
	const auto pointers = ValueArray::splice(util::product(spliceSize) );
	assert(pointers.size()==util::product(wholesize)/util::product(spliceSize));

	//create new Chunks from this ValueArray's
	for( const ValueArray &ref :  pointers ) {
		ret.push_back( Chunk( ref, spliceSize[0], spliceSize[1], spliceSize[2], spliceSize[3] ) );
	}
	propSource.splice(ret.begin(),ret.end(),false);//copy/spliceAt properties into spliced chunks / this will empty propSource
	return ret;
}

size_t Chunk::useCount() const
{
	return ValueArray::useCount();
}

void Chunk::flipAlong( const dimensions dim )
{
	const size_t elSize = getBytesPerVoxel();
	const util::vector4<size_t> whole_size = getSizeAsVector();

	auto swap_ptr = std::static_pointer_cast<uint8_t>( getRawAddress() );
	uint8_t *swap_start = swap_ptr.get();
	const uint8_t *const swap_end = swap_start + util::product(whole_size) * elSize;

	size_t block_volume = util::product(whole_size);

	for( int i = data::timeDim; i >= dim; i-- ) {
		assert( ( block_volume % whole_size[i] ) == 0 );
		block_volume /= whole_size[i];
	}

	assert( block_volume );
	block_volume *= elSize;
	const size_t swap_volume = block_volume * whole_size[dim];
	uint8_t buff[ block_volume ];

	//iterate over all swap-volumes
	for( ; swap_start < swap_end; swap_start += swap_volume ) { //outer loop
		// swap each block with the one at the oppsite end of the swap_volume
		uint8_t *a = swap_start; //first block
		uint8_t *b = swap_start + swap_volume - block_volume; //last block within the swap-volume

		for( ; a < b; a += block_volume, b -= block_volume ) { // grow a, shrink b (inner loop)
			memcpy( buff, a, block_volume );
			memcpy( a, b, block_volume );
			memcpy( b, buff, block_volume );
		}

	}
}

void Chunk::swapDim( unsigned short dim_a,unsigned short dim_b, std::shared_ptr<util::ProgressFeedback> feedback)
{
	NDimensional<4>::swapDim(dim_a,dim_b,begin(),feedback);
	//swap voxel sizes
	auto voxel_size_query=queryValueAs<util::fvector3>("voxelSize");
	if(voxel_size_query && dim_a<data::timeDim && dim_b<data::timeDim){
		util::fvector3 &voxel_size=*voxel_size_query;
		std::swap(voxel_size[dim_a],voxel_size[dim_b]);
	}
}

const util::Value Chunk::getVoxelValue (size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps ) const
{
	LOG_IF(!isInRange( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} ), Debug, isis::error )
	    << "Index " << util::vector4<size_t>( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} ) << nrOfTimesteps
	    << " is out of range (" << getSizeAsString() << ")";

	const auto iter=begin();
	return util::Value(iter[getLinearIndex({nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} )]);
}
void Chunk::setVoxelValue (const util::Value &val, size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps )
{
	LOG_IF(!isInRange( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} ), Debug, isis::error )
	        << "Index " << util::vector4<size_t>( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} ) << nrOfTimesteps
	        << " is out of range (" << getSizeAsString() << ")";

	begin()[getLinearIndex( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} )] = val;
}


}
}
