/*
 * imageIONiiTest.cpp
 *
 *  Created on: Apr 12, 2010
 *      Author: Thomas Proeger
 */

#include <isis/core/image.hpp>
#include <isis/core/io_factory.hpp>
#include <isis/core/log.hpp>
#include <isis/core/tmpfile.hpp>

using namespace isis;

#define BOOST_TEST_MODULE "imageIONiiTest"
#include <boost/test/unit_test.hpp>
#include <filesystem>
#include <iostream>
#include <string>

namespace isis
{
namespace test
{
const util::Selection formCodes({"SCANNER_ANAT", "ALIGNED_ANAT", "TALAIRACH", "MNI_152"} );

BOOST_AUTO_TEST_SUITE ( imageIONii_NullTests )

BOOST_AUTO_TEST_CASE( loadsaveNullImage )
{
	util::DefaultMsgPrint::stopBelow( warning );
	util::Selection formCode = formCodes;
	formCode.set( "SCANNER_ANAT" );

	std::list<data::Image> images = data::IOFactory::load( "nix.null" );
	BOOST_REQUIRE( images.size() >= 1 );

	data::enableLog<util::DefaultMsgPrint>(info);
	image_io::enableLog<util::DefaultMsgPrint>( info );
	for( data::Image & null :  images ) {

		// adapt additional/non supported properties
        null.remove( "performingPhysician" );

		const size_t ID=null.getValueAs<uint32_t>( "typeID" );
		switch(ID){//skip unsupported types
			case util::typeID<util::color48>():
			case util::typeID<util::vector3<float>>():
			case util::typeID<util::vector3<double>>():
			case util::typeID<util::vector3<int32_t>>():
			case util::typeID<util::vector4<float>>():
			case util::typeID<util::vector4<double>>():
			case util::typeID<util::vector4<int32_t>>():
				continue;
			default:{}
		}
		if(null.getValueAs<uint32_t>("sequenceNumber")>=100)
			continue;// @todo splicing of interleaved images (images with acquisitionNumber as an interleaved list) is currently broken

        std::filesystem::path niifile(std::string(std::tmpnam(nullptr))+"_ID"+null.property( "typeID" ).toString()+".nii");
		BOOST_REQUIRE( data::IOFactory::write( null, niifile.native() ) );


		// nifti does not know voxelGap - so some other properties have to be modified
		null.touchProperty( "voxelSize" ) += null.getValueAs<util::fvector3>( "voxelGap" );
		null.remove( "voxelGap" );
        null.remove( "typeID" );

		// that will be set by the nifti reader
		const auto minmax = null.getMinMax();
		if(ID==util::typeID<util::color24>() || ID==util::typeID<util::color48>()){
			null.setValueAs("window/min",0);
			null.setValueAs("window/max", 255);
		} else if(ID!=util::typeID<std::complex<float>>() && ID!=util::typeID<std::complex<double>>()){//nifti does not support cal_min/cal_max for complex data
			null.touchProperty("window/min") = minmax.first;
			null.touchProperty("window/max") = minmax.second;
		}

		std::list< data::Image > niftilist = data::IOFactory::load( niifile.native() );
		BOOST_REQUIRE( niftilist.size() == 1 );
		data::Image &nii = niftilist.front();

		// nifti cannot store sequenceNumber - so its always "0"
		null.remove( "sequenceNumber" );
		nii.remove( "sequenceNumber" );

		//we always store sform *and* qform to prevent spm from doing silly things
		//that means we always end up with both when loading our own data
		null.setValueAs( "nifti/sform_code",formCode );
		nii.remove( "nifti/qfac" );
		nii.remove( "nifti/qform_code" );
		nii.remove( "nifti/qoffset" );
		nii.remove( "nifti/quatern_b" );
		nii.remove( "nifti/quatern_c" );
		nii.remove( "nifti/quatern_d" );

		nii.spliceDownTo( ( data::dimensions )null.getChunk( 0, 0 ).getRelevantDims() ); // make the chunks of the nifti have the same dimensionality as the origin

		std::vector< data::Chunk > niiChunks = nii.copyChunksToVector();
		std::vector< data::Chunk > nullChunks = null.copyChunksToVector();

		BOOST_REQUIRE_EQUAL( niiChunks.size(), nullChunks.size() );

		for( size_t i = 0; i < niiChunks.size(); i++ ) { // compare the chunks
			niiChunks[i].remove( "source" );
			nullChunks[i].remove( "source" );

			// @todo because of the spliceAt we get a big rounding error in indexOrigin (from summing up voxelSize)
			niiChunks[i].remove( "indexOrigin" );
			nullChunks[i].remove( "indexOrigin" );

			// because of the quaternions we get some rounding errors in rowVec and columnVec
			const char *fuzzies[] = {"rowVec", "columnVec", "voxelSize"};
			for( const char * fuzz :  fuzzies ) {
				const auto niiVec = niiChunks[i].getValueAs<util::fvector3>( fuzz );
				const auto nullVec = nullChunks[i].getValueAs<util::fvector3>( fuzz );
				BOOST_REQUIRE( util::fuzzyEqualV( niiVec, nullVec ) );
				niiChunks[i].remove( fuzz );
				nullChunks[i].remove( fuzz );
			}

			const util::PropertyMap::DiffMap diff = niiChunks[i].getDifference( nullChunks[i] );

			if( !diff.empty() )
				std::cout << diff << std::endl;

			BOOST_REQUIRE( diff.empty() );
		}

	}
}

BOOST_AUTO_TEST_SUITE_END()

}
}
