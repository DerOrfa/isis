/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2016  Enrico Reimer <reimer@cbs.mpg.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "transform.hpp"
#include "common.hpp"
#include <Eigen/Eigen>

namespace isis::math {
namespace _internal{

bool transformCoords( util::PropertyMap& propertyObject, const util::vector4< size_t > size, util::Matrix3x3<float> transform_in, bool transformCenterIsImageCenter = false )
{
	LOG_IF( !propertyObject.hasProperty( "rowVec" ) || !propertyObject.hasProperty( "columnVec" ) || !propertyObject.hasProperty( "sliceVec" )
			|| !propertyObject.hasProperty( "voxelSize" ) || !propertyObject.hasProperty( "indexOrigin" ), Debug, error )
			<< "Missing one of the properties (rowVec, columnVec, sliceVec, voxelSize, indexOrigin)";

	Eigen::Matrix3f transform;
	for( int r = 0; r < 3; r++ ) {
		for( int c = 0; c < 3; c++ ) {
			transform(r, c) = transform_in[r][c]; //rowVec is the first column in the translation matric
		}
	}

	// this implementation assumes that the PropMap properties is either a
	// data::Chunk or a data::Image object. Hence it should contain the
	// properties rowVec, columnVec, sliceVec and indexOrigin.
	// get row, column and slice vector from property map
	util::fvector3 &row = propertyObject.refValueAs<util::fvector3>( "rowVec" );
	util::fvector3 &column = propertyObject.refValueAs<util::fvector3>( "columnVec" );
	util::fvector3 &slice = propertyObject.refValueAs<util::fvector3>( "sliceVec" );
	// reference index origin from property map
	util::fvector3 &indexorig = propertyObject.refValueAs<util::fvector3>( "indexOrigin" );
	Eigen::Vector3f origin_out;
	//check if we have a property "voxelGap" to prevent isis from throwing a warning "blabla"
	util::fvector3 scaling;

	if( propertyObject.hasProperty( "voxelGap" ) ) {
		scaling  = propertyObject.getValueAs<util::fvector3>( "voxelSize" ) +  propertyObject.getValueAs<util::fvector3>( "voxelGap" );
	} else {
		scaling  = propertyObject.getValueAs<util::fvector3>( "voxelSize" );
	}

	// STEP 1 transform orientation matrix
	// input matrix
	Eigen::Matrix3f R_in;

	for( int r = 0; r < 3; r++ ) {
		R_in( r, 0 ) = row[r]; //rowVec is the first column in the translation matric
		R_in( r, 1 ) = column[r]; //columnVec is the second column in the translation matric
		R_in( r, 2 ) = slice[r]; //sliceVec is the third column in the translation matric
	}

	Eigen::Matrix3f R_out;

	if( transformCenterIsImageCenter ) {
		R_out =  R_in * transform ;
	} else {
		R_out = transform * R_in ;
	}

	for ( int i = 0; i < 3; i++ ) {
		row[i] = R_out( i, 0 );
		column[i] = R_out( i, 1 );
		slice[i] = R_out( i, 2 );
	}

	Eigen::Vector3f origin_in;

	for( int i = 0; i < 3; i++ ) {
		origin_in( i ) = indexorig[i];
	}

	//the center of the transformation is the image center (eg. spm transformation)
	//@todo test me
	if( transformCenterIsImageCenter ) {
		Eigen::Matrix3f R_in_inverse;
		bool check=false;
		R_in.computeInverseWithCheck(R_in_inverse,check);

		if( !check ) {
			LOG( Runtime, error ) << "Can not inverse orientation matrix: " << R_in;
			return false;
		}

		//we have to map the indexes of the image size into the scanner space

		Eigen::Vector3f physicalSize;
		Eigen::Vector3f boostScaling;

		for ( unsigned short i = 0; i < 3; i++ ) {
			physicalSize( i ) = size[i] * scaling[i];
			boostScaling( i ) = scaling[i];
		}

		// now we have to calculate the center of the image in physical space
		Eigen::Vector3f half_image( 3 );

		for ( unsigned short i = 0; i < 3; i++ ) {
			half_image( i ) = ( physicalSize( i )  - boostScaling( i ) ) * 0.5;
		}

		Eigen::Vector3f center_image = R_in*half_image + origin_in ;
		//now translate this center to the center of the physical space and get the new image origin
		Eigen::Vector3f io_translated = origin_in - center_image;
		//now multiply this translated origin with the inverse of the orientation matrix of the image
		Eigen::Vector3f io_ortho = R_in_inverse * io_translated;
		//now transform this matrix with the actual transformation matrix
		Eigen::Vector3f transformed_io_ortho = transform * io_ortho;
		//now transform ths point back with the orientation matrix of the image
		Eigen::Vector3f transformed_io = R_in * transformed_io_ortho;
		//and finally we have to retranslate this origin to get the image to our old position in physical space
		origin_out = transformed_io + center_image;

	} else {
		origin_out = transform* origin_in;
	}

	// write modified values back into the referenced indexOrigin
	for( int i = 0; i < 3; i++ ) {
		indexorig[i] = origin_out( i );
	}
	return true;
}
}

bool transformCoords(data::Chunk& chk, util::Matrix3x3<float> transform_matrix, bool transformCenterIsImageCenter)
{
	//for transforming we have to ensure to have the below properties in our chunks
	std::set<util::PropertyMap::PropPath> propPathList;
	for( const char * prop :  {"indexOrigin", "rowVec", "columnVec", "sliceVec", "voxelSize"} ) {
		const util::PropertyMap::PropPath pPath( prop );
		if ( !chk.hasProperty ( pPath ) ) {
			LOG( Runtime, error ) << "Cannot do transformCoords without " << prop;
			return false;
		}
	}

	if( !_internal::transformCoords( chk, chk.getSizeAsVector(), transform_matrix, transformCenterIsImageCenter ) ) {
		LOG( Runtime, error ) << "Error during transforming the coords of the chunk.";
		return false;
	}

	return true;
}

bool transformCoords(data::Image& img, util::Matrix3x3<float> transform_matrix, bool transformCenterIsImageCenter)
{
#pragma message("test me")
	// we transform an image by transforming its chunks
	std::vector< data::Chunk > chunks=img.copyChunksToVector();

	for( data::Chunk &chRef :  chunks ) {
		if ( !transformCoords (chRef, transform_matrix, transformCenterIsImageCenter ) ) {
			return false;
		}
	}
	//re-build image from transformed chunks
	img=data::Image(chunks);
	return img.isClean();
}

data::dimensions mapScannerAxisToImageDimension(const data::Image &img, data::scannerAxis scannerAxes)
{
#pragma message("test me")
	Eigen::Matrix4f latchedOrientation;
	Eigen::Vector4f mapping;
	latchedOrientation( getBiggestVecElemAbs(img.getValueAs<util::fvector3>("rowVec")), 0 ) = 1;
	latchedOrientation( getBiggestVecElemAbs(img.getValueAs<util::fvector3>("columnVec")), 1 ) = 1;
	latchedOrientation( getBiggestVecElemAbs(img.getValueAs<util::fvector3>("sliceVec")), 2 ) = 1;
	latchedOrientation( 3, 3 ) = 1;

	for( size_t i = 0; i < 4; i++ ) {
		mapping( i ) = i;
	}

	return static_cast<data::dimensions>( (latchedOrientation * mapping)( scannerAxes ) );

}


}
