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
	if(!(src.isValid() || ValueArray::getLength())){//creation of an zero-sized chunk from a zero sized Array was probably intentional
		LOG(Debug,info) << "Creating a zero sized Chunk. Make sure you change that before using it";
	} else {
		LOG_IF(!src.isValid(),Runtime,error) << "Creating a chunk from an invalid ValueArray, thats not going to end well ...";
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

std::list<Chunk> Chunk::autoSplice ( uint32_t acquisitionNumberStride )const
{
	if ( !PropertyMap::isValid() ) {
		LOG( Runtime, error ) << "Cannot spliceAt invalid Chunk (missing properties are " << this->getMissing() << ")";
		return std::list<Chunk>();
	}

	util::fvector3 offset;
	const auto voxelSize = getValueAs<util::fvector3>( "voxelSize" );
	const auto voxelGap = getValueAsOr( "voxelGap",util::fvector3());

	const util::fvector3 distance = voxelSize + voxelGap;
	const size_t atDim = getRelevantDims() - 1;

	LOG_IF( atDim < data::timeDim && distance[atDim] == 0, Runtime, error ) << "The voxel distance (voxelSize + voxelGap) at the splicing direction (" << atDim << ") is zero. This will likely cause errors in the Images structure.";

	switch( atDim ) { // init offset with the given direction
	case rowDim :
		offset = getValueAs<util::fvector3>( "rowVec" );
		break;
	case columnDim:
		offset = getValueAs<util::fvector3>( "columnVec" );
		break;
	case sliceDim:{
		const auto svec=queryProperty( "sliceVec" );
		if( svec ) {
			offset = svec->as<util::fvector3>();
		} else {
			const auto row = getValueAs<util::fvector3>( "rowVec" );
			const auto column = getValueAs<util::fvector3>( "columnVec" );
			assert( util::fuzzyEqual<float>( util::sqlen(row), 1 ) );
			assert( util::fuzzyEqual<float>( util::sqlen(column), 1 ) );
			offset[0] = row[1] * column[2] - row[2] * column[1];
			offset[1] = row[2] * column[0] - row[0] * column[2];
			offset[2] = row[0] * column[1] - row[1] * column[0];
		}
	}break;
	case timeDim :
		LOG_IF( acquisitionNumberStride == 0, Runtime, error ) << "Splicing at timeDim without acquisitionNumberStride will very likely make the next reIndex() fail";
	default:;
	}

	// prepare some attributes
	const util::fvector3 indexOriginOffset = atDim < data::timeDim ? offset * distance[atDim] : util::fvector3{0,0,0};
	const bool acqWasList=queryProperty("acquisitionNumber")->size()==getDimSize(atDim);
	const bool originWasList=queryProperty("indexOrigin")->size()==getDimSize(atDim);

	LOG( Debug, info ) << "Splicing chunk at dimenstion " << atDim + 1 << " with indexOrigin stride " << indexOriginOffset << " and acquisitionNumberStride " << acquisitionNumberStride;
	std::list<Chunk> ret = spliceAt((dimensions) atDim); // do low level spliceAt - get the chunklist

	std::list<Chunk>::iterator it = ret.begin();
	it++;// skip the first one

	for( uint32_t cnt = 1; it != ret.end(); it++, cnt++ ) { // adapt some metadata in them
		if(!originWasList){
			LOG( Debug, verbose_info ) << "Origin was " << it->queryProperty( "indexOrigin" ) << " will be moved by " << indexOriginOffset << "*"  << cnt;
			it->touchProperty( "indexOrigin" )+= util::fvector3(indexOriginOffset * cnt);
		}

		if(!acqWasList && acquisitionNumberStride){//@todo acquisitionTime needs to be fixed as well
			LOG( Debug, verbose_info ) << "acquisitionNumber was " << it->queryProperty( "acquisitionNumber" ) << " will be moved by " << acquisitionNumberStride << "*"  << cnt;
			it->touchProperty( "acquisitionNumber" ) += acquisitionNumberStride * cnt;
		}
	}

	return ret;
}

std::list<Chunk> Chunk::spliceAt (dimensions atDim )const
{
	std::list<Chunk> ret;

	//@todo should be locking
	const std::array<size_t, dims> wholesize = getSizeAsVector();
	std::array<size_t, dims> spliceSize{1,1,1,1};
	//copy the relevant dimensional sizes from wholesize (in case of sliceDim we copy only the first two elements of wholesize - making slices)
	std::copy(std::begin(wholesize),std::begin(wholesize)+atDim,std::begin(spliceSize));
	//get the spliced ValueArray's (the volume of the requested dims is the split-size - in case of sliceDim it is rows*columns)
	const auto pointers = ValueArray::splice(util::product(spliceSize) );

	//create new Chunks from this ValueArray's
	for( const ValueArray &ref :  pointers ) {
		ret.push_back( Chunk( ref, spliceSize[0], spliceSize[1], spliceSize[2], spliceSize[3] ) );
	}
	PropertyMap dummyMap(*this);
	dummyMap.splice(ret.begin(),ret.end(),false);//copy/spliceAt properties into spliced chunks @todo why is this not const
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
