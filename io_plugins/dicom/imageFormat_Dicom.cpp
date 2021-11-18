#include "imageFormat_Dicom.hpp"
#include <isis/core/common.hpp>
#include <isis/core/istring.hpp>

#include <boost/iostreams/copy.hpp>

namespace isis::image_io
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
util::PropertyMap readStream(DicomElement &token,size_t stream_len,std::multimap<uint32_t,data::ValueArray> &data_elements);
void readDataItems(DicomElement &token, std::multimap<uint32_t, data::ValueArray> &data_elements){
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
util::PropertyMap readSequence(DicomElement &token,std::multimap<uint32_t,data::ValueArray> &data_elements,const std::function<bool(DicomElement &token)>& delimiter);
util::PropertyMap readStream(DicomElement &token,size_t stream_len,std::multimap<uint32_t,data::ValueArray> &data_elements){
	size_t start=token.getPosition();
	util::PropertyMap ret;

	do{
		//break the loop if we find an item delimiter
		if(token.getID32()==0xFFFEE00D){
			token.next(token.getPosition()+8);
			break;
		}

		const std::string vr=token.getVR();
		if(vr=="OB" || vr=="OW"){
			const uint32_t len=token.getLength();
			if(len==0xFFFFFFFF){ // itemized data of undefined length
				readDataItems(token,data_elements);
			} else {
				std::multimap<uint32_t,data::ValueArray>::iterator inserted;
				if(vr=="OW")
					inserted=data_elements.insert({token.getID32(),token.dataAs<uint16_t>()});
				else
					inserted=data_elements.insert({token.getID32(),token.dataAs<uint8_t>()});

				LOG(Debug,info)
				    << "Found " << inserted->second.typeName() << "-data for " << token.getIDString() << " at " << token.getPosition()
				    << " it is " <<token.getLength() << " bytes long";
			}
			if(!token.next())
			    break;
		}
		else if(vr=="SQ")
		{ // http://dicom.nema.org/dicom/2013/output/chtml/part05/sect_7.5.html

            uint32_t len=token.getLength();
			const auto name=token.getName();

			//get to first item
			if(token.implicit_vr)
				token.next(token.getPosition()+4+4);//implicit SQ (4 bytes tag-id + 4 bytes length)
			else
				token.next(token.getPosition()+4+2+2+4);//explicit SQ (4 bytes tag-id + 2bytes "SQ" + 2bytes reserved + 4 bytes length)

			const size_t start_sq=token.getPosition();

            std::function<bool(DicomElement &token)> delimiter;

            if(len==0xffffffff){
                LOG(Debug,verbose_info) << "Sequence of undefined length found (" << name << "), looking for items at " << start_sq;
                delimiter=[](DicomElement &t){
                    if(t.getID32()==0xFFFEE0DD) { //sequence delimiter
                        t.next(t.getPosition()+8); // eat the delimiter and exit
                        return true;
                    } else
                        return false;
                };
            } else {
                assert(len<stream_len); //sequence length mus be shorter than the stream its in
                LOG(Debug,verbose_info) << "Sequence of length " << len << " found (" << name << "), looking for items at " << start_sq;
                delimiter=[start_sq,len](DicomElement &t){return t.getPosition()>=start_sq+len;};
            }

            util::PropertyMap subtree=readSequence(token,data_elements,delimiter);
            ret.touchBranch(name).transfer(subtree);
			LOG(Debug,verbose_info) << "Sequence " << name << " started at " << start_sq << " finished, continuing at " << token.getPosition();
		}
		else
		{
			auto value=token.getValue(vr);
			if(value){
				ret.touchProperty(token.getName())=*value;
			}
            if(!token.next())
                break;
        }
	}while(token.getPosition()<start+stream_len);
	return ret;
}

util::PropertyMap readSequence(DicomElement &token,std::multimap<uint32_t,data::ValueArray> &data_elements, const std::function<bool(DicomElement &token)>& delimiter){
    util::PropertyMap ret;
    //load items (which themself again are made of tags)
    while(!delimiter(token) && !token.eof()){
        assert(token.getID32()==0xFFFEE000);//must be an item-tag
        const size_t item_len=token.getLength();
        token.next(token.getPosition()+8);
        util::PropertyMap buffer=readStream(token,item_len,data_elements);
        ret.transfer(buffer);
    }
    return ret;
}
template<typename T>
data::ValueArray repackValueArray(data::ValueArray &data){
	//the VR of pixel data not necessarily fits the actual pixel representation so we have to repack the raw byte data
	const auto new_ptr=std::static_pointer_cast<T>(data.getRawAddress());
	const size_t bytes = data.getLength()*data.bytesPerElem();
	return data::ValueArray(new_ptr, bytes/sizeof(T));
}
template<typename T>
data::ValueArray repackValueArray(data::ValueArray &data, bool invert){
	data::ValueArray ret=repackValueArray<T>(data);

	if(invert){
		auto minmax=ret.getMinMax();
		const T min=minmax.first.as<T>();
		const T max=minmax.second.as<T>();
		for(auto it=ret.beginTyped<T>();it!=ret.endTyped<T>();++it){
			const T dist_from_min=*it-min;
			*it=max-1-dist_from_min;
		}
	}
	return ret;
}

class DicomChunk : public data::Chunk
{
	static data::Chunk getUncompressedPixel(data::ValueArray &data, const util::PropertyMap &props){
		auto rows=props.getValueAs<uint32_t>("Rows");
		auto columns=props.getValueAs<uint32_t>("Columns");
		//Number of Frames: 0028,0008

		//repack the pixel data into proper type
		data::ValueArray pixel;
		auto color=props.getValueAs<std::string>("PhotometricInterpretation");
		auto bits_allocated=props.getValueAs<uint8_t>("BitsAllocated");
		auto signed_values=props.getValueAsOr<bool>("PixelRepresentation",false);

		//@todo add more "color-modes" https://dicom.innolitics.com/ciods/mr-image/image-pixel/00280004
		if(color=="COLOR" || color=="RGB"){
			assert(signed_values==false);
			switch(bits_allocated){
			    case  8:pixel=repackValueArray<util::color24>(data);break;
			    case 16:pixel=repackValueArray<util::color48>(data);break;
			    default:LOG(Runtime,error) << "Unsupported bit-depth "<< bits_allocated << " for color image";
			}
		}else if(color=="MONOCHROME2" || color=="MONOCHROME1"){
			bool invert = (color=="MONOCHROME1");
			switch(bits_allocated){
			    case  8:pixel=signed_values? repackValueArray< int8_t>(data):repackValueArray< uint8_t>(data,invert);break;
			    case 16:pixel=signed_values? repackValueArray<int16_t>(data):repackValueArray<uint16_t>(data,invert);break;
			    case 32:pixel=signed_values? repackValueArray<int32_t>(data):repackValueArray<uint32_t>(data,invert);break;
			    default:LOG(Runtime,error) << "Unsupported bit-depth "<< bits_allocated << " for greyscale image";
			}
		}else {
			LOG(Runtime,error) << "Unsupported photometric interpretation " << color;
			ImageFormat_Dicom::throwGenericError("bad pixel type");
		}

		// create a chunk of the proper type
		assert(pixel.getLength()==rows*columns);
		data::Chunk ret(pixel,columns,rows);
		return ret;
	}
public:
	DicomChunk(data::ValueArray &data, const std::string &transferSyntax, const util::PropertyMap &props)
	{
#ifdef HAVE_OPENJPEG
		if(transferSyntax=="1.2.840.10008.1.2.4.90"){ //JPEG 2K
			assert(data.getTypeID()==util::typeID<uint8_t>());
			static_cast<data::Chunk&>(*this)=_internal::getj2k(data::ByteArray(data.castTo<uint8_t>(),data.getLength()));

			LOG(Runtime,info)
			    << "Created " << this->getSizeAsString() << "-Image of type " << this->typeName()
			    << " from a " << data.getLength() << " bytes j2k stream";
		} else
#endif //HAVE_OPENJPEG
		{
			static_cast<data::Chunk&>(*this)=getUncompressedPixel(data,props);
			LOG(Runtime,info)
			    << "Created " << this->getSizeAsString() << "-Image of type " << this->typeName()
			    << " from a " << data.getLength() << " bytes of raw data";
		}
		this->touchBranch(ImageFormat_Dicom::dicomTagTreeName)=props;
	}
};

// stolen from https://github.com/malaterre/GDCM/blob/e501d71938a0889f55885e4401fbfe60a8b7c4bd/Examples/Cxx/rle2img.cxx
void delta_decode(const char *inbuffer, size_t length, data::TypedArray<uint16_t> &outbuffer)
{
	// RLE pass
	std::vector<char> temp;
	uint16_t *output = outbuffer.begin();
	for(size_t i = 0; i < length; ++i)
	{
		if( inbuffer[i] == (char)0xa5 )
		{
			//unsigned char repeat = (unsigned char)inbuffer[i+1] + 1;
			//assert( (unsigned char)inbuffer[i+1] != 255 );
			int repeat = (unsigned char)inbuffer[i+1] + 1;
			char value = inbuffer[i+2];
			while(repeat)
			{
				temp.push_back( value );
				--repeat;
			}
			i+=2;
		}
		else
		{
			temp.push_back( inbuffer[i] );
		}
	}

	// Delta encoding pass
	unsigned short delta = 0;
	for(size_t i = 0; i < temp.size(); ++i)
	{
		if( temp[i] == 0x5a )
		{
			unsigned char v1 = (unsigned char)temp[i+1];
			unsigned char v2 = (unsigned char)temp[i+2];
			unsigned short value = (unsigned short)(v2 * 256 + v1);
			*(output++) = value;
			delta = value;
			i+=2;
		}
		else
		{
			unsigned short value = (unsigned short)(temp[i] + delta);
			*(output++) = value;
			delta = value;
		}
		//assert( output[output.size()-1] == ref[output.size()-1] );
	}
}

}

const char ImageFormat_Dicom::dicomTagTreeName[] = "DICOM";
const char ImageFormat_Dicom::unknownTagName[] = "UnknownTag/";

std::list<util::istring> ImageFormat_Dicom::suffixes(io_modes modes )const
{
	if( modes == write_only )
		return {};
	else
		return {".ima",".dcm"};
}
std::string ImageFormat_Dicom::getName()const {return "Dicom";}
std::list<util::istring> ImageFormat_Dicom::dialects()const {return {"siemens","withExtProtocols","nocsa","keepmosaic","forcemosaic"};}


void ImageFormat_Dicom::sanitise( util::PropertyMap &object, const std::list<util::istring>& dialects )
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
		const auto dt=dicomTree.getValueAs<util::date>("StudyDate");
		const auto tm=dicomTree.getValueAs<util::timestamp>("StudyTime");
		    object.setValueAs("studyStart",tm+dt.time_since_epoch());
			dicomTree.remove("StudyTime");
			dicomTree.remove("StudyDate");
	}

	transformOrTell<int32_t>  ( prefix + "SeriesNumber",     "sequenceNumber",     object, warning );
	transformOrTell<uint16_t>  ( prefix + "PatientAge",     "subjectAge",     object, info );
	transformOrTell<std::string>( prefix + "SeriesDescription", "sequenceDescription", object, warning );
	transformOrTell<std::string>( prefix + "PatientName",     "subjectName",        object, info );
	transformOrTell<util::date>       ( prefix + "PatientBirthDate", "subjectBirth",       object, info );
	transformOrTell<uint16_t>  ( prefix + "PatientWeight",   "subjectWeigth",      object, info );
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
			auto CSA_SliceRes = object.queryValueAs<float>( "DICOM/CSASeriesHeaderInfo/SliceResolution" );
			if(CSA_SliceRes)
				voxelSize[2] = 1 /  *CSA_SliceRes;
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
		auto buff = dicomTree.getValueAs<util::dlist>( "ImageOrientationPatient" );

		if ( buff.size() == 6 ) {
			util::fvector3 row, column;
			auto b = buff.begin();

			for ( int i = 0; i < 3; i++ )row[i] = float(*b++);

			for ( int i = 0; i < 3; i++ )column[i] = float(*b++);

			object.setValueAs( "rowVec" , row );
			object.setValueAs( "columnVec", column );
			dicomTree.remove( "ImageOrientationPatient" );
		} else {
			LOG( Runtime, error ) << "Could not extract row- and columnVector from " << dicomTree.property( "ImageOrientationPatient" );
		}

		if( object.hasProperty( prefix + "SIEMENS CSA HEADER/SliceNormalVector" ) && !object.hasProperty( "sliceVec" ) ) {
			LOG( Debug, info ) << "Extracting sliceVec from SIEMENS CSA HEADER/SliceNormalVector " << dicomTree.property( "SIEMENS CSA HEADER/SliceNormalVector" );
			auto list = dicomTree.getValueAs<util::dlist >( "SIEMENS CSA HEADER/SliceNormalVector" );
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
	} else {
		auto CSA_SliceNumber = object.queryValueAs<int32_t>( prefix + "SIEMENS CSA HEADER/ProtocolSliceNumber" );
		auto CSA_SliceRes =object.queryValueAs<float>("DICOM/CSASeriesHeaderInfo/SliceResolution");

		if( CSA_SliceNumber && CSA_SliceRes ){
			util::fvector3 orig {0,0,float(*CSA_SliceNumber) / *CSA_SliceRes	};
			LOG(Runtime, info) << "Synthesize missing indexOrigin from SIEMENS CSA HEADER/ProtocolSliceNumber as " << orig;
			object.setValueAs("indexOrigin", orig);
		} else {
			object.setValueAs( "indexOrigin", util::fvector3() );
			LOG( Runtime, warning ) << "Making up indexOrigin, because the image lacks this information";
		}
	}

	transformOrTell<uint32_t>( prefix + "InstanceNumber", "acquisitionNumber", object, error );

	if( dicomTree.hasProperty( "AcquisitionNumber" )){
		if(dicomTree.property("AcquisitionNumber").eq(object.property( "acquisitionNumber" )))
			dicomTree.remove( "AcquisitionNumber" );
	}

	if ( hasOrTell( prefix + "PatientSex", object, info ) ) {
		util::Selection isisGender({"male", "female", "other"} );
		bool set = false;

		switch ( dicomTree.getValueAs<std::string>( "PatientSex" )[0] ) {
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
			LOG( Runtime, warning ) << "Dicom gender code " << util::MSubject( object.property( prefix + "PatientSex" ) ) <<  " not known";
		}

		if( set ) {
			object.setValueAs( "subjectGender", isisGender);
			dicomTree.remove( "PatientSex" );
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
		const auto org = object.getValueAs<util::fvector3>( "indexOrigin" );
		const auto comp = dicomTree.getValueAs<util::fvector3>( util::istring( unknownTagName ) + "(0019,1015)" );

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
		auto fov = dicomTree.getValueAs<std::string>( util::istring( unknownTagName ) + "(0051,100c)" );
		float row, column;

		if ( std::sscanf( fov.c_str(), "FoV %f*%f", &column, &row ) == 2 ) {
			object.setValueAs( "fov", util::fvector3( {row, column, invalid_float} ) );
		}
	}

	auto windowCenterQuery=dicomTree.queryProperty("WindowCenter");
	auto windowWidthQuery=dicomTree.queryProperty("WindowWidth");
	if( windowCenterQuery && windowWidthQuery){
		util::Value windowCenterVal=windowCenterQuery->front();
		util::Value windowWidthVal=windowWidthQuery->front();
		double windowCenter,windowWidth;
		if(windowCenterVal.isFloat()){
			windowCenter=windowCenterVal.as<double>();
			dicomTree.remove("WindowCenter");
		} else
			windowCenter=windowCenterVal.as<util::dlist>().front(); // sometimes there are actually multiple windows, use the first

		if(windowWidthVal.isFloat()){
			windowWidth=windowWidthVal.as<double>();
			dicomTree.remove("WindowWidth");
		} else
			windowWidth = windowWidthVal.as<util::dlist>().front();

		object.setValueAs("window/min",windowCenter-windowWidth/2);
		object.setValueAs("window/max",windowCenter+windowWidth/2);
	}
}

data::Chunk ImageFormat_Dicom::readMosaic( data::Chunk source )
{
	// prepare some needed parameters
	const util::istring prefix = util::istring( ImageFormat_Dicom::dicomTagTreeName ) + "/";
	auto iType = source.getValueAs<util::slist>( prefix + "ImageType" );
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
	const auto voxelSize = source.getValueAs<util::fvector3>( "voxelSize" );
	const auto rowVec = source.getValueAs<util::fvector3>( "rowVec" );
	const auto columnVec = source.getValueAs<util::fvector3>( "columnVec" );
	//remove the additional mosaic offset
	//eg. if there is a 10x10 Mosaic, subtract the half size of 9 Images from the offset
	const util::fvector3 fovCorr = ( voxelSize + voxelGap ) * size * ( matrixSize - 1 ) / 2; // @todo this will not include the voxelGap between the slices
	auto &origin = source.refValueAs<util::fvector3>( "indexOrigin" );
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
		auto &ref = dest.refValueAs<util::fvector3>( "fov" );
		ref[0] /= float(matrixSize);
		ref[1] /= float(matrixSize);
		ref[2] = voxelSize[2] * float(images) + voxelGap[2] * float( images - 1 );
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

	const std::shared_ptr<uint8_t> p((uint8_t*)buff.data(),data::ValueArray::NonDeleter());
	data::ByteArray wrap(p,buff.length());
	return load(wrap,formatstack,dialects,progress);
}

std::list< data::Chunk > ImageFormat_Dicom::load(data::ByteArray source, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback )
{
	std::list< data::Chunk > ret;
	const char prefix[4]={'D','I','C','M'};
	if(memcmp(&source[128],prefix,4)!=0)
		throwGenericError("Prefix \"DICM\" not found");

	size_t meta_info_length = _internal::DicomElement(source,128+4,boost::endian::order::little,false).getValue()->as<uint32_t>();
	std::multimap<uint32_t,data::ValueArray> data_elements;

	LOG(Debug,info)<<"Reading Meta Info beginning at " << 158 << " length: " << meta_info_length-14;
	_internal::DicomElement m(source,158,boost::endian::order::little,false);
	util::PropertyMap meta_info=readStream(m,meta_info_length-14,data_elements);

	const auto transferSyntax= meta_info.getValueAsOr<std::string>("TransferSyntaxUID","1.2.840.10008.1.2");
	bool implicit_vr=false;
	if(transferSyntax=="1.2.840.10008.1.2"){  // Implicit VR Little Endian
		 implicit_vr=true;
	} else if(
	    transferSyntax.substr(0,19)=="1.2.840.10008.1.2.1" // Explicit VR Little Endian
#ifdef HAVE_OPENJPEG
	    || transferSyntax=="1.2.840.10008.1.2.4.90" //JPEG 2000 Image Compression (Lossless Only)
#endif //HAVE_OPENJPEG
	){
//	} else if(transferSyntax=="1.2.840.10008.1.2.2"){ //explicit big endian
	} else if(transferSyntax=="1.3.46.670589.33.1.4.1"){ //CT-private-ELE (little endian explicit VR)
	} else {
		LOG(Runtime,error) << "Sorry, transfer syntax " << transferSyntax <<  " is not (yet) supported";
		ImageFormat_Dicom::throwGenericError("Unsupported transfer syntax");
	}

	//the "real" dataset
	LOG(Debug,info)<<"Reading dataset begining at " << 144+meta_info_length;
	_internal::DicomElement dataset_token(source,144+meta_info_length,boost::endian::order::little,implicit_vr);

	util::PropertyMap props=_internal::readStream(dataset_token,source.getLength()-144-meta_info_length,data_elements);

	//extract CSA header from data_elements
	auto private_code=props.queryProperty("Private Code for (0029,1000)-(0029,10ff)");
	if(private_code && private_code->as<std::string>()=="SIEMENS CSA HEADER"){
		for(uint32_t csa_id=0x00291000;csa_id<0x00291100;csa_id+=0x10){
			auto found = data_elements.find(csa_id);
			if(found!=data_elements.end()){
				util::PropertyMap &subtree=props.touchBranch(private_code->as<std::string>().c_str());
				parseCSA(data::ByteArray(found->second.castTo<uint8_t>(),found->second.getLength()),subtree,dialects);
				data_elements.erase(found);
			}
		}
	}

	//extract actual image data from data_elements
	std::list<data::ValueArray> img_data;
	if(transferSyntax=="1.3.46.670589.33.1.4.1"){  // CT-private-ELE stores image data elsewhere
		for(auto e_it = data_elements.find(0x07A1100A); e_it != data_elements.end() && e_it->first == 0x07A1100A;){
			auto compression= props.getValueAs<std::string>("UnknownTag/(07a1,1011)");
			LOG(Runtime, info) << "Found CT-private-ELE image data at " << e_it->first << " compression is " << compression;
			if(compression == "PMSCT_RLE1" ){
				const char *in=reinterpret_cast<const char *>(e_it->second.castTo<uint8_t>().get());
				data::TypedArray<uint16_t> out(props.getValueAs<uint32_t>("Rows")*props.getValueAs<uint32_t>("Columns"));
				_internal::delta_decode(in,e_it->second.getLength(),out);
				img_data.push_back(std::move(out));
				data_elements.erase(e_it++);
			} else
				LOG(Runtime,error) << "Unknown compression for CT-private-ELE image data.";
		}
	}
	else {
		for(auto e_it = data_elements.find(0x7FE00010); e_it != data_elements.end() && e_it->first == 0x7FE00010;){
			img_data.push_back(e_it->second);
			data_elements.erase(e_it++);
		}
	}

	if(img_data.empty()){
		throwGenericError("No image data found");
	} else {
		LOG_IF(img_data.size()>1,Runtime,error) << "There is more than one image in the source, will only use the first";
		data::Chunk chunk(_internal::DicomChunk(img_data.front(),transferSyntax,props));

		//we got a chunk from the file
		sanitise( chunk, dialects );
		const auto iType = chunk.queryValueAs<util::slist>( util::istring( ImageFormat_Dicom::dicomTagTreeName ) + "/" + "ImageType");


		//handle philips scaling
		data::scaling_pair philps_scale(1,0);
		auto ri = chunk.queryValueAs<float>("DICOM/Philips private sequence/Philips private sequence/RescaleIntercept");
		auto rs = chunk.queryValueAs<float>("DICOM/Philips private sequence/Philips private sequence/RescaleSlope");

		auto si = chunk.queryValueAs<float>("DICOM/UnknownTag/(2005,100d)");
		auto ss = chunk.queryValueAs<float>("DICOM/UnknownTag/(2005,100e)"); //default 1

		if(ss){
			if(si){
				philps_scale.offset = -(*si / *ss);
			} else if(ri && rs){ // if we don't have si we can reconstruct it from ri and rs
				philps_scale.offset=(*ri / *rs) / *ss;
			}
			philps_scale.scale = 1 / *ss;
		}

		if(philps_scale.isRelevant()) {
			LOG(Runtime, info) << "Applying Philips scaling of " << philps_scale << " on data";
			chunk.convertToType(util::typeID<float>(), philps_scale);
		}

		//handle siemens mosaic data
		if ( iType && std::find( iType->begin(), iType->end(), "MOSAIC" ) != iType->end() ) { // if we have an image type and its a mosaic
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
	throw( std::runtime_error( "writing dicom files is not yet supported" ) );
}

ImageFormat_Dicom::ImageFormat_Dicom()
{
	for( unsigned short i = 0x0010; i <= 0x00FF; i++ ) {
		tag(0x00290000 + i) =
		    {"--",util::istring( "Private Code for " ) + _internal::id2Name( 0x0029, i << 8 ) + "-" + _internal::id2Name( 0x0029, ( i << 8 ) + 0xFF )};
	}

	//http://www.healthcare.siemens.com/siemens_hwem-hwem_ssxa_websites-context-root/wcm/idc/groups/public/@global/@services/documents/download/mdaw/mtiy/~edisp/2008b_ct_dicomconformancestatement-00073795.pdf
	//@todo do we need this
	for( unsigned short i = 0x0; i <= 0x02FF; i++ ) {
		char buff[7];
		std::snprintf(buff,7,"0x%.4X",i);
		tag((0x6000<<16)+ i) = {"--",util::PropertyMap::PropPath("DICOM overlay info") / util::PropertyMap::PropPath(buff)};
	}
	tag(0x60003000) = {"--","DICOM overlay data"};
}

}

isis::image_io::FileFormat *factory()
{
	return new isis::image_io::ImageFormat_Dicom;
}
