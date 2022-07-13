/*
 *    <one line to give the program's name and a brief idea of what it does.>
 *    Copyright (C) 2011  <copyright holder> <email>
 * 
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 * 
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 * 
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <list>
#include <isis/core/image.hpp>
#include <isis/core/bytearray.hpp>
#include <isis/core/progressfeedback.hpp>


namespace isis::io::_internal{

class WriterSpec{
protected:
	bool m_isInt,m_isFloat;
	WriterSpec(std::string repn,std::string name, uint8_t prio, bool isInt, bool isFloat, uint8_t sizeFact, uint16_t storeTypeID);
	size_t m_storeTypeID;
public:
	uint8_t m_sizeFact,m_priority;
	std::string m_vistaRepnName,m_vistaImageName;
	bool isCompatible(const WriterSpec &other)const{
		return (m_isInt != other.m_isInt || m_isFloat != other.m_isFloat );
	}
	virtual uint16_t storeVImageImpl(std::list< isis::data::Chunk >& chunks, std::ofstream& out, data::scaling_pair scaling );
	virtual void modHeaderImpl(isis::util::PropertyMap& props, const isis::util::vector4< size_t > &size);
	virtual ~WriterSpec(){};
};
template<typename T> class typeSpecImpl:public WriterSpec{
public:
	typeSpecImpl(std::string repn,uint8_t prio):
		WriterSpec(repn,"image",prio,std::is_integral_v<T>,std::is_floating_point_v<T>,sizeof(T),util::typeID<T>()){}
};
template<> class typeSpecImpl<util::color24>:public WriterSpec{
public:
	typeSpecImpl(std::string repn,uint8_t prio):
		WriterSpec(repn,"3DVectorfield",prio,false,false,3,util::typeID<util::color24>()){}
	uint16_t storeVImageImpl(std::list< isis::data::Chunk >& chunks, std::ofstream& out, data::scaling_pair scaling );
	void modHeaderImpl(isis::util::PropertyMap& props, const isis::util::vector4< size_t > &size);
};

class VistaProtoImage: protected std::list<data::Chunk>{};

class VistaInputImage: public VistaProtoImage{
	typedef data::ValueArray ( *readerPtr )(data::ByteArray data, size_t offset, size_t size );
	readerPtr m_reader;
	data::ByteArray m_data;
	data::ByteArray::iterator m_data_start;
	
	unsigned short last_type;
	util::fvector3 last_voxelsize;
	util::istring last_repn;
	util::PropertyValue last_component;
	
	std::map<util::istring, readerPtr> vista2isis;
	bool big_endian;
	
	template<typename T> data::TypedArray<util::color<T> > toColor(const data::ValueArray ref, size_t slice_size ) {
		assert( ref.getLength() % 3 == 0 );

		//colors are stored slice-wise in the 3d block
		std::vector<data::TypedArray<T>> layers = data::TypedArray<T>(ref).typed_splice( slice_size ); ;
		data::TypedArray<util::color<T>> ret( ref.getLength() / 3 );
		
		typename data::TypedArray<util::color<T> >::iterator d_pix = ret.begin();
		
		for( auto l = layers.begin(); l != layers.end(); ) {
			for( T s_pix: ( *l++ ) )
				( *( d_pix++ ) ).r = data::endianSwap( s_pix ); //red
			d_pix -= slice_size; //return to the slice start
			for( T s_pix: ( *l++ ) )
				( *( d_pix++ ) ).g = data::endianSwap( s_pix ); //green
			d_pix -= slice_size; //return to the slice start
			for( T s_pix: ( *l++ ) )
				( *( d_pix++ ) ).b = data::endianSwap( s_pix ); //blue
		}
		
		big_endian = false;
		return ret;
	}
	
public:
	VistaInputImage( data::ByteArray data, data::TypedArray< uint8_t >::iterator data_start );
	/// add a chunk to the protoimage
	bool add( util::PropertyMap props );
	bool isFunctional()const;
	void transformFromFunctional( );
	void fakeAcqNum();
	
	/// store the protoimage's' chunks into the output list, do byteswap if necessary
	void store( std::list< data::Chunk >& out, const util::PropertyMap& root_map, uint16_t sequence, const std::shared_ptr< util::ProgressFeedback > &feedback );
};
class VistaOutputImage:public VistaProtoImage{
	template<typename T> void insertSpec(std::map<size_t ,std::shared_ptr<WriterSpec> > &map,std::string name,uint8_t prio){
		map[util::typeID<T>()].reset(new typeSpecImpl<T>(name,prio));
	}
	size_t chunksPerVistaImage;
	util::PropertyMap imageProps;
	void writeMetadata(std::ofstream& out, const isis::util::PropertyMap& data, const std::string& title, size_t indent=0);
	std::map<size_t,std::shared_ptr<WriterSpec> > isis2vista;
	size_t storeTypeID;
	data::scaling_pair scaling;
	template<typename FIRST,typename SECOND> static void typeFallback(unsigned short &typeID){
		LOG(Runtime,notice) 
			<< util::MSubject(util::typeName<FIRST>()) << " is not supported in vista falling back to "
			<< util::MSubject(util::typeName<SECOND>());
		typeID=util::typeID<SECOND>();
	}
public:
	VistaOutputImage(data::Image src);
	void storeVImages(std::ofstream &out);
	void extractHistory(util::slist &ref);
	void storeHeaders(std::ofstream& out, size_t& offset);
	void storeHeader( const isis::util::PropertyMap& ch, const isis::util::vector4< size_t > size, size_t data_offset, std::ofstream& out );
};

	
}


