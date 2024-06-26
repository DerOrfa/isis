// kate: show-indent on; indent-mode tab; indent-width 4; tab-width 4; replace-tabs off; auto-insert-doxygen on

//
// C++ Implementation: image
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
#pragma warning(disable:4996 4244)
#endif

#include "image.hpp"
#include "vector.hpp"
#include "property.hpp"

#define _USE_MATH_DEFINES 1
#include <math.h>
#include <cmath>

namespace isis::data
{

Image::Image ( ) : set( defaultChunkEqualitySet ), clean( false )
{
	for(const auto &needed: neededProperties)
		addNeeded(needed);

	set.addSecondarySort( "acquisitionNumber");
	set.addSecondarySort( "acquisitionTime" );
}

Image::Image ( const Chunk &chunk, dimensions min_dim ): Image()
{
	minIndexingDim=min_dim;
	if ( ! ( insertChunk( chunk ) && reIndex() && isClean() ) ) {
		LOG( Runtime, error ) << "Failed to create image from single chunk.";
	} else if( !isValid() ) {
		LOG_IF( !getMissing().empty(), Debug, warning )
		    << "The created image is missing some properties: " << getMissing() << ". It will be invalid.";
	}
}

Image::Image( const data::Image &ref ): set({})/*SortedChunkList has no default constructor - lets just make an empty (and invalid) set*/
{
	( *this ) = ref; // set will be replaced here anyway
}

Image &Image::operator=( const data::Image &ref )
{
	//deep copy bases
	static_cast<util::PropertyMap &>( *this ) = static_cast<const util::PropertyMap &>( ref );
	static_cast<NDimensional< 4 >&>( *this ) = static_cast<const NDimensional< 4 >&>( ref );
	//deep copy members
	chunkVolume = ref.chunkVolume;
	clean = ref.clean;
	set = ref.set;
	minIndexingDim = ref.minIndexingDim;
	//replace all chunks (in set) by cheap copies of them
	struct : public _internal::SortedChunkList::chunkPtrOperator {
		std::shared_ptr< Chunk > operator()( const std::shared_ptr< Chunk >& ptr ) {
			return std::make_shared< Chunk > ( *ptr );
		}
	} replace;
	set.transform( replace );
	lookup = set.getLookup();
	return *this;
}


bool Image::checkMakeClean()
{
	if ( ! clean ) {
		LOG( Debug, info )  << "Image is not clean. Running reIndex ...";

		if( !reIndex() ) {
			LOG( Runtime, error ) << "Reindexing failed -- undefined behavior ahead ...";
		}
	}

	return clean;
}
bool Image::isClean()const
{
	return clean;
}

void Image::swapDim( short unsigned int dim_a, short unsigned int dim_b, std::shared_ptr<util::ProgressFeedback> feedback )
{
	NDimensional<4>::swapDim( dim_a, dim_b, begin(), feedback ); // this runs through all chunks as the Iterator does that
	for(auto pCh:lookup){ //but we still have to reshape the chunks
		auto shape=pCh->getSizeAsVector();
		std::swap(shape[dim_a],shape[dim_b]);
		pCh->init(shape);
	}

	//swap voxel sizes
	auto voxel_size_query=queryValueAs<util::fvector3>("voxelSize");
	if(voxel_size_query && dim_a<data::timeDim && dim_b<data::timeDim){
		util::fvector3 &voxel_size=*voxel_size_query;
		std::swap(voxel_size[dim_a],voxel_size[dim_b]);
	}

	//voxelsize is needed to be equal inside Images so there should be no voxelSize in the chunks
	assert(!getChunkAt(0,false).hasProperty("voxelSize"));
}


void Image::deduplicateProperties()
{

	LOG_IF( lookup.empty(), Debug, error ) << "The lookup table is empty. Won't do anything.";
	if(lookup.empty())
		return;

	//transform lookup into a "maps-list"
	std::list<std::shared_ptr<PropertyMap>> maps;
	std::transform(
	    lookup.begin(),lookup.end(),std::back_inserter(maps),
	    [](const std::shared_ptr<Chunk> &m){
		    return std::static_pointer_cast<PropertyMap>(m);
	    }
	);

	PropertyMap::deduplicate(maps); // call PropertyMap's deduplicate
	maps.clear();

	// if there are some non-spliced..
	std::list<std::list<std::shared_ptr<Chunk>>> chunks;
	if(!set.not_spliced.empty()){ //deduplicate them as well

		// first get them into the proper container
		for(auto &pair:set.not_spliced){
			maps.emplace_back(new util::PropertyMap());
			maps.back()->transfer(pair.first);
			chunks.push_back({});
			chunks.back().swap(pair.second);
		}
		set.not_spliced.clear();
		PropertyMap::deduplicate(maps); // call PropertyMap's deduplicate on the not_spliced

		// and copy into the chunks what's left
		auto c=chunks.begin();
		for(auto m=maps.begin();m!=maps.end();m++,c++){
			LOG(Debug,verbose_info) << "Copying true splices " << **m << " into " << c->size() << " chunks";
			for(auto &ch:*c){
				ch->join(**m);
			}
		}
	}

	LOG( Debug, verbose_info ) << "common properties now in the image: " << *this;
}

bool Image::insertChunk ( const Chunk &chunk )
{
	if ( chunk.getVolume() == 0 ) {
		LOG( Runtime, error )
		        << "Cannot insert empty Chunk (Size is " << chunk.getSizeAsString() << ").";
		return false;
	}

	if ( !chunk.isValid() ) {
		LOG( Runtime, error ) << "Cannot insert invalid Chunk.";
		return false;
	}

	if( clean ) {
		LOG( Runtime, warning ) << "Inserting into already indexed images is inefficient. You should not do that.";

		// re-move all properties from the image back into the chunks
		for( std::shared_ptr<Chunk> &ref :  lookup ) {
			ref->transfer( *this );
		}
	}


	if( set.insert( chunk ) ) { // if the insertion was successful the image has to be reindexed anyway
		clean = false;
		lookup.clear();
		return true;
	} else {
		// if the insertion failed, but the image was clean (aka we above moved properties back into chunks)
		// so we have to de-duplicate properties again
		// the image is still clean though, no need to reindex
		if( clean )
			deduplicateProperties();

		return false;
	}
}

void Image::setIndexingDim( dimensions d )
{
	minIndexingDim = d;

	if( clean ) {
		LOG( Debug, warning ) << "Image was allready indexed. reIndexing ...";
		reIndex();
	}
}

bool Image::reIndex(util::slist* rejected)
{
	if ( set.isEmpty() ) {
		LOG( Debug, warning ) << "Reindexing an empty image is useless.";
		return false;
	}

	if( set.makeRectangular(rejected) > 0 && set.isEmpty() ) {
		LOG( Runtime, error ) << "No retangular image data left. Skipping";
		return false;
	}

	//redo lookup table
	lookup = set.getLookup();
	util::vector<size_t, dims> structure_size; //storage for the size of the chunk structure
	structure_size.fill( 1 );

	//get primary attributes from geometrically first chunk - will be useful
	const Chunk &first = chunkAt( 0 );

	//start indexing at either the chunk-size or the give minIndexingDim (whichever is bigger)
	const unsigned short chunk_dims = std::max<unsigned short>( first.getRelevantDims(), minIndexingDim );
	chunkVolume = first.getVolume();

	//move all props from the image and move them (back) into the chunks to redo common check
	for(size_t i=0;i+1<lookup.size();i++) // copy the first n-1
		lookup[i]->join(*this);
	lookup.back()->transfer(*this); //transfer into the last

	//clean generated props if they're still here (which would mean the chunks has their own -- which is a good thing)
	if(hasProperty("indexOrigin"))remove( "indexOrigin" );
	if(hasProperty("rowVec"))remove( "rowVec" );
	if(hasProperty("columnVec"))remove( "columnVec" );
	if(hasProperty("sliceVec"))remove( "sliceVec" );

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Determine structure of the image by searching for dimensional breaks in the chunklist
	///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//if there are many chunks, they must leave at least on dimension to the image to "sort" them in
	const size_t timesteps = set.getHorizontalSize();
	const unsigned short sortDims = dims - ( timesteps > 1 ? 1 : 0 ); // don't use the uppermost dim, if the timesteps are already there

	if ( chunk_dims >= Image::dims ) {
		if ( lookup.size() > 1 ) {
			LOG( Runtime, error )
			        << "Cannot handle multiple Chunks, if they have more than "
			        << Image::dims - 1 << " dimensions";
			return false;
		}

		//if there is only one chunk, it's ok - the image will consist only of this one,
		//and commonGet will always return <0,set.begin()->getLinearIndex()>
		//thus in this case voxel() equals set.begin()->voxel()
	} else {// OK there is at least one dimension to sort in the chunks
		LOG( Debug, info ) << "Computing strides for dimensions " << util::MSubject( chunk_dims + 1 ) << " to " << util::MSubject( sortDims );

		// check the chunks for at least one dimensional break - use that for the size of that dimension
		for ( unsigned short i = chunk_dims; i < sortDims; i++ ) { //if there are dimensions left figure out their size
			structure_size[i] = getChunkStride( structure_size.product() ) / structure_size.product();
			assert( structure_size[i] != 0 );
		}
	}

	if ( sortDims < dims ) { //if there is a timedim (not all dims was used for geometric sort)
		assert( structure_size[sortDims] == 1 );
		structure_size[sortDims] = timesteps; // fill the dim above the top geometric dim with the timesteps
	}

	assert( structure_size.product() == lookup.size() );
	//Clean up the properties
	deduplicateProperties();

	// add the chunk-size to the image-size
	for ( unsigned short i = 0; i < chunk_dims; i++ )
		structure_size[i] = first.getDimSize( i );

	init( structure_size ); // set size of the image
	//////////////////////////////////////////////////////////////////////////////////////////////////
	//reconstruct some redundant information, if its missing
	//////////////////////////////////////////////////////////////////////////////////////////////////
	const util::PropertyMap::key_type vectors[] = {"rowVec", "columnVec", "sliceVec"};
	int oneCnt = 0;

	for( const util::PropertyMap::key_type & ref :  vectors ) {
		const auto found=queryValueAs<util::fvector3>( ref );
		if ( found ) {
			util::fvector3 &vec = *found;

			if( vec.sqlen() == 0 ) {
				util::fvector3  v_one{0,0,0};
				v_one[oneCnt] = 1;
				LOG( Runtime, error )
				        << "The existing " << ref << " " << vec << ( hasProperty( "source" ) ? " from " + getValueAs<std::string>( "source" ) : "" )
				        << " has the length zero. Falling back to " << v_one << ".";
				vec = v_one;
			}

			vec.normalize();
		}
		oneCnt++;
	}

	if(!hasProperty("indexOrigin")){ // if there is no common indexOrigin
		const util::PropertyValue* found =first.queryProperty("indexOrigin");
		if(found) // get it from the first chunk - which than by definition should have one
			touchProperty("indexOrigin")=*found;
		else{
			LOG(Runtime,error) << "No indexOrigin found " << " falling back to " << util::fvector3();
			setValueAs("indexOrigin",util::fvector3());
		}
	}

	// check voxelsize
	util::fvector3 &voxeSize = refValueAs<util::fvector3>( "voxelSize" );
	for( int i = 0; i < 3; i++ ) {
		if( voxeSize[i] == 0 || std::isinf( voxeSize[i] ) ) {
			LOG( Runtime, warning ) << "voxelSize[" << i << "]=="  << voxeSize[i] << " is invalid, using 1";
			voxeSize[i] = 1;
		}
	}


	//if we have at least two slides (and have slides (with different positions) at all)
	const util::PropertyValue* firstV = first.queryProperty( "indexOrigin" );

	if ( chunk_dims == 2 && structure_size[2] > 1 && firstV ) {
		const Chunk &last = chunkAt( structure_size[2] - 1 );

		const util::PropertyValue* lastV = last.queryProperty( "indexOrigin" );

		if ( lastV ) {
			//check the slice vector
			util::fvector3 distVecNorm = lastV->as<util::fvector3>() - firstV->as<util::fvector3>();
			LOG_IF( distVecNorm.sqlen() == 0, Runtime, error )
			        << "The distance between the the first and the last chunk is zero. That's bad, because I'm going to normalize it.";
			distVecNorm.normalize();

			const auto sliceVec = queryValueAs<util::fvector3>( "sliceVec" );

			if ( sliceVec ) {
				LOG_IF( ! distVecNorm.fuzzyEqual(*sliceVec) , Runtime, info )
				        << "The existing sliceVec " << *sliceVec
				        << " differs from the distance vector between chunk 0 and " << structure_size[2] - 1
				        << " " << distVecNorm;
			} else {
				LOG( Debug, info )
				        << "used the distance between chunk " << 0 << " and " << structure_size[2] - 1
				        << " to synthesize the missing sliceVec as " << distVecNorm;
				setValueAs( "sliceVec", distVecNorm); // this should message if there really is a conflict
			}

			const float avDist = ( lastV->as<util::fvector3>() - firstV->as<util::fvector3>()).len() / ( structure_size[2] - 1 ); //average dist between the middle of two slices

			const float sliceDist = avDist - voxeSize[2]; // the gap between two slices

			if ( sliceDist > 0 ) {
				static const float inf = -std::numeric_limits<float>::infinity();

				util::fvector3 &voxelGap = refValueAsOr( "voxelGap", util::fvector3{0, 0, inf} ); //if there is no voxelGap yet, we create it as (0,0,inf)

				if ( voxelGap[2] != inf ) {
					if(! util::fuzzyEqual( voxelGap[2], sliceDist, 500 )){
						LOG(Runtime, warning )
						        << "The existing slice distance (voxelGap[2]) " << util::MSubject( voxelGap[2] )
						        << " differs from the distance between chunk 0 and 1, which is " << util::MSubject( sliceDist );
					}
				} else {
					voxelGap[2] = sliceDist;
					LOG( Debug, info )
					        << "used the distance between chunk " << 0 << " and " << 1 << " to synthesize the missing slice distance (voxelGap[2]) as "
					        << util::MSubject( sliceDist );
				}
			}
		}
	}

	//if we have row- and column- vector
	auto rrow = queryValueAs<util::fvector3>( "rowVec" ),rcolumn = queryValueAs<util::fvector3>( "columnVec" );
	if(rrow && rcolumn){
		const util::fvector3 &row=*rrow,&column=*rcolumn;
		LOG_IF( row.dot( column ) > 0.01f, Runtime, warning ) << "The cosine between the columns and the rows of the image is bigger than 0.01";
		const util::fvector3 crossVec = util::fvector3({ //we could use their cross-product as sliceVector
		                                    row[1] * column[2] - row[2] * column[1],
		                                    row[2] * column[0] - row[0] * column[2],
		                                    row[0] * column[1] - row[1] * column[0]
		                                });
		auto sliceVec = queryValueAs<util::fvector3>( "sliceVec" );

		if ( sliceVec ) {
			LOG_IF( std::acos( crossVec.dot(*sliceVec) )  > 180 / M_PI, Runtime, warning ) //angle more than one degree
			        << "The existing sliceVec " << util::MSubject( sliceVec ) << " differs from the cross product of the row- and column vector " << util::MSubject( crossVec );
		} else {
			// We don't know anything about the slice-direction
			// we just guess its along the positive cross-product between row- and column direction
			// so at least warn the user if we do that long shot
			LOG( Runtime, info )
			        << "used the cross product between rowVec and columnVec as sliceVec:"
			        << crossVec << ". That might be wrong!";
			setValueAs( "sliceVec", crossVec );
		}
	} else {
		LOG(Runtime,warning) << "Image has no common rowVec/columnVec, won't check for sliceVec";
	}


	auto storedFoV = queryValueAs<util::fvector3>( "fov" );

	if ( storedFoV ) {
		const util::fvector3 calcFoV = getFoV();

		bool ok = true;

		for ( size_t i = 0; i < 3; i++ ) {
			if ( ( *storedFoV )[i] != -std::numeric_limits<float>::infinity() ) {
				ok &= util::fuzzyEqual( ( *storedFoV )[i], calcFoV[i] );
			} else
				( *storedFoV )[i] = calcFoV[i];
		}

		LOG_IF( ! ok, Runtime, info )
		        << "The calculated field of view differs from the stored " << util::MSubject( *storedFoV ) << "/" << calcFoV;
	}

	LOG_IF( ! isValid(), Runtime, warning ) << "The image is not valid after reindexing. Missing properties: " << getMissing();

	return clean = isValid();
}

bool Image::isEmpty()const
{
	return set.isEmpty();
}

const std::shared_ptr< Chunk >& Image::chunkPtrAt( size_t pos )const
{
	LOG_IF( lookup.empty(), Debug, error ) << "The lookup table is empty. Run reIndex first.";
	LOG_IF( pos >= lookup.size(), Debug, error ) << "Index is out of the range of the lookup table (" << pos << ">=" << lookup.size() << ").";
	const std::shared_ptr<Chunk> &ptr = lookup[pos];
	LOG_IF( !ptr, Debug, error ) << "There is no chunk at " << pos << ". This usually happens in incomplete images.";
	return ptr;
}

Chunk Image::getChunkAt( size_t pos, bool copy_metadata )const
{
	Chunk ret( *chunkPtrAt( pos ) );
	if( copy_metadata )ret.join( *this ); // copy all metadata from the image in here

	return ret;
}
Chunk &Image::chunkAt( size_t pos )
{
	return *chunkPtrAt( pos );
}

Chunk Image::getChunk ( size_t first, size_t second, size_t third, size_t fourth, bool copy_metadata )
{
	checkMakeClean();
	return const_cast<const Image &>( *this ).getChunk( first, second, third, fourth, copy_metadata ); // use the const version
}

const Chunk Image::getChunk ( size_t first, size_t second, size_t third, size_t fourth, bool copy_metadata ) const
{
	const size_t index = commonGet( first, second, third, fourth ).first;
	return getChunkAt( index, copy_metadata );
}

void Image::copyToValueArray(ValueArray &dst, scaling_pair scaling ) const
{
	if( getVolume() > dst.getLength() ) {
		LOG( Runtime, error ) << "Image won't fit into the ValueArray, wont copy..";
		return;
	}

	if ( clean ) {
		if(!scaling.valid)
			scaling = getScalingTo ( dst.getTypeID() );

		assert(scaling.valid);

		std::vector<ValueArray > targets;

		if( lookup.size() > 1 ) { //if there are more than 1 chunks
			//splice target to have the same parts as the image
			targets = dst.splice( lookup.front()->getVolume() );
		} else {
			//just put that ValueArray into the list
			targets.push_back( dst );
		}
		assert(targets.size()==lookup.size());

		auto target = targets.begin();
		for( const std::shared_ptr<Chunk> &ref :  lookup ) { // copy chunks into the parts
			if ( !ref->copyTo ( *target, scaling ) ) {
				LOG ( Runtime, error )
				        << "Failed to copy raw data of type " << ref->typeName() << " from " << getSizeAsString() << "-image into ValueArray of type "
				        << dst.typeName() << " and length " << dst.getLength();
			}

			target++;
		}
	} else {
		LOG ( Runtime, error ) << "Cannot copy from non clean images. Run reIndex first";
	}
}

Image Image::copy()const{
	Image ret( *this ); // ok we just cheap-copied the whole image

	//we want deep copies of the chunks, and we want them to be of type ID
	struct : _internal::SortedChunkList::chunkPtrOperator {
		std::shared_ptr<Chunk> operator() ( const std::shared_ptr< Chunk >& ptr ) {
			return std::make_shared<Chunk> ( ptr->copyByID( ptr->getTypeID(), scaling_pair(1,0) ) );
		}
	} conv_op;

	ret.set.transform ( conv_op );

	if ( ret.isClean() ) {
		ret.lookup = ret.set.getLookup(); // the lookup table still points to the old chunks
	} else {
		LOG ( Debug, info ) << "Copied unclean image. Running reIndex on the copy.";
		ret.reIndex();
	}

	return ret;
}

Image Image::copyByID( short unsigned int ID, const scaling_pair &scaling ) const
{
	Image ret( *this ); // ok we just cheap-copied the whole image

	//we want deep copies of the chunks, and we want them to be of type ID
	struct : _internal::SortedChunkList::chunkPtrOperator {
		scaling_pair scale;
		unsigned short ID;
		std::shared_ptr<Chunk> operator() ( const std::shared_ptr< Chunk >& ptr ) {
			return std::make_shared<Chunk> ( ptr->copyByID( ID, scale ) );
		}
	} conv_op;

	conv_op.scale = scaling.valid?scaling:getScalingTo(ID);
	conv_op.ID = ID;

	ret.set.transform ( conv_op );

	if ( ret.isClean() ) {
		ret.lookup = ret.set.getLookup(); // the lookup table still points to the old chunks
	} else {
		LOG ( Debug, info ) << "Copied unclean image. Running reIndex on the copy.";
		ret.reIndex();
	}

	return ret;
}

std::vector< Chunk > Image::copyChunksToVector( bool copy_metadata )const
{
	std::vector<isis::data::Chunk> ret;
	LOG_IF(!this->isClean(),Runtime,error) << "Running copyChunksToVector on a non-clean image";
	assert(this->isClean());
	ret.reserve( lookup.size() );
	auto meta=static_cast<const util::PropertyMap*>(this);
	std::transform(lookup.begin(),lookup.end(),std::back_inserter(ret),[copy_metadata,meta](const std::shared_ptr<Chunk> &ch)->data::Chunk{
			Chunk cpy(*ch);
			if( copy_metadata )
				cpy.join(*meta);
			return cpy;
		}
	);
	return ret;
}

size_t Image::getChunkStride ( size_t base_stride )
{
	LOG_IF( set.isEmpty(), Runtime, error ) << "Trying to get chunk stride in an empty image";
	LOG_IF( lookup.empty(), Debug, error ) << "Lookup table for chunks is empty. Do reIndex() first!";

	if ( lookup.size() >= 4 * base_stride ) {
		/* there can't be any stride with less than 3*base_stride chunks (which would actually be an invalid image)
		 * _____
		 * |c c| has no stride/dimensional break
		 * _____
		 * |c c|
		 * |c  | has a dimensional break, but is invalid
		 * _____
		 * |c c|
		 * |c c| is the first reasonable case
		 */
		// get the distance between first and second chunk for comparision
		const util::fvector3 firstV = chunkAt( 0 ).getValueAs<util::fvector3>( "indexOrigin" );
		const util::fvector3 secondV = chunkAt( base_stride ).getValueAs<util::fvector3>( "indexOrigin" );
		const util::fvector3 dist1 = secondV - firstV;

		if( dist1.sqlen() == 0 ) { //if there is no geometric structure anymore - so asume its flat from here on
			LOG( Debug, info ) << "Distance between 0 and " << util::MSubject( base_stride )
			                   << " is zero. Assuming there are no dimensional breaks anymore. Returning " << util::MSubject( base_stride );
			return base_stride;
		} else for ( size_t i = base_stride; i < lookup.size() - base_stride; i += base_stride ) {  // compare every follwing distance to that
			    const util::fvector3 thisV = chunkAt( i ).getValueAs<util::fvector3>( "indexOrigin" );
				const util::fvector3 nextV = chunkAt( i + base_stride ).getValueAs<util::fvector3>( "indexOrigin" );
				const util::fvector3 distFirst = nextV - firstV;
				const util::fvector3 distThis = nextV - thisV;
				LOG( Debug, verbose_info )
				        << "Distance between chunk " << util::MSubject( i ) << " and " << util::MSubject( i + base_stride )
				        << " is " << distThis.len() << ". Distance between 0 and " << util::MSubject( i + base_stride ) << " is " << distThis.len();

				if ( distFirst.sqlen() <= distThis.sqlen() ) { // the next chunk is nearer to the begin than to this => dimensional break => leave
					LOG( Debug, info )
					        << "Distance between chunk " << util::MSubject( i + base_stride )
					        << " and 0 is not bigger than the distance between " << util::MSubject( i + base_stride )
					        << " and " << util::MSubject( i ) << ", assuming dimensional break at " << i + base_stride;
					return i + base_stride;
				}
		    }
	} else  if ( lookup.size() % base_stride ) {
		LOG( Runtime, error )
		        << "The amount of chunks (" << lookup.size()
		        << ") is not divisible by the block size of the dimension below (" << base_stride
		        << "). Maybe the image is incomplete.";
		LOG( Runtime, warning )
		        << "Ignoring "  <<  lookup.size() % base_stride << " chunks.";
		return lookup.size() - ( lookup.size() % base_stride );
	}

	//we didn't find any break, so we assume it's a linear image |c c ... c|
	LOG( Debug, info )
	        << "No dimensional break found, assuming it to be at the end (" << lookup.size() << "/" << set.getHorizontalSize() << ")";
	return lookup.size() / set.getHorizontalSize();
}

std::list<util::PropertyValue> Image::getChunksProperties( const util::PropertyMap::PropPath &key, bool unique )const
{
	std::list<util::PropertyValue > ret;

	if( clean ) {
		for( const auto &ref :  lookup ) {
			const auto prop = std::const_pointer_cast<const Chunk>(ref)->queryProperty( key );

			if(unique){ // if unique
				if( !prop || prop->isEmpty() || //if there is no (or an empty)  prop in ref skip it
				    (prop && !ret.empty() &&  *prop == ret.back()) // if there is a prop, skip if it's the same as the one inserted before
				)
					continue;
			}
			ret.push_back( prop ? *prop : util::PropertyValue() );
		}
	} else {
		LOG( Runtime, error ) << "Cannot get chunk-properties from non clean images. Run reIndex first";
	}

	return ret;
}

size_t Image::getMaxBytesPerVoxel() const
{
	size_t bytes = chunkPtrAt( 0 )->getBytesPerVoxel();
	for( const std::shared_ptr<Chunk> &ref :  lookup ) {
		LOG_IF( bytes != ref->getBytesPerVoxel(), Debug, warning )
		        << "Not all voxels have the same byte size (" << bytes << "!=" << ref->getBytesPerVoxel() << "). Using the biggest.";

		if( bytes < ref->getBytesPerVoxel() ) {
			bytes = ref->getBytesPerVoxel();
		}
	}
	return bytes;
}

std::pair<util::Value, util::Value> Image::getMinMax (bool unify) const
{
	if( !lookup.empty() ) {
		auto minmax_ch = getChunksMinMax();
		auto minmax_pair = std::make_pair(minmax_ch.first.getMinMax(),minmax_ch.second.getMinMax());

		auto ret=std::make_pair(minmax_pair.first.first,minmax_pair.second.second);
		if(unify) { //try to make min and max of the same type
			if (ret.first.typeID() == ret.second.typeID()) { // ok min and max are the same type - trivial case
			} else if (ret.first.fitsInto(ret.second.typeID())) { // if min fits into the type of max, use that
				ret.first=ret.first.copyByID(ret.second.typeID());
			} else if (ret.second.fitsInto(ret.first.typeID())) { // if max fits into the type of min, use that
				ret.second=ret.second.copyByID(ret.first.typeID());
			} else {
				LOG(Runtime, warning)
				    << "Sorry I don't know which datatype I should use. (" << ret.first.typeName()
				    << " or " << ret.second.typeName() << ")";
			}
		}
		return ret;

	} else {
		LOG(Runtime,error) << "Won't run getMinMax on an empty image.";
		return std::pair<util::Value, util::Value>();
	}
}

// @todo this wont work with images of more 2 two different data types
scaling_pair Image::getScalingTo( short unsigned int targetID ) const
{
	LOG_IF( !clean, Debug, error ) << "You should run reIndex before running this";
	std::pair<util::Value, util::Value> minmax = getMinMax();

	for( const std::shared_ptr<const Chunk> &ref :  lookup ) { //find a chunk which would be converted
		if( targetID != ref->getTypeID() ) {
			// and ask that for the scaling / @todo what if there are more than one different chunk-types (and thus scalings)
			const scaling_pair scale = ref->getScalingTo( targetID, minmax );
			return scale;
		}
	}
	return {1,0}; //ok seems like no conversion is needed - return 1/0
}

size_t Image::compare( const isis::data::Image &comp ) const
{
	size_t ret = 0;
	LOG_IF( ! ( clean && comp.clean ), Debug, error )
	        << "Comparing unindexed images will cause you trouble, run reIndex()!";

	if ( getSizeAsVector() != comp.getSizeAsVector() ) {
		LOG( Runtime, warning ) << "Size of images differs (" << getSizeAsVector() << "/"
		                        << comp.getSizeAsVector() << "). Adding difference to the result.";
		ret += ( getSizeAsVector() - comp.getSizeAsVector() ).product();
	}

	const util::vector4<size_t> firstVec=chunkPtrAt( 0 )->getSizeAsVector(),secondVec=comp.chunkPtrAt( 0 )->getSizeAsVector();
	util::vector4<size_t> minVector;
	std::transform(
	    std::begin(firstVec),std::end(firstVec),
	    std::begin(secondVec),std::begin(minVector),
	    [](size_t first,size_t second){return std::min(first,second);}
	);
	const size_t increment = minVector.product();

	for ( size_t i = 0; i < getVolume(); i += increment ) {
		const size_t nexti = i + increment - 1;
		const std::pair<size_t, size_t> c1pair1( i / chunkVolume, i % chunkVolume );
		const std::pair<size_t, size_t> c1pair2( nexti / chunkVolume, nexti % chunkVolume );
		const std::pair<size_t, size_t> c2pair1( i / comp.chunkVolume, i % comp.chunkVolume );
		assert( c1pair1.first == c1pair2.first );
		LOG( Debug, verbose_info ) << "Comparing chunks at " << c1pair1.first << " and "   << c2pair1.first;
		const Chunk &c1 = *chunkPtrAt( c1pair1.first );
		const Chunk &c2 = *( comp.chunkPtrAt( c2pair1.first ) );
		LOG( Debug, verbose_info )
		        << "Start positions are " << c1pair1.second << " and " << c2pair1.second
		        << " and the length is " << c1pair2.second - c1pair1.second;
		ret += static_cast<const ValueArray&>(c1).compare(c1pair1.second, c1pair2.second, c2, c2pair1.second );
	}

	return ret;
}

Image::orientation Image::getMainOrientation()const
{
	LOG_IF( ! isValid() || ! clean, Debug, warning ) << "You should not run this on non clean image. Run reIndex first.";
	auto row = getValueAs<util::fvector3>( "rowVec" );
	auto column = getValueAs<util::fvector3>( "columnVec" );
	row.normalize();
	column.normalize();
	LOG_IF( row.dot( column ) > 0.01, Runtime, warning ) << "The cosine between the columns and the rows of the image is bigger than 0.01";
	const util::fvector3 crossVec = util::fvector3({
	                                    row[1] * column[2] - row[2] * column[1],
	                                    row[2] * column[0] - row[0] * column[2],
	                                    row[0] * column[1] - row[1] * column[0]
	                                });
	const util::fvector3 x({1, 0}), y({0, 1}), z({0, 0, 1});
	double a_axial    = std::acos( crossVec.dot( z ) ) / M_PI;
	double a_sagittal = std::acos( crossVec.dot( x ) ) / M_PI;
	double a_coronal  = std::acos( crossVec.dot( y ) ) / M_PI;
	bool a_inverse = false, s_inverse = false, c_inverse = false;
	LOG( Debug, info ) << "Angles to vectors are " << ( a_sagittal * 180 ) << " to x, " << ( a_coronal * 180 ) << " to y and " << ( a_axial * 180 ) << " to z";

	if( a_axial > .5 ) {
		a_axial = std::abs( a_axial - 1 );
		a_inverse = true;
	}

	if( a_sagittal > .5 ) {
		a_sagittal = std::abs( a_sagittal - 1 );
		s_inverse = true;
	}

	if( a_coronal > .5 ) {
		a_coronal = std::abs( a_coronal - 1 );
		c_inverse = true;
	}

	if( a_axial <= .25 )
		return a_inverse ? reversed_axial : axial;
	else if( a_sagittal <= .25 )
		return s_inverse ? reversed_sagittal : sagittal;
	else if( a_coronal <= .25 )
		return c_inverse ? reversed_coronal : coronal;
	else
		assert( false );

	return axial; //will never be reached
}

ValueArray Image::copyAsValueArray() const {
	ValueArray ret=ValueArray::createByID(getMajorTypeID(), getVolume());
	copyToValueArray(ret);
	return ret;
}

size_t Image::getMajorTypeID() const
{
	switch( getChunk( 0 ).getTypeID() ) { // don't do smart typeID detection for types who can't do minmax
	case util::typeID<util::color24>():
	case util::typeID<util::color48>():
	case util::typeID<std::complex< float >  >():
	case util::typeID<std::complex< double > >():
	case util::typeID<util::fvector3>():
	case util::typeID<util::fvector4>():
	case util::typeID<util::dvector3>():
	case util::typeID<util::dvector4>():
	case util::typeID<util::ivector3>():
	case util::typeID<util::ivector4>():
		LOG( Debug, info ) << "Using flat typeID for " << getChunk( 0 ).typeName() << " because I cannot use min/max";
		return getChunk(lookup.size()/2).getTypeID();
		break;
	default:
		auto minmax = getMinMax();
		LOG( Debug, info ) << "Determining datatype of image with the value range " << minmax;

		if( minmax.first.typeID() == minmax.second.typeID() ) { // ok min and max are the same type - trivial case
			return minmax.first.typeID();
		} else if( minmax.first.fitsInto( minmax.second.typeID() ) ) { // if min fits into the type of max, use that
			return minmax.second.typeID();
		} else if( minmax.second.fitsInto( minmax.first.typeID() ) ) { // if max fits into the type of min, use that
			return minmax.first.typeID();
		} else {
			LOG( Runtime, error ) << "Sorry I dont know which datatype I should use. (" << minmax.first.typeName() << " or " << minmax.second.typeName() << ")";
			std::stringstream o;
			o << "Type selection failed. Range was: " << minmax;
			throw( std::logic_error( o.str() ) );
		}

		break;
	}

	return 0; // @todo id 0 should be invalid
}
std::string Image::getMajorTypeName() const
{
	return util::getTypeMap().at(getMajorTypeID());
}

bool Image::convertToType( short unsigned int ID, scaling_pair scaling )
{
	bool retVal = true;
	if(!scaling.valid){
		scaling=getScalingTo(ID);
		LOG( Debug, info ) << "Computed scaling of the original image data: [" << scaling << "]";
	}


	//we want all chunks to be of type ID - so tell them
	for( std::shared_ptr<Chunk> &ref :  lookup ) {
		retVal &= ref->convertToType( ID, scaling );
	}

	//apply scaling to the window if it's there
	auto windowMax = queryProperty("window/max");
	auto windowMin = queryProperty("window/min");
	if(windowMax)
		(*windowMax) = windowMax->multiply(scaling.scale).plus(scaling.offset);
	if(windowMin)
		(*windowMin) = windowMin->multiply(scaling.scale).plus(scaling.offset);

	return retVal;
}

size_t Image::spliceDownTo( dimensions dim )   //rowDim = 0, columnDim, sliceDim, timeDim
{
	if( lookup[0]->getRelevantDims() < ( size_t ) dim ) {
		LOG( Debug, error ) << "The dimensionality of the chunks of this image is already below " << dim << " cannot spliceAt it.";
		return 0;
	} else if( lookup[0]->getRelevantDims() == dim ) {
		LOG( Debug, info ) << "Skipping useless splicing, relevantDims is already " << lookup[0]->getRelevantDims();
		return lookup.size();
	}

	// get a list of needed properties (everything which is missing in a newly created chunk plus everything which is needed for autosplice)
	static const std::list<util::PropertyMap::PropPath> splice_needed{
		"voxelSize","voxelGap","rowVec","columnVec","sliceVec","indexOrigin","acquisitionNumber"
	};
	static util::PropertyMap::PathSet needed = MemChunk<short>( 1 ).getMissing();
	needed.insert( splice_needed.begin(), splice_needed.end() );
	// reset the Chunk set, so we can insert new splices
	set = _internal::SortedChunkList( defaultChunkEqualitySet );
	set.addSecondarySort( "acquisitionNumber" );
	set.addSecondarySort( "acquisitionTime" ); //@todo this should not be here

	clean = false; // mark the image for reIndexing

	//spliceAt existing lists into the current chunks -- they will be spliced further if necessary
	PropertyMap::splice(lookup.begin(),lookup.end(),true);

	//transfer properties needed for the chunk back into the chunk (if they're there but not lists)
	for( util::PropertyMap::PathSet::const_reference need : needed ) {
		const auto foundNeed=queryProperty( need );
		if(foundNeed){
			for( std::shared_ptr<Chunk> &ref : lookup ) {
				if( !ref->hasProperty( need ) ) {
					LOG( Debug, verbose_info ) << "Copying " << std::make_pair(need, *foundNeed) << " from the image to the chunk for splicing";
					ref->touchProperty( need ) = *foundNeed;
				} else
					LOG(Debug,error) << need << " was found in the chunk although it is in the image as well. It will be deleted in the image";
			}
			this->remove(need);
		}
	}
	// do the splicing
	std::vector<std::shared_ptr<Chunk> > buffer;
	buffer.swap(lookup); // move the old lookup table into a buffer, so it is empty when the splicer starts inserting the new chunks
	for(auto &c_ptr:buffer){
		for(auto &c:c_ptr->autoSplice(dim)){
			const bool inserted=insertChunk(c);
			assert(inserted);//it was in there, should go in again
		}
	}
	reIndex();
	return lookup.size();
}

size_t Image::getNrOfColumns() const
{
	return getDimSize( data::rowDim );
}

size_t Image::getNrOfRows() const
{
	return getDimSize( data::columnDim );
}
size_t Image::getNrOfSlices() const
{
	return getDimSize( data::sliceDim );
}
size_t Image::getNrOfTimesteps() const
{
	return getDimSize( data::timeDim );
}

util::fvector3 Image::getFoV() const
{
	util::fvector4 voxelGap{0,0,0,0};

	if ( hasProperty( "voxelGap" ) ) {
		voxelGap = getValueAs<util::fvector4>( "voxelGap" );

		for ( size_t i = 0; i < 3; i++ )
			if ( voxelGap[i] == -std::numeric_limits<float>::infinity() ) {
				LOG( Runtime, info ) << "Ignoring unknown voxel gap in direction " << i;
				voxelGap[i] = 0;
			}
	}

	const util::fvector4 ret = NDimensional<4>::getFoV( getValueAs<util::fvector4>( "voxelSize" ), voxelGap );

	LOG_IF( ret[timeDim], Runtime, warning ) << "Ignoring fourth dim extend of " << ret[timeDim] << " in Image";

	return util::fvector3( {ret[0], ret[1], ret[2]} );
}

void Image::foreachChunk(std::function<void (Chunk &, util::vector4<size_t>)> func)
{
	checkMakeClean();
	const size_t chunkSize = lookup.front()->getVolume();
	size_t index=0;

	for(std::shared_ptr<Chunk> &ch:lookup){
		func(*ch,getCoordsFromLinIndex(index));
		index+=chunkSize;
	}
}

void Image::foreachChunk(std::function<void (Chunk &)> func)
{
	checkMakeClean();
	for(std::shared_ptr<Chunk> &ch:lookup){
		func(*ch);
	}
}

void Image::foreachChunk(std::function<void (const Chunk &, util::vector4<size_t>)> func) const
{
	if(clean){
		const size_t chunkSize = lookup.front()->getVolume();
		size_t index=0;
		for(const std::shared_ptr<Chunk> &ch:lookup){
			func(*ch,getCoordsFromLinIndex(index));
			index+=chunkSize;
		}
	} else {
		LOG(Runtime,error) << "Trying to run foreachChunk on an unclean image. Won't do anything ..";
	}
}

void Image::foreachChunk(std::function<void (const Chunk &)> func) const
{
	if(clean){
		for(const std::shared_ptr<Chunk> &ch:lookup)
			func(*ch);
	} else {
		LOG(Runtime,error) << "Trying to run foreachChunk on an unclean image. Won't do anything ..";
	}
}

Image::iterator Image::begin()
{
	if(! checkMakeClean() ) {
		LOG( Debug, error )  << "Image is not clean. Returning invalid iterator ...";
	}
	std::vector<ValueArray> buff(lookup.size());
	for(size_t i=0;i<lookup.size();i++)
		buff[i]=*lookup[i];//Chunk is derived from ValueArray
	return iterator( buff );
}
Image::iterator Image::end() {return begin() + getVolume();}
Image::const_iterator Image::begin()const
{
	if(! isClean() ) {
		LOG( Debug, error )  << "Image is not clean. Returning invalid iterator ...";
	}
	std::vector<ValueArray> buff(lookup.size());
	for(size_t i=0;i<lookup.size();i++)
		buff[i]=*lookup[i];//Chunk is derived from ValueArray
	return iterator( buff );
}
Image::const_iterator Image::end()const {return begin() + getVolume();}

const util::Value Image::getVoxelValue (size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps ) const
{
	LOG_IF( !isInRange( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} ), Debug, isis::error )
	        << "Index " << util::vector4<size_t>( {nrOfColumns, nrOfRows, nrOfSlices, nrOfTimesteps} ) << " is out of range (" << getSizeAsString() << ")";
	const auto index = commonGet(nrOfColumns,nrOfRows,nrOfSlices,nrOfTimesteps);
	const Chunk ch = getChunkAt(index.first,false);
	const auto it = ch.begin()[index.second];
	return it;
}
void Image::setVoxelValue (const util::Value &val, size_t nrOfColumns, size_t nrOfRows, size_t nrOfSlices, size_t nrOfTimesteps )
{
	const auto index = commonGet(nrOfColumns,nrOfRows,nrOfSlices,nrOfTimesteps);
	Chunk ch = getChunkAt(index.first,false);
	ch.begin()[index.second]=val;
}

std::string Image::identify ( bool withpath, bool withdate )const
{
	LOG_IF(withpath && !(hasProperty("source")||getChunkAt(0).hasProperty("source")),Runtime,warning) << "Asking for the path in an image that has no \"source\"-property";
	_internal::SortedChunkList::getproplist source("source"),seqNum("sequenceNumber"),seqDesc("sequenceDescription"),seqStart("sequenceStart");
	seqNum(*this);seqDesc(*this);seqStart(*this);source(*this);
	return set.identify(withpath,withdate,source, seqNum,seqDesc,seqStart);
}
std::pair<ValueArray, ValueArray> Image::getChunksMinMax()const
{
	LOG_IF(!clean, Debug, info )  << "Image is not clean, result will be faulty  ...";
	assert(!lookup.empty());
	//get all the minmax from all the chunks
	std::list<util::Value> min, max;
	for(const auto ch_ptr:lookup){
		const auto minmax=ch_ptr->getMinMax();
		min.push_back(minmax.first);
		max.push_back(minmax.second);
	}
	//check to be run on all values
	auto typefind = [](size_t &curtype,const util::Value &val)->void
	{
		if (val.fitsInto(curtype))
			return;// val fits into current type go on
		else // val does not fit into current type
			curtype = val.typeID();//use that as current type from now on
	};

	//start with the type IDs of the first chunk
	size_t mintype(lookup.front()->getTypeID()),maxtype(lookup.front()->getTypeID());

	//run the check with bound mintype/maxtype
	std::for_each(min.begin(),min.end(),std::bind(typefind,std::ref(mintype),std::placeholders::_1));
	std::for_each(max.begin(),max.end(),std::bind(typefind,std::ref(maxtype),std::placeholders::_1));

	//create ValueArrays
	auto ret=std::make_pair(
		ValueArray::createByID(mintype, lookup.size()),
		ValueArray::createByID(maxtype, lookup.size())
	);
	std::copy(min.begin(),min.end(),ret.first.begin());
	std::copy(max.begin(),max.end(),ret.second.begin());
	return ret;
}

} // END namespace isis::data
