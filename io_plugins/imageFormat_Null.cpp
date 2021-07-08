#include <isis/core/io_interface.h>

#include <time.h>

namespace isis
{
namespace image_io
{
namespace _internal{
template<typename T> void setval(std::shared_ptr<T> ptr,uint32_t s, uint32_t t, uint32_t ypselon){
	const T val=std::is_signed_v<T> ?
	                    uint8_t(127 - s * 10):
	                    uint8_t(255 - s * 10);
	std::fill(ptr.get()+ypselon+10,ptr.get()+ypselon+40,val);
	*ptr.get()=t*40;
}
template<typename T> void setval(std::shared_ptr<util::color<T>> ptr,uint32_t s, uint32_t t, uint32_t ypselon){
	const util::color<T> val{uint8_t(255 - s * 10),uint8_t(255 - s * 10),uint8_t(255 - s * 10)};
	std::fill(ptr.get()+ypselon+10,ptr.get()+ypselon+40,val);
	*ptr.get()=util::color<T>{uint8_t(t*40),uint8_t(t*40),uint8_t(t*40)};
}
template<typename T> void setval(std::shared_ptr<util::vector3<T>> ptr,uint32_t s, uint32_t t, uint32_t ypselon){
	const util::vector3<T>  val{T(255.f - s * 10),T(255.f - s * 10),T(255.f - s * 10)};
	std::fill(ptr.get()+ypselon+10,ptr.get()+ypselon+40,val);
	*ptr.get()=util::vector3<T>{T(t*40.f),T(t*40.f),T(t*40.f)};
}
template<typename T> void setval(std::shared_ptr<util::vector4<T>> ptr,uint32_t s, uint32_t t, uint32_t ypselon){
	const util::vector4<T> val{T(255.f - s * 10),T(255.f - s * 10),T(255.f - s * 10),T(255.f - s * 10)};
	std::fill(ptr.get()+ypselon+10,ptr.get()+ypselon+40,val);
	*ptr.get()=util::vector4<T>{T(t*40.f),T(t*40.f),T(t*40.f),T(t*40.f)};
}
template<> void setval<bool>(std::shared_ptr<bool> ptr,uint32_t, uint32_t t, uint32_t ypselon){
	std::fill(ptr.get()+ypselon+10,ptr.get()+ypselon+40,true);
}

}
class ImageFormat_Null: public FileFormat
{
	static const size_t timesteps = 20;
	std::list<data::Chunk> makeImageByID(uint32_t ID, unsigned short size, uint16_t sequence_offset, std::string desc ) {
		//##################################################################################################
		//## standard null image
		//##################################################################################################
		std::list<data::Chunk> ret;

		for ( uint32_t t = 0; t < timesteps; t++ ) {
			for ( uint32_t s = 0; s < size; s++ ) {

				data::Chunk ch(data::ValueArray::createByID(ID, size*size), size, size );
				ch.setValueAs( "indexOrigin", util::fvector3{ 0, -150 / 2, s * 110.f / size - 100 / 2 } ); //don't use s*100./size-100/2 because we want a small gap
				ch.setValueAs( "sequenceNumber", sequence_offset+ID );
				ch.setValueAs( "performingPhysician", std::string( "Dr. Jon Doe" ) );
				ch.setValueAs( "rowVec",    util::fvector3{  cosf( M_PI / 8 ), -sinf( M_PI / 8 ) } ); //rotated by pi/8
				ch.setValueAs( "columnVec", util::fvector3{  sinf( M_PI / 8 ),  cosf( M_PI / 8 ) } ); // @todo also rotate the sliceVec
				ch.setValueAs( "voxelSize", util::fvector3{ 150.f / size, 150.f / size, 100.f / size } );
				ch.setValueAs( "repetitionTime", 1234 );
				ch.setValueAs( "sequenceDescription", desc );

				ch.setValueAs( "acquisitionNumber", s+size*t );
				ch.setValueAs( "acquisitionTime", s+size*t );
				ch.setValueAs( "typeID", ID );

				ch.visit([s,t,size](auto ptr){
					    for ( int y = 10; y < 40; y++ ){
							const auto ypselon=y*size;
							_internal::setval(ptr,s,t,ypselon);
						}
				    }
				);

				ret.push_back( ch );
			}
		}

		return ret;
	}
protected:
	util::istring suffixes( io_modes /*modes=both*/ )const override {return ".null"; }
public:
	std::string getName()const override {
		return "Null";
	}

	std::list<data::Chunk>
	load( const std::filesystem::path &filename, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback ) override  {

		size_t size = 50;

		std::list<data::Chunk> ret;
		const auto types=util::getTypeMap(true);
		if(feedback)
			feedback->show(types.size()*2,"Loading artificial images..");

		//sequential images
		for(const auto &t:types){
			ret.splice( ret.end(),
			    makeImageByID(t.first, size, 0, std::string("sequencial ") + t.second + " Image" )
			);
			if(feedback)feedback->progress();
		}

		// interleaved images
		for(const auto &t:types){
			std::list<data::Chunk> loaded= makeImageByID(t.first, size, 100, std::string("interleaved ") + t.second + " Image" ) ;
			std::list< data::Chunk >::iterator ch = loaded.begin();

			for ( size_t t = 0; t < timesteps; t++ ) {
				//even numbers
				for ( uint32_t a = 0; a < ( size / 2. ); a++ ) { //eg. size==5  2 < (5/2.) => 2 < 2.5 == true
					( ch++ )->setValueAs<uint32_t>( "acquisitionNumber", a * 2 + t * size );
				}

				//uneven numbers
				for ( uint32_t a = 0; a < ( size / 2 ); a++ ) { //eg. size==5  2 < (5/2) => 2 < 2 == false
					( ch++ )->setValueAs<uint32_t>( "acquisitionNumber", a * 2 + 1 + t * size );
				}
			}

			assert( ch == loaded.end() );
			ret.splice( ret.end(), loaded );
			if(feedback)feedback->progress();
		}

		return ret;
	}

	void write( const data::Image &img, const std::string &filename, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback )  override {
		data::Image image = img;

		// set by the core, thus the newChunks cannot have one
		image.remove( "source" );
		image.remove( "voxelGap" );
		image.remove( "sliceVec" );

		size_t size = image.getSizeAsVector()[0];
		std::list< data::Chunk > newChunks;
		std::vector< data::Chunk > oldChunks = image.copyChunksToVector();
		std::list< data::Chunk >::iterator iCh;
		uint32_t s = 0;

		auto seqNumber = image.getValueAs<int>( "sequenceNumber" );

		if(seqNumber<100){
			//image 0 is a "normal" image
			newChunks = makeImageByID(seqNumber, size, 0, "" ) ;
		} else {//image 1 is a "interleaved" image
			newChunks = makeImageByID(seqNumber-100, size, 100, "" ) ;
			iCh = newChunks.begin();

			for ( uint32_t t = 0; t < timesteps; t++ ) {
				//even numbers
				for ( uint32_t s = 0; s < ( size / 2. ); s++ ) { //eg. size==5  2 < (5/2.) => 2 < 2.5 == true
					( iCh++ )->setValueAs<uint32_t>( "acquisitionNumber", s * 2 + t * size );
				}

				//uneven numbers
				for ( uint32_t s = 0; s < ( size / 2 ); s++ ) { //eg. size==5  2 < (5/2) => 2 < 2 == false
					( iCh++ )->setValueAs<uint32_t>( "acquisitionNumber", s * 2 + 1 + t * size );
				}
			}

			assert( iCh == newChunks.end() );
		}

		if( newChunks.size() != oldChunks.size() )
			throwGenericError( "ammount of chunks differs" );

		std::list< data::Chunk >::iterator newCH = newChunks.begin();

		for( size_t i = 0; i < oldChunks.size(); ++i, ++newCH ) {
			// check for the orientation seperately
			if(
			    util::fuzzyEqualV(newCH->getValueAs<util::fvector3>( "columnVec" ), oldChunks[i].getValueAs<util::fvector3>( "columnVec" ) ) == false ||
			    util::fuzzyEqualV(newCH->getValueAs<util::fvector3>( "rowVec" ), oldChunks[i].getValueAs<util::fvector3>( "rowVec" ) ) == false
			) {
				throwGenericError( "orientation is not equal" );
			} else {
				newCH->remove( "rowVec" );
				newCH->remove( "columnVec" );
				oldChunks[i].remove( "rowVec" );
				oldChunks[i].remove( "columnVec" );
			}

			util::PropertyMap::DiffMap metaDiff = newCH->getDifference( oldChunks[i] );
			metaDiff.erase("sequenceDescription");//we didn't set this in newChunks'

			if( metaDiff.size() ) {
				std::cerr << metaDiff << std::endl;
				throwGenericError( "differences in the metainformation found" );
			}

			if( newCH->getTypeID() != oldChunks[i].getTypeID() )
				throwGenericError( std::string("Expected ") + newCH->typeName() + " but got " + oldChunks[i].typeName() );

			if( newCH->compare( oldChunks[i] ) ) {
				throwGenericError( "voxels do not fit" );
			}
		}
	}
};
}
}
isis::image_io::FileFormat *factory()
{
	return new isis::image_io::ImageFormat_Null();
}
