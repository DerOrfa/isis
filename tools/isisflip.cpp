#include <isis/core/io_application.hpp>
#include <isis/math/transform.hpp>
#include <map>

using namespace isis;

bool swapProperties( data::Image &image, const unsigned short dim )
{
	const util::vector4<size_t> size = image.getSizeAsVector();
	std::vector<data::Chunk> chunks = image.copyChunksToVector( true );

	if( chunks.front().getRelevantDims() < 2 && dim >= chunks.front().getRelevantDims() ) {
		LOG( data::Runtime, error ) << "Your data is spliced at dimension " << chunks.front().getRelevantDims()
									<< " and you trying to flip at dimenstion " << dim << ". Sorry but this has not yet been impleneted.";
		return false;
	}

	std::vector<data::Chunk> buffer = chunks;
	std::vector<data::Chunk> swapped_chunks;

	for( util::ivector4::value_type one_above_dim = 0; one_above_dim < size[dim + 1]; one_above_dim++ ) {
		util::ivector4::value_type reverse_count = size[dim] - 1;

		for( util::ivector4::value_type count = 0; count < size[dim]; count++, reverse_count-- ) {
			static_cast<util::PropertyMap &>( chunks[count] ) = static_cast<util::PropertyMap &>( buffer[reverse_count] );
		}
	}

	std::list<data::Chunk> chunk_list( chunks.begin(), chunks.end() );
	image = data::Image( chunk_list );
	return true;
}


int main( int argc, char **argv )
{
	ENABLE_LOG( data::Runtime, util::DefaultMsgPrint, error );
	std::map<std::string, unsigned int> alongMap = {{"row", 0 },{"column", 1},{"slice", 2 },{ "x", 3 },{ "y", 4 },{"z", 5 }};
	data::IOApplication app( "isisflip", true, true );
	util::Selection along({"row", "column", "slice", "x", "y", "z"} );
	util::Selection flip({"image", "space", "both"} );
	along.set( "x" );
	flip.set( "both" );
	app.parameters["along"] = along;
	app.parameters["along"].needed() = true;
	app.parameters["along"].setDescription( "Flip along the specified axis" );
	app.parameters["flip"] = flip;
	app.parameters["flip"].needed() = true;
	app.parameters["flip"].setDescription( "What has to be flipped" );
	app.parameters["center"] = false;
	app.parameters["center"].needed() = false;
	app.parameters["center"].setDescription( "If activated the center of the image will be translated to the of the scanner space and after flipping back to its initial position" );
	app.init( argc, argv );
	std::list<data::Image> finImageList;
	unsigned int dim = alongMap[app.parameters["along"].toString()];
	//go through every image
	for( data::Image & refImage :  app.images ) {
		std::vector< data::Chunk > delme = refImage.copyChunksToVector( true );
		isis::data::Image dummy( delme );

		//make an identity matrix
		auto T = util::identityMatrix<float,3>();

		if( dim > 2 ) {
			dim = math::mapScannerAxisToImageDimension(refImage, static_cast<data::scannerAxis>( dim - 3 ) );
		}

		T[dim][dim] *= -1;
		data::Image newImage = refImage;

		if ( app.parameters["flip"].toString() == "image" || app.parameters["flip"].toString() == "both" ) {
			if( refImage.getChunkAt(0).getRelevantDims() > dim ) {
				refImage.foreachChunk( 
					[dim]( data::Chunk &ch, util::vector4<size_t> /*posInImage*/ ) {
						ch.flipAlong( (data::dimensions)dim );
						return true;
					}
				);
			} else {
				if( !swapProperties( refImage, dim ) ) {
					return EXIT_FAILURE;
				}
			}
		}

		if ( app.parameters["flip"].toString() == "both" || app.parameters["flip"].toString() == "space" ) {
			math::transformCoords(refImage, T, app.parameters["center"] );
		}

		finImageList.push_back( refImage );
	}
	app.autowrite( finImageList );
	return 0;
}
