#include "imageFormat_Dicom.hpp"
#include <isis/core/common.hpp>
#include <isis/core/istring.hpp>

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/stream.hpp>

namespace isis
{
namespace image_io
{
namespace _internal
{
util::istring id2Name( const uint16_t group, const uint16_t element ){
	char id_str[4+4+3+1];
	sprintf(id_str,"(%04x,%04x)",group,element);
	return id_str;
}
util::istring id2Name( const uint32_t id32 ){
	return id2Name((id32&0xFFFF0000)>>16,id32&0xFFFF);
}
util::PropertyMap readStream(DicomElement &token,size_t stream_len,std::multimap<uint32_t,data::ValueArrayReference> &data_elements);
util::PropertyMap readItem(DicomElement &token,std::multimap<uint32_t,data::ValueArrayReference> &data_elements){
	assert(token.getID32()==0xFFFEE000);//must be an item-tag
	size_t len=token.getLength();
	token.next(token.getPosition()+8);
	return readStream(token,len,data_elements);
}
void readDataItems(DicomElement &token,std::multimap<uint32_t,data::ValueArrayReference> &data_elements){
	const uint32_t id=token.getID32();
	
	bool wide= (token.getVR()=="OW");
	
	for(token.next(token.getPosition()+8+4);token.getID32()==0xFFFEE000;token.next()){ //iterate through items and store them
		const size_t len=token.getLength();
		if(len){
			LOG(Debug,info) << "Found data item with " << len << " bytes at " << token.getPosition();
			if(wide)
				data_elements.insert({id,token.dataAs<uint16_t>()});
			else
				data_elements.insert({id,token.dataAs<uint8_t>()});
		} else 
			LOG(Debug,info) << "Ignoring zero length data item at " << token.getPosition();
	}
	assert(token.getID32()==0xFFFEE0DD);//we expect a sequence delimiter (will be eaten by the calling loop)
}
util::PropertyMap readStream(DicomElement &token,size_t stream_len,std::multimap<uint32_t,data::ValueArrayReference> &data_elements){
	size_t start=token.getPosition();
	util::PropertyMap ret;

	do{
		//break the loop if we find a delimiter
		if(
			token.getID32()==0xFFFEE00D //Item Delim. Tag
		){ 
			token.next(token.getPosition()+8);
			break;
		}

		const std::string vr=token.getVR();
		if(vr=="OB" || vr=="OW"){
			const uint32_t len=token.getLength();
			if(len==0xFFFFFFFF){ // itemized data of undefined length
				readDataItems(token,data_elements);
			} else {
				std::multimap<uint32_t,data::ValueArrayReference>::iterator inserted;
				if(vr=="OW")
					inserted=data_elements.insert({token.getID32(),token.dataAs<uint16_t>()});
				else
					inserted=data_elements.insert({token.getID32(),token.dataAs<uint8_t>()});
				
				LOG(Debug,info) 
					<< "Found " << inserted->second->getTypeName() << "-data for " << token.getIDString() << " at " << token.getPosition()
					<< " it is " <<token.getLength() << " bytes long";
			}
		}else if(vr=="SQ"){ //explicit SQ (4 bytes tag-id + 2bytes "SQ" + 2bytes reserved)
			//next 4 bytes are the length of the sequence
			uint32_t len=token.getLength();
			const auto name=token.getName();
			
			//we expect the sequence start token
			token.next(token.getPosition()+8+4);
			LOG_IF(len==0xffffffff,Debug,verbose_info) << "Sequence of undefined length found (" << name << "), looking for items at " << token.getPosition();
			LOG_IF(len!=0xffffffff,Debug,verbose_info) << "Sequence of length " << len << " found (" << name << "), looking for items at " << token.getPosition();
			size_t start=token.getPosition();
			//load items (which themself again are made of tags)
			while(token.getPosition()-start<len && token.getID32()!=0xFFFEE0DD){ //break the loop when we find the sequence delimiter tag or reach the end
				assert(token.getID32()==0xFFFEE000);//must be an item-tag
				const size_t item_len=token.getLength();
				token.next(token.getPosition()+8);
				util::PropertyMap subtree=readStream(token,item_len,data_elements);
				ret.touchBranch(name).transfer(subtree);
			}
			LOG(Debug,verbose_info) << "Sequence " << name << " finished, continuing at " << token.getPosition()+token.getLength()+8;
		}else{
			auto value=token.getValue();
			if(!value.isEmpty()){
				ret.touchProperty(token.getName())=*value;
			}
		}
	}while(token.next() && token.getPosition()-start<stream_len);
	return ret;
}
template<typename T> data::ValueArrayReference repackValueArray(data::ValueArrayBase &data){
	const auto new_ptr=std::static_pointer_cast<T>(data.getRawAddress());
	return data::ValueArray<T>(new_ptr,data.getLength());
}
class DicomChunk : public data::Chunk
{
	data::Chunk getUncompressedPixel(data::ValueArrayBase &data,const util::PropertyMap &props){
		auto rows=props.getValueAs<uint32_t>("Rows");
		auto columns=props.getValueAs<uint32_t>("Columns");
		//Number of Frames: 0028,0008
		
		//repack the pixel data into proper type
		data::ValueArrayReference pixel;
		auto color=props.getValueAs<std::string>("PhotometricInterpretation");
		auto bits_allocated=props.getValueAs<uint8_t>("BitsAllocated");
		auto signed_values=props.getValueAsOr<bool>("PixelRepresentation",false);

		if(color=="COLOR"){
			assert(signed_values==false);
			switch(bits_allocated){
				case  8:pixel=repackValueArray<util::color24>(data);break;
				case 16:pixel=repackValueArray<util::color48>(data);break;
				default:LOG(Runtime,error) << "Unsupportet bit-depth "<< bits_allocated << " for color image";
			}
		}else if(color=="MONOCHROME2"){
			switch(bits_allocated){
				case  8:pixel=signed_values? repackValueArray< int8_t>(data):repackValueArray< uint8_t>(data);break;
				case 16:pixel=signed_values? repackValueArray<int16_t>(data):repackValueArray<uint16_t>(data);break;
				case 32:pixel=signed_values? repackValueArray<int32_t>(data):repackValueArray<uint32_t>(data);break;
				default:LOG(Runtime,error) << "Unsupportet bit-depth "<< bits_allocated << " for greyscale image";
			}
		}else {
			LOG(Runtime,error) << "Unsupportet photometric interpretation " << color;
			ImageFormat_Dicom::throwGenericError("bad pixel type");
		}

		// create a chunk of the proper type
		data::Chunk ret(pixel,columns,rows);
		return ret;
	}
public:
	DicomChunk(std::list<data::ValueArrayReference> &&data_elements,const std::string &transferSyntax,const util::PropertyMap &props)
	{
		assert(data_elements.size()==1);
		data::ValueArrayBase &data=*data_elements.front();
#ifdef HAVE_OPENJPEG
		if(transferSyntax=="1.2.840.10008.1.2.4.90"){ //JPEG 2K
			assert(data_elements.front()->getTypeID()==data::ByteArray::staticID());
			static_cast<data::Chunk&>(*this)=
					_internal::getj2k(data.castToValueArray<uint8_t>());
			
			LOG(Runtime,info) 
				<< "Created " << this->getSizeAsString() << "-Image of type " << this->getTypeName() 
				<< " from a " << data.getLength() << " bytes j2k stream";
		} else 
#endif //HAVE_OPENJPEG
		{
			static_cast<data::Chunk&>(*this)=getUncompressedPixel(data,props);
			LOG(Runtime,info) 
				<< "Created " << this->getSizeAsString() << "-Image of type " << this->getTypeName() 
				<< " from a " << data.getLength() << " bytes of raw data";
		}
		this->touchBranch(ImageFormat_Dicom::dicomTagTreeName)=props;
	}
};

}

const char ImageFormat_Dicom::dicomTagTreeName[] = "DICOM";
const char ImageFormat_Dicom::unknownTagName[] = "UnknownTag/";

util::istring ImageFormat_Dicom::suffixes( io_modes modes )const 
{
	if( modes == write_only )
		return util::istring();
	else
		return ".ima .dcm";
}
std::string ImageFormat_Dicom::getName()const {return "Dicom";}
std::list<util::istring> ImageFormat_Dicom::dialects()const {return {"siemens","withExtProtocols","nocsa","keepmosaic","forcemosaic"};}


void ImageFormat_Dicom::sanitise( util::PropertyMap &object, std::list<util::istring> dialects )
{
	const util::istring prefix = util::istring( ImageFormat_Dicom::dicomTagTreeName ) + "/";
	util::PropertyMap &dicomTree = object.touchBranch( dicomTagTreeName );
	/////////////////////////////////////////////////////////////////////////////////
	// Transform known DICOM-Tags into default-isis-properties
	/////////////////////////////////////////////////////////////////////////////////

	// compute sequenceStart and acquisitionTime (have a look at table C.10.8 in the standard)
	{
		// get series start time (remember this is in UTC)
		auto o_seqStart=extractOrTell("SeriesTime",dicomTree,warning );
		if(o_seqStart) {
			auto o_acDate= extractOrTell({"SeriesDate", "AcquisitionDate", "ContentDate"},dicomTree,warning);
			if( o_acDate ) { // add days since epoch from the date
				const util::timestamp seqStart = o_seqStart->as<util::timestamp>()+o_acDate->as<util::date>().time_since_epoch();
				object.setValueAs( "sequenceStart", seqStart);
				LOG(Debug,verbose_info) 
					<< "Merging Series Time " << *o_seqStart << " and Date " << *o_acDate << " as " 
					<< std::make_pair("sequenceStart",object.property("sequenceStart"));
			}
		}
	}
	{
		// compute acquisitionTime
		auto o_acTime= extractOrTell({"AcquisitionTime","ContentTime"},dicomTree,warning);
		if ( o_acTime ) {
			auto o_acDate= extractOrTell({"AcquisitionDate", "ContentDate", "SeriesDate"},dicomTree,warning);
			if( o_acDate ) {
				const util::timestamp acTime = o_acTime->as<util::timestamp>()+o_acDate->as<util::date>().time_since_epoch();
				object.setValueAs<util::timestamp>("acquisitionTime", acTime);
				LOG(Debug,verbose_info) 
					<< "Merging Content Time " << *o_acTime << " and Date " << *o_acDate
					<< " as " << std::make_pair("acquisitionTime",object.property("acquisitionTime"));
			}
		}
	}

	// compute studyStart
	if ( hasOrTell( "StudyTime", dicomTree, warning ) && hasOrTell( "StudyDate", dicomTree, warning ) ) {
		const util::date dt=dicomTree.getValueAs<util::date>("StudyDate");
		const util::timestamp tm=dicomTree.getValueAs<util::timestamp>("StudyTime");
			object.setValueAs("studyStart",tm+dt.time_since_epoch());
			dicomTree.remove("StudyTime");
			dicomTree.remove("StudyDate");
	}
	
	transformOrTell<int32_t>  ( prefix + "SeriesNumber",     "sequenceNumber",     object, warning );
	transformOrTell<uint16_t>  ( prefix + "PatientsAge",     "subjectAge",     object, info );
	transformOrTell<std::string>( prefix + "SeriesDescription", "sequenceDescription", object, warning );
	transformOrTell<std::string>( prefix + "PatientsName",     "subjectName",        object, info );
	transformOrTell<util::date>       ( prefix + "PatientsBirthDate", "subjectBirth",       object, info );
	transformOrTell<uint16_t>  ( prefix + "PatientsWeight",   "subjectWeigth",      object, info );
	// compute voxelSize and gap
	{
		util::fvector3 voxelSize( {invalid_float, invalid_float, invalid_float} );
		const util::istring pixelsize_params[]={"PixelSpacing","ImagePlanePixelSpacing","ImagerPixelSpacing"};
		for(const util::istring &name:pixelsize_params){
			if ( hasOrTell( prefix + name, object, warning ) ) {
				voxelSize = dicomTree.getValueAs<util::fvector3>( name );
				dicomTree.remove( name );
				std::swap( voxelSize[0], voxelSize[1] ); // the values are row-spacing (size in column dir) /column spacing (size in row dir)
				break;
			}
			
		}

		if ( hasOrTell( prefix + "SliceThickness", object, warning ) ) {
			voxelSize[2] = dicomTree.getValueAs<float>( "SliceThickness" );
			dicomTree.remove( "SliceThickness" );
		} else {
			voxelSize[2] = 1 / object.getValueAs<float>( "DICOM/CSASeriesHeaderInfo/SliceResolution" );
		}
		
		object.setValueAs( "voxelSize", voxelSize );
		transformOrTell<uint16_t>( prefix + "RepetitionTime", "repetitionTime", object, warning );
		transformOrTell<float>( prefix + "EchoTime", "echoTime", object, warning );
		transformOrTell<int16_t>( prefix + "FlipAngle", "flipAngle", object, warning );

		if ( hasOrTell( prefix + "SpacingBetweenSlices", object, info ) ) {
			if ( voxelSize[2] != invalid_float ) {
				object.setValueAs( "voxelGap", util::fvector3( {0, 0, dicomTree.getValueAs<float>( "SpacingBetweenSlices" ) - voxelSize[2]} ) );
				dicomTree.remove( "SpacingBetweenSlices" );
			} else
				LOG( Runtime, warning )
						<< "Cannot compute the voxel gap from the slice spacing ("
						<< object.property( prefix + "SpacingBetweenSlices" )
						<< "), because the slice thickness is not known";
		}
	}
	transformOrTell<std::string>   ( prefix + "PerformingPhysiciansName", "performingPhysician", object, info );
	transformOrTell<uint16_t>     ( prefix + "NumberOfAverages",        "numberOfAverages",   object, warning );

	if ( hasOrTell( prefix + "ImageOrientationPatient", object, info ) ) {
		util::dlist buff = dicomTree.getValueAs<util::dlist>( "ImageOrientationPatient" );

		if ( buff.size() == 6 ) {
			util::fvector3 row, column;
			util::dlist::iterator b = buff.begin();

			for ( int i = 0; i < 3; i++ )row[i] = *b++;

			for ( int i = 0; i < 3; i++ )column[i] = *b++;

			object.setValueAs( "rowVec" , row );
			object.setValueAs( "columnVec", column );
			dicomTree.remove( "ImageOrientationPatient" );
		} else {
			LOG( Runtime, error ) << "Could not extract row- and columnVector from " << dicomTree.property( "ImageOrientationPatient" );
		}

		if( object.hasProperty( prefix + "SIEMENS CSA HEADER/SliceNormalVector" ) && !object.hasProperty( "sliceVec" ) ) {
			LOG( Debug, info ) << "Extracting sliceVec from SIEMENS CSA HEADER/SliceNormalVector " << dicomTree.property( "SIEMENS CSA HEADER/SliceNormalVector" );
			util::dlist list = dicomTree.getValueAs<util::dlist >( "SIEMENS CSA HEADER/SliceNormalVector" );
			util::fvector3 vec;
			std::copy(list.begin(), list.end(), std::begin(vec) );
			object.setValueAs( "sliceVec", vec );
			dicomTree.remove( "SIEMENS CSA HEADER/SliceNormalVector" );
		}
	} else {
		LOG( Runtime, warning ) << "Making up row and column vector, because the image lacks this information";
		object.setValueAs( "rowVec" , util::fvector3( {1, 0, 0} ) );
		object.setValueAs( "columnVec", util::fvector3( {0, 1, 0} ) );
	}

	if ( hasOrTell( prefix + "ImagePositionPatient", object, info ) ) {
		object.setValueAs( "indexOrigin", dicomTree.getValueAs<util::fvector3>( "ImagePositionPatient" ) );
	} else if( object.hasProperty( prefix + "SIEMENS CSA HEADER/ProtocolSliceNumber" ) ) {
		util::fvector3 orig( {0, 0, object.getValueAs<float>( prefix + "SIEMENS CSA HEADER/ProtocolSliceNumber" ) / object.getValueAs<float>( "DICOM/CSASeriesHeaderInfo/SliceResolution" )} );
		LOG( Runtime, info ) << "Synthesize missing indexOrigin from SIEMENS CSA HEADER/ProtocolSliceNumber as " << orig;
		object.setValueAs( "indexOrigin", orig );
	} else {
		object.setValueAs( "indexOrigin", util::fvector3() );
		LOG( Runtime, warning ) << "Making up indexOrigin, because the image lacks this information";
	}

	transformOrTell<uint32_t>( prefix + "InstanceNumber", "acquisitionNumber", object, error );

	if( dicomTree.hasProperty( "AcquisitionNumber" )){
		if(dicomTree.property("AcquisitionNumber").eq(object.property( "acquisitionNumber" )))
			dicomTree.remove( "AcquisitionNumber" );
	}

	if ( hasOrTell( prefix + "PatientsSex", object, info ) ) {
		util::Selection isisGender( "male,female,other" );
		bool set = false;

		switch ( dicomTree.getValueAs<std::string>( "PatientsSex" )[0] ) {
		case 'M':
			isisGender.set( "male" );
			set = true;
			break;
		case 'F':
			isisGender.set( "female" );
			set = true;
			break;
		case 'O':
			isisGender.set( "other" );
			set = true;
			break;
		default:
			LOG( Runtime, warning ) << "Dicom gender code " << util::MSubject( object.property( prefix + "PatientsSex" ) ) <<  " not known";
		}

		if( set ) {
			object.setValueAs( "subjectGender", isisGender);
			dicomTree.remove( "PatientsSex" );
		}
	}

	transformOrTell<uint32_t>( prefix + "SIEMENS CSA HEADER/UsedChannelMask", "coilChannelMask", object, info );
	////////////////////////////////////////////////////////////////
	// interpret DWI data
	////////////////////////////////////////////////////////////////
	int32_t bValue;
	bool foundDiff = true;

	// find the B-Value
	if ( dicomTree.hasProperty( "DiffusionBValue" ) ) { //in case someone actually used the right Tag
		bValue = dicomTree.getValueAs<int32_t>( "DiffusionBValue" );
		dicomTree.remove( "DiffusionBValue" );
	} else if ( dicomTree.hasProperty( "SiemensDiffusionBValue" ) ) { //fallback for siemens
		bValue = dicomTree.getValueAs<int32_t>( "SiemensDiffusionBValue" );
		dicomTree.remove( "SiemensDiffusionBValue" );
	} else foundDiff = false;

	// If we do have DWI here, create a property diffusionGradient (which defaults to 0,0,0)
	if( foundDiff ) {
		if( checkDialect(dialects, "siemens") ) {
			LOG( Runtime, warning ) << "Removing acquisitionTime=" << util::MSubject( object.property( "acquisitionTime" ).toString( false ) ) << " from siemens DWI data as it is probably broken";
			object.remove( "acquisitionTime" );
		}

		bool foundGrad=false;
		if( dicomTree.hasProperty( "DiffusionGradientOrientation" ) ) {
			foundGrad= object.transform<util::fvector3>(prefix+"DiffusionGradientOrientation","diffusionGradient");
		} else if( dicomTree.hasProperty( "SiemensDiffusionGradientOrientation" ) ) {
			foundGrad= object.transform<util::fvector3>(prefix+"SiemensDiffusionGradientOrientation","diffusionGradient");
		} else {
			if(bValue)
				LOG( Runtime, warning ) << "Found no diffusion direction for DiffusionBValue " << util::MSubject( bValue );
			else {
				LOG(Runtime, info ) << "DiffusionBValue is 0, setting (non existing) diffusionGradient to " << util::fvector3{0,0,0};
				object.setValueAs("diffusionGradient",util::fvector3{0,0,0});
			}
		}

		if( bValue && foundGrad ) // if bValue is not zero multiply the diffusionGradient by it
			object.refValueAs<util::fvector3>("diffusionGradient")*=bValue;
	}


	//@todo fallback for GE/Philips
	////////////////////////////////////////////////////////////////
	// Do some sanity checks on redundant tags
	////////////////////////////////////////////////////////////////
	if ( dicomTree.hasProperty( util::istring( unknownTagName ) + "(0019,1015)" ) ) {
		const util::fvector3 org = object.getValueAs<util::fvector3>( "indexOrigin" );
		const util::fvector3 comp = dicomTree.getValueAs<util::fvector3>( util::istring( unknownTagName ) + "(0019,1015)" );

		if ( util::fuzzyEqualV(comp, org ) )
			dicomTree.remove( util::istring( unknownTagName ) + "(0019,1015)" );
		else
			LOG( Debug, warning )
					<< prefix + util::istring( unknownTagName ) + "(0019,1015):" << dicomTree.property( util::istring( unknownTagName ) + "(0019,1015)" )
					<< " differs from indexOrigin:" << object.property( "indexOrigin" ) << ", won't remove it";
	}

	if(
		dicomTree.hasProperty( "SIEMENS CSA HEADER/MosaicRefAcqTimes" ) &&
		dicomTree.hasProperty( util::istring( unknownTagName ) + "(0019,1029)" ) &&
		dicomTree.property( util::istring( unknownTagName ) + "(0019,1029)" ) == dicomTree.property( "SIEMENS CSA HEADER/MosaicRefAcqTimes" )
	) {
		dicomTree.remove( util::istring( unknownTagName ) + "(0019,1029)" );
	}

	if ( dicomTree.hasProperty( util::istring( unknownTagName ) + "(0051,100c)" ) ) { //@todo siemens only ?
		std::string fov = dicomTree.getValueAs<std::string>( util::istring( unknownTagName ) + "(0051,100c)" );
		float row, column;

		if ( std::sscanf( fov.c_str(), "FoV %f*%f", &column, &row ) == 2 ) {
			object.setValueAs( "fov", util::fvector3( {row, column, invalid_float} ) );
		}
	}
	
	auto windowCenterQuery=dicomTree.queryProperty("WindowCenter");
	auto windowWidthQuery=dicomTree.queryProperty("WindowCenter");
	if( windowCenterQuery && windowWidthQuery){
		util::ValueReference windowCenterVal=windowCenterQuery->front();
		util::ValueReference windowWidthVal=windowWidthQuery->front();
		double windowCenter,windowWidth;
		if(windowCenterVal->isFloat()){
			windowCenter=windowCenterVal->as<double>();
			dicomTree.remove("WindowCenter");
		} else 
			windowCenter=windowCenterVal->as<util::dlist>().front(); // sometimes there are actually multiple windows, use the first
			
		if(windowWidthVal->isFloat()){
			windowWidth=windowWidthVal->as<double>();
			dicomTree.remove("WindowWidth");
		} else 
			windowWidth = windowWidthVal->as<util::dlist>().front();
			
		object.setValueAs("window/min",windowCenter-windowWidth/2);
		object.setValueAs("window/max",windowCenter+windowWidth/2);
	}
}

data::Chunk ImageFormat_Dicom::readMosaic( data::Chunk source )
{
	// prepare some needed parameters
	const util::istring prefix = util::istring( ImageFormat_Dicom::dicomTagTreeName ) + "/";
	util::slist iType = source.getValueAs<util::slist>( prefix + "ImageType" );
	std::replace( iType.begin(), iType.end(), std::string( "MOSAIC" ), std::string( "WAS_MOSAIC" ) );
	util::istring NumberOfImagesInMosaicProp;

	if ( source.hasProperty( prefix + "SiemensNumberOfImagesInMosaic" ) ) {
		NumberOfImagesInMosaicProp = prefix + "SiemensNumberOfImagesInMosaic";
	} else if ( source.hasProperty( prefix + "SIEMENS CSA HEADER/NumberOfImagesInMosaic" ) ) {
		NumberOfImagesInMosaicProp = prefix + "SIEMENS CSA HEADER/NumberOfImagesInMosaic";
	}

	// All is fine, lets start
	uint16_t images;
	if(NumberOfImagesInMosaicProp.empty()){
		images = source.getSizeAsVector()[0]/ source.getValueAs<util::ilist>( prefix+"AcquisitionMatrix" ).front();
		images*=images;
		LOG(Debug,warning) << "Guessing number of slices in the mosaic as " << images << ". This might be to many";
	} else
		images = source.getValueAs<uint16_t>( NumberOfImagesInMosaicProp );
	
	const util::vector4<size_t> tSize = source.getSizeAsVector();
	const uint16_t matrixSize = std::ceil( std::sqrt( images ) );
	const util::vector3<size_t> size( {tSize[0] / matrixSize, tSize[1] / matrixSize, images} );

	LOG( Debug, info ) << "Decomposing a " << source.getSizeAsString() << " mosaic-image into a " << size << " volume";
	// fix the properties of the source (we 'll need them later)
	const util::fvector3 voxelGap = source.getValueAsOr("voxelGap",util::fvector3());
	const util::fvector3 voxelSize = source.getValueAs<util::fvector3>( "voxelSize" );
	const util::fvector3 rowVec = source.getValueAs<util::fvector3>( "rowVec" );
	const util::fvector3 columnVec = source.getValueAs<util::fvector3>( "columnVec" );
	//remove the additional mosaic offset
	//eg. if there is a 10x10 Mosaic, substract the half size of 9 Images from the offset
	const util::fvector3 fovCorr = ( voxelSize + voxelGap ) * size * ( matrixSize - 1 ) / 2; // @todo this will not include the voxelGap between the slices
	util::fvector3 &origin = source.refValueAs<util::fvector3>( "indexOrigin" );
	origin = origin + ( rowVec * fovCorr[0] ) + ( columnVec * fovCorr[1] );
	source.remove( NumberOfImagesInMosaicProp ); // we dont need that anymore
	source.setValueAs( prefix + "ImageType", iType );

	//store and remove acquisitionTime
	std::list<double> acqTimeList;
	std::list<double>::const_iterator acqTimeIt;

	bool haveAcqTimeList = source.hasProperty( prefix + "SIEMENS CSA HEADER/MosaicRefAcqTimes" );
	isis::util::timestamp acqTime;

	if( haveAcqTimeList ) {
		acqTimeList = source.getValueAs<std::list<double> >( prefix + "SIEMENS CSA HEADER/MosaicRefAcqTimes" );
		source.remove( prefix + "SIEMENS CSA HEADER/MosaicRefAcqTimes" );
		acqTimeIt = acqTimeList.begin();
		LOG( Debug, info ) << "The acquisition time offsets of the slices in the mosaic where " << acqTimeList;
	}

	if( source.hasProperty( "acquisitionTime" ) )acqTime = source.getValueAs<isis::util::timestamp>( "acquisitionTime" );
	else {
		LOG_IF( haveAcqTimeList, Runtime, info ) << "Ignoring SIEMENS CSA HEADER/MosaicRefAcqTimes because there is no acquisitionTime";
		haveAcqTimeList = false;
	}

	data::Chunk dest = source.cloneToNew( size[0], size[1], size[2] ); //create new 3D chunk of the same type
	static_cast<util::PropertyMap &>( dest ) = static_cast<const util::PropertyMap &>( source ); //copy _only_ the Properties of source
	// update origin
	dest.setValueAs( "indexOrigin", origin );

	// update fov
	if ( dest.hasProperty( "fov" ) ) {
		util::fvector3 &ref = dest.refValueAs<util::fvector3>( "fov" );
		ref[0] /= matrixSize;
		ref[1] /= matrixSize;
		ref[2] = voxelSize[2] * images + voxelGap[2] * ( images - 1 );
	}

	// for every slice add acqTime to Multivalue

	auto acqTimeQuery= dest.queryProperty( "acquisitionTime"); 
	if(acqTimeQuery && haveAcqTimeList) 
		*acqTimeQuery=util::PropertyValue(); //reset the selected ordering property to empty

	for ( size_t slice = 0; slice < images; slice++ ) {
		// copy the lines into the corresponding slice in the chunk
		for ( size_t line = 0; line < size[1]; line++ ) {
			const std::array<size_t,4> dpos = {0, line, slice, 0}; //begin of the target line
			const size_t column = slice % matrixSize; //column of the mosaic
			const size_t row = slice / matrixSize; //row of the mosaic
			const std::array<size_t,4> sstart{column *size[0], row *size[1] + line, 0, 0}; //begin of the source line
			const std::array<size_t,4> send{sstart[0] + size[0] - 1, row *size[1] + line, 0, 0}; //end of the source line
			source.copyRange( sstart, send, dest, dpos );
		}

		if(acqTimeQuery && haveAcqTimeList){
			auto newtime=acqTime +  std::chrono::milliseconds((std::chrono::milliseconds::rep)* ( acqTimeIt ) );
			acqTimeQuery->push_back(newtime);
			LOG(Debug,verbose_info) 
				<< "Computed acquisitionTime for slice " << slice << " as " << newtime
				<< "(" << acqTime << "+" <<  std::chrono::milliseconds((std::chrono::milliseconds::rep)* ( acqTimeIt ) );
			++acqTimeIt;
		}
	}

	return dest;
}

std::list<data::Chunk> ImageFormat_Dicom::load ( std::streambuf *source, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> progress ) {

	std::basic_stringbuf<char> buff_stream;
	boost::iostreams::copy(*source,buff_stream);
	const auto buff = buff_stream.str();
	
	data::ValueArray<uint8_t> wrap((uint8_t*)buff.data(),buff.length(),data::ValueArray<uint8_t>::NonDeleter());
	return load(wrap,formatstack,dialects,progress);
}

std::list< data::Chunk > ImageFormat_Dicom::load(const data::ByteArray source, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback )
{
	std::list< data::Chunk > ret;
	const char prefix[4]={'D','I','C','M'};
	if(memcmp(&source[128],prefix,4)!=0)
		throwGenericError("Prefix \"DICM\" not found");
	
	size_t meta_info_length = _internal::DicomElement(source,128+4,boost::endian::order::little).getValue()->as<uint32_t>();
	std::multimap<uint32_t,data::ValueArrayReference> data_elements;
	
	LOG(Debug,info)<<"Reading Meta Info begining at " << 158 << " length: " << meta_info_length-14;
	_internal::DicomElement m(source,158,boost::endian::order::little);
	util::PropertyMap meta_info=readStream(m,meta_info_length-14,data_elements);
	
	const auto transferSyntax= meta_info.getValueAsOr<std::string>("TransferSyntaxUID","1.2.840.10008.1.2");
	boost::endian::order endian;
	if(
		transferSyntax=="1.2.840.10008.1.2"  // Implicit VR Little Endian
		|| transferSyntax.substr(0,19)=="1.2.840.10008.1.2.1" // Explicit VR Little Endian
#ifdef HAVE_OPENJPEG
		|| transferSyntax=="1.2.840.10008.1.2.4.90" //JPEG 2000 Image Compression (Lossless Only)
#endif //HAVE_OPENJPEG
	){ 
		 endian=boost::endian::order::little;
	} else if(transferSyntax=="1.2.840.10008.1.2.2"){ //explicit big endian
		 endian=boost::endian::order::big;
	} else {
		LOG(Runtime,error) << "Sorry, transfer syntax " << transferSyntax <<  " is not (yet) supportet";
		ImageFormat_Dicom::throwGenericError("Unsupported transfer syntax");
	}

	//the "real" dataset
	LOG(Debug,info)<<"Reading dataset begining at " << 144+meta_info_length;
	_internal::DicomElement dataset_token(source,144+meta_info_length,boost::endian::order::little);
	
	util::PropertyMap props=_internal::readStream(dataset_token,source.getLength()-144-meta_info_length,data_elements);
	
	//extract CSA header from data_elements
	auto private_code=props.queryProperty("Private Code for (0029,1000)-(0029,10ff)");
	if(private_code && private_code->as<std::string>()=="SIEMENS CSA HEADER"){
		for(uint32_t csa_id=0x00291000;csa_id<0x00291100;csa_id+=0x10){
			auto found = data_elements.find(csa_id);
			if(found!=data_elements.end()){
				util::PropertyMap &subtree=props.touchBranch(private_code->as<std::string>().c_str());
				parseCSA(found->second->castToValueArray<uint8_t>(),subtree,dialects);
				data_elements.erase(found);
			} 
		}
	}

	//extract actual image data from data_elements
	std::list<data::ValueArrayReference> img_data;
	for(auto e_it=data_elements.find(0x7FE00010);e_it!=data_elements.end() && e_it->first==0x7FE00010;){
		img_data.push_back(e_it->second);
		data_elements.erase(e_it++);
	}
	
	if(img_data.empty()){
		throwGenericError("No image data found");
	} else {
		data::Chunk chunk(_internal::DicomChunk(std::move(img_data),transferSyntax,props));
	
		//we got a chunk from the file
		sanitise( chunk, dialects );
		const util::slist iType = chunk.getValueAs<util::slist>( util::istring( ImageFormat_Dicom::dicomTagTreeName ) + "/" + "ImageType" );

		if ( std::find( iType.begin(), iType.end(), "MOSAIC" ) != iType.end() ) { // if its a mosaic
			if( checkDialect(dialects, "keepmosaic") ) {
				LOG( Runtime, info ) << "This seems to be an mosaic image, but dialect \"keepmosaic\" was selected";
				ret.push_back( chunk );
			} else {
				ret.push_back( readMosaic( chunk ) );
			}
		} else if(checkDialect(dialects, "forcemosaic") ) 
			ret.push_back( readMosaic( chunk ) );
		else 
			ret.push_back( chunk );
		
		if( ret.back().hasProperty( "SiemensNumberOfImagesInMosaic" ) ) { // if its still there image was no mosaic, so I guess it should be used according to the standard
			ret.back().rename( "SiemensNumberOfImagesInMosaic", "SliceOrientation" );
		}

	} 
	
	return ret;
}

void ImageFormat_Dicom::write( const data::Image &/*image*/, const std::string &/*filename*/, std::list<util::istring> /*dialects*/, std::shared_ptr<util::ProgressFeedback> /*feedback*/ )
{
	throw( std::runtime_error( "writing dicom files is not yet supportet" ) );
}

ImageFormat_Dicom::ImageFormat_Dicom()
{
	//modify the dicionary
	// override known entries
	_internal::dicom_dict[0x00100010] = {"PN","PatientsName"};
	_internal::dicom_dict[0x00100030] = {"DA","PatientsBirthDate"};
	_internal::dicom_dict[0x00100040] = {"CS","PatientsSex"};
	_internal::dicom_dict[0x00101010] = {"AS","PatientsAge"};
	_internal::dicom_dict[0x00101030] = {"DS","PatientsWeight"};

	_internal::dicom_dict[0x00080008] = {"CS","ImageType"};
	_internal::dicom_dict[0x00081050] = {"PN","PerformingPhysiciansName"};

	// override some Siemens specific stuff because it is SliceOrientation in the standard and mosaic-size for siemens - we will figure out while sanitizing
	_internal::dicom_dict[0x0019100a] = {"--","SiemensNumberOfImagesInMosaic"};
	_internal::dicom_dict[0x0019100c] = {"--","SiemensDiffusionBValue"};
	_internal::dicom_dict[0x0019100e] = {"--","SiemensDiffusionGradientOrientation"};
	_internal::dicom_dict.erase(0x00211010); // dcmtk says its ImageType but it isn't (at least not on Siemens)

	for( unsigned short i = 0x0010; i <= 0x00FF; i++ ) {
		_internal::dicom_dict[0x00290000 + i] = 
			{"--",util::istring( "Private Code for " ) + _internal::id2Name( 0x0029, i << 8 ) + "-" + _internal::id2Name( 0x0029, ( i << 8 ) + 0xFF )};
	}
	
	//http://www.healthcare.siemens.com/siemens_hwem-hwem_ssxa_websites-context-root/wcm/idc/groups/public/@global/@services/documents/download/mdaw/mtiy/~edisp/2008b_ct_dicomconformancestatement-00073795.pdf
	//@todo do we need this
	for( unsigned short i = 0x0; i <= 0x02FF; i++ ) {
		char buff[7];
		std::snprintf(buff,7,"0x%.4X",i);
		_internal::dicom_dict[(0x6000<<16)+ i] = {"--",util::PropertyMap::PropPath("DICOM overlay info") / util::PropertyMap::PropPath(buff)};
	}
	_internal::dicom_dict[0x60003000] = {"--","DICOM overlay data"};
}

}
}

isis::image_io::FileFormat *factory()
{
	return new isis::image_io::ImageFormat_Dicom;
}
