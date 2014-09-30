/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2013  Enrico Reimer <reimer@cbs.mpg.de>

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

// #define BOOST_SPIRIT_DEBUG_PRINT_SOME 100
// #define BOOST_SPIRIT_DEBUG_INDENT 5

#include <boost/foreach.hpp>
#include <CoreUtils/vector.hpp>
#include "imageFormat_nifti_dcmstack.hpp"
#include "imageFormat_nifti_sa.hpp"
#include <CoreUtils/value_base.hpp>
#include <CoreUtils/property.hpp>
#include <limits.h>

namespace isis
{
namespace image_io
{

using boost::posix_time::ptime;
using boost::gregorian::date;

namespace _internal
{

DCMStack::DCMStack( const util::PropertyMap &src ): util::PropertyMap( src ) {}

////////////////////////////////////////////////////////////////////////////////
// low level json parser
////////////////////////////////////////////////////////////////////////////////
bool DCMStack::readJson( data::ValueArray< uint8_t > stream, char extra_token )
{
	return PropertyMap::readJson(&stream[0],&stream[stream.getLength()],extra_token,"samples:slices");
}

////////////////////////////////////////////////////////////////////////////////
// high level translation to ISIS properties
////////////////////////////////////////////////////////////////////////////////

// some basic functors for later use
struct ComputeTimeDist {
	ptime sequenceStart;
	util::Value<float> operator()( const util::ValueBase &val )const {
		const boost::posix_time::time_duration acDist = val.as<ptime>() - sequenceStart;
		return float( acDist.ticks() ) / acDist.ticks_per_second() * 1000;
	}
};

std::list< data::Chunk > DCMStack::translateToISIS( data::Chunk orig )
{
	// translate some of the entries and clean up the tree we got from DcmMeta (don't touch anything we got from the normal header though)

	if( hasBranch( "time" ) ) { //store pervolume data (samples and slices) 
		//get "samples" and "slices" into DICOM
		branch( "DICOM" ).transfer( *this,"time/samples" );
		branch( "DICOM" ).transfer( *this,"time/slices" );
	}

	if( hasBranch( "global/const" ) ) { //store const data 
		branch( "DICOM" ).transfer( *this, "global/const" );
	}

	if( hasBranch( "global/slices" ) ) { //store slice data 
		branch( "DICOM" ).transfer( *this,"global/slices" );
	}

	// if we have a DICOM/AcquisitionNumber-list or DICOM/InstanceNumber rename that to acquisitionNumber 
	if( hasProperty( "DICOM/InstanceNumber" ) ) {
		rename( "DICOM/InstanceNumber", "acquisitionNumber" );
	} else if( hasProperty( "DICOM/AcquisitionNumber" ) ) {
		rename( "DICOM/AcquisitionNumber", "acquisitionNumber" );
	}

	//translate TM sets we know about to proper Timestamps
	const char *TMs[] = {"DICOM/ContentTime", "DICOM/AcquisitionTime"};
	BOOST_FOREACH( const util::PropertyMap::PropPath tm, TMs ) {
		const boost::optional< util::PropertyValue& > found=hasProperty( tm );
		found && found->transform<ptime>();
	}


	// compute acquisitionTime as relative to DICOM/SeriesTime @todo include date
	const char *time_stor[] = {"DICOM/ContentTime", "DICOM/AcquisitionTime"};
	optional< util::PropertyValue& > serTime=hasProperty( "DICOM/SeriesTime" );
	if(serTime){
		BOOST_FOREACH( const char * time, time_stor ) {
			optional< util::PropertyValue& > src=hasProperty( time );
			if( src ) {
				const ComputeTimeDist comp = {serTime->as<ptime>()};
				util::PropertyValue &dst = touchProperty( "acquisitionTime" );
                for(util::PropertyValue::const_iterator i=src->begin();i!=src->end();i++)//@todo use transform in c++11
                    dst.push_back(comp(*i) );
				remove( time );
				break;
			}
		}
	}

	// deal with mosaic
	boost::optional< util::slist& > iType=refValueAs<util::slist>("DICOM/ImageType");
	if( iType && std::find( iType->begin(), iType->end(), std::string( "MOSAIC" ) ) != iType->end() )
			decodeMosaic();

	// compute voxelGap (must be done after mosaic because it removes SpacingBetweenSlices)
	if ( hasProperty( "DICOM/SliceThickness" ) && hasProperty( "DICOM/SpacingBetweenSlices" ) ) {
		const float gap = getValueAs<float>( "DICOM/SpacingBetweenSlices" ) - getValueAs<float>( "DICOM/SliceThickness" );

		if( gap )setValueAs( "voxelGap", util::fvector3( 0, 0, gap ) );

		remove( "DICOM/SpacingBetweenSlices" );
		remove( "DICOM/SliceThickness" );
	}

	// flatten matrizes
// 	const char *matrizes[] = {"dcmmeta_affine", "dcmmeta_reorient_transform"};
// 	BOOST_FOREACH( util::istring matrix, matrizes ) {
// 		int cnt = 0;
// 		BOOST_FOREACH( const util::PropertyValue & val, propertyValue( matrix ) ) {
// 			setValueAs( matrix + "[" + boost::lexical_cast<util::istring>( cnt++ ) + "]", val.as<util::fvector4>() );
// 		}
// 		remove( matrix );
// 	}

	orig.join( *this, true );

// 	std::list< data::Chunk > ret = ( dosplice ? orig.autoSplice( 1 ) : std::list<data::Chunk>( 1, orig ) ); //splice at (probably) timedim

// 	return ret;
}

void DCMStack::decodeMosaic()
{
	// prepare variables
	const util::PropertyMap::PropPath mosaicTimes = find( "MosaicRefAcqTimes" );
	static const util::PropertyMap::PropPath NumberOfImagesInMosaicProp =  "DICOM/CSAImage/NumberOfImagesInMosaic";
	static const util::PropertyMap::PropPath MosaicOrigin =  "DICOM/ImagePositionPatient";

	// if we have geometric data
	if(
		hasProperty( NumberOfImagesInMosaicProp ) && hasProperty( NumberOfImagesInMosaicProp ) &&
		hasProperty( "DICOM/Columns" ) && hasProperty( "DICOM/Rows" ) &&
		hasProperty( "DICOM/SliceThickness" ) && hasProperty( "DICOM/PixelSpacing" ) &&
		property( MosaicOrigin ).size() == 1
	) {
		// All is fine, lets start
		uint16_t slices = getValueAs<uint16_t>( NumberOfImagesInMosaicProp );
		const uint16_t matrixSize = std::ceil( std::sqrt( slices ) );
		const util::vector3<size_t> size( getValueAs<uint64_t>( "DICOM/Columns" ) / matrixSize, getValueAs<uint64_t>( "DICOM/Rows" ) / matrixSize, slices );
		const util::dlist orientation = getValueAs<util::dlist>( "DICOM/ImageOrientationPatient" );
		util::dlist::const_iterator middle = orientation.begin();
		std::advance( middle, 3 );
		util::fvector3 rowVec, columnVec;
		rowVec.copyFrom( orientation.begin(), middle );
		columnVec.copyFrom( middle, orientation.end() );

		// fix the properties of the source (we 'll need them later)
		util::fvector3 voxelSize = getValueAs<util::fvector3>( "DICOM/PixelSpacing" );
		voxelSize[2] = getValueAs<float>( "DICOM/SpacingBetweenSlices" );
		//remove the additional mosaic offset
		//eg. if there is a 10x10 Mosaic, substract the half size of 9 Images from the offset
		const util::fvector3 fovCorr = ( voxelSize ) * size * ( matrixSize - 1 ) / 2;

		// correct the origin
		util::fvector3 &origin = *refValueAs<util::fvector3>( MosaicOrigin );
		origin += ( rowVec * fovCorr[0] ) + ( columnVec * fovCorr[1] );

		// we dont need that anymore
		remove( NumberOfImagesInMosaicProp ); 

		// replace "MOSAIC" ImageType by "WAS_MOSAIC"
		util::slist &iType = *refValueAs<util::slist>( "DICOM/ImageType" );
		std::replace( iType.begin(), iType.end(), std::string( "MOSAIC" ), std::string( "WAS_MOSAIC" ) );
	} else {
		LOG( Runtime, error ) << "Failed to decode mosaic geometry data, won't touch " << MosaicOrigin;
	}

	//flatten MosaicRefAcqTimes and add it to acquisitionTime
	if( ! mosaicTimes.empty() ) { // if there are MosaicRefAcqTimes recompute acquisitionTime

		util::PropertyValue &acq = touchProperty( "acquisitionTime" );
		const util::PropertyValue &mos = property( mosaicTimes );

		if( !acq.isEmpty() && ( acq.is<util::ilist>() || acq.is<util::dlist>() || acq.is<util::slist>() ) ) {
			LOG( Runtime, warning ) << "There is already an acquisitionTime for each slice, won't recompute it from " << mosaicTimes;
		} else if( !acq.isEmpty() && mos.size() != acq.size() ) {
			LOG( Runtime, warning ) << "The list size of " << mosaicTimes << "(" << mos.size() << ") and acquisitionTime (" << acq.size() << ") don't match, won't touch them";
		} else {
			util::PropertyValue old_acq;

			if( acq.size() == mos.size() )
				old_acq.transfer(acq);

			acq.reserve( mos[0].as<util::dlist>().size() * mos.size() );

			for( size_t i = 0; i < mos.size(); i++ ) {
				const util::dlist inner_mos = mos[i].as<util::dlist>();
				const float offset= old_acq.isEmpty() ? 0:old_acq[i].as<float>();
				BOOST_FOREACH(const float m,inner_mos){
					acq.push_back(m+offset);
				}
			}

			remove( mosaicTimes ); // remove the original prop if the distribution was successful
		}
	}
}

}

}
}
