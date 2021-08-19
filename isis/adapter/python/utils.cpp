//
// Created by enrico on 11.07.21.
//

#include "utils.hpp"
#include "pybind11/stl.h"
#include <type_traits>
#include "../../core/io_factory.hpp"
#include "logging.hpp"

namespace isis::python{
namespace _internal{
	py::capsule make_capsule(const std::shared_ptr<void> &ptr)
	{
		return py::capsule(new std::shared_ptr<void>(ptr), [](void *f)
		{
			LOG(Debug,info) << "Freeing shared_ptr capsule at " << f;
			delete reinterpret_cast<std::shared_ptr<void> *>(f);
		});
	}

	std::pair<std::vector<size_t>,std::vector<size_t>>
	make_shape(const data::NDimensional<4> &shape, size_t elem_size){
		std::vector<size_t> shape_v,strides_v;
		for(size_t i=0;i<shape.getRelevantDims();i++){
			shape_v.push_back(shape.getDimSize(i));
			strides_v.push_back(i?
								shape_v[i-1]*strides_v[i-1]:
								elem_size
			);
		}
		//numpy defaults to row major
		if(strides_v.size()>1){
			std::swap(strides_v[0],strides_v[1]);
			std::swap(shape_v[0],shape_v[1]);
		}
		return {shape_v,strides_v};
	}


	template<typename T> std::enable_if_t<std::is_arithmetic<T>::value, py::buffer_info>
	make_buffer_impl(const std::shared_ptr<T> &ptr,const data::NDimensional<4> &shape){
		auto [shape_v, strides_v] = make_shape(shape,sizeof(T));
		return py::buffer_info(
			const_cast<T*>(ptr.get()),//its ok to drop the const here, we mark the buffer as readonly
			shape_v,strides_v,
			true);
	}
	py::buffer_info make_buffer_impl(const std::shared_ptr<util::color24> &ptr,const data::NDimensional<4> &shape){
		auto [shape_v, strides_v] = make_shape(shape,sizeof(util::color24));
		shape_v.push_back(3);
		strides_v.push_back(sizeof(uint8_t));
		return py::buffer_info(
			const_cast<uint8_t*>(&ptr->r),//its ok to drop the const here, we mark the buffer as readonly
			shape_v, strides_v,
			true);
	}
	py::buffer_info make_buffer_impl(const std::shared_ptr<util::color48> &ptr,const data::NDimensional<4> &shape){
		auto [shape_v, strides_v] = make_shape(shape,sizeof(util::color48));
		shape_v.push_back(3);
		strides_v.push_back(sizeof(uint16_t));
		return py::buffer_info(
			const_cast<uint16_t*>(&ptr->r),//its ok to drop the const here, we mark the buffer as readonly
			shape_v, strides_v,
			true);
	}
	template<typename T> std::enable_if_t<!std::is_arithmetic_v<T>, py::buffer_info>
	make_buffer_impl(const std::shared_ptr<T> &ptr,const data::NDimensional<4> &shape){
		LOG(Runtime,error) << "Sorry nothing but scalar pixel types or color supported (for now)";
		return {};
	}
}

py::array make_array(data::Chunk &ch)
{
	return py::array(ch.visit(
		[&ch](auto ptr)->py::buffer_info{return _internal::make_buffer_impl(ptr,ch);}
	),_internal::make_capsule(ch.getRawAddress()));
}

py::array make_array(data::Image &img)
{
	if(img.getRelevantDims() == img.getChunkAt(0,false).getRelevantDims()){ // only one chunk, no merging needed
		LOG(Runtime,info) << "making cheap copy of single chunk image";
		auto chk=img.getChunkAt(0,false);
		return make_array(chk);
	} else {
		//we have to merge
		LOG(Debug,info) << "merging " << img.copyChunksToVector(false).size() << " chunks into one image";
		data::ValueArray whole_image=img.copyAsValueArray();
		LOG(Runtime,info) << "created " << whole_image.bytesPerElem()*whole_image.getLength()/1024/1024 << "MB buffer from multi chunk image";

		return py::array(whole_image.visit(
			[&img](auto ptr)->py::buffer_info{return _internal::make_buffer_impl(ptr,img);}
		),_internal::make_capsule(whole_image.getRawAddress()));
	}
}


std::pair<std::list<data::Image>, util::slist> load_list(util::slist paths,util::slist formatstack,util::slist dialects)
{
	std::list<util::istring> _formatstack,_dialects;
	util::slist rejects;
	auto conversion= [](const std::string_view &s)->util::istring {return {s.data(),s.length()};};
	std::transform(formatstack.begin(),formatstack.end(),std::back_inserter(_formatstack),conversion);
	std::transform(dialects.begin(),dialects.end(),std::back_inserter(_dialects),conversion);

	std::list<data::Image> loaded=data::IOFactory::load(paths,_formatstack,_dialects,&rejects);
	return {loaded,rejects};
}
std::pair<std::list<data::Image>, util::slist> load(std::string path,util::slist formatstack,util::slist dialects){
	return load_list({path},formatstack,dialects);
}

std::variant<py::none, util::ValueTypes, std::list<util::ValueTypes>> property2python(util::PropertyValue p)
{
	if(p.is<util::Selection>())
		p.transform<std::string>();
	switch(p.size()){
		case 0:return {};
		case 1:return p.front();
		default:return std::list<util::ValueTypes>(p.begin(),p.end());
	}
}
data::Image makeImage(py::buffer b, std::map<std::string, util::ValueTypes> metadata)
{
	static const TypeMap type_map;
	/* Request a buffer descriptor from Python */
	py::buffer_info buffer = b.request();
	if(auto found_type=type_map.find(buffer.format);found_type!=type_map.end()) //a supported type was found
	{
		if(buffer.shape.size()>4)
			throw std::runtime_error("Only up to 4 dimensions are allowed!");

		if(buffer.shape.size()>1){
			std::swap(buffer.shape[0],buffer.shape[1]);
			std::swap(buffer.strides[0],buffer.strides[1]);
		}


		util::vector4<size_t> sizes;
		std::copy(buffer.shape.begin(),buffer.shape.end(),sizes.begin());
		std::fill(sizes.begin()+buffer.shape.size(),sizes.end(),1);


		//@todo find a way to use the pyobject directly instead of making a copy
		auto array=data::ValueArray::createByID(found_type->second,buffer.size);
		LOG(Debug,info) << "Making an " << sizes << " image of type " << array.typeName();
		LOG(Debug,info) << "strides are " << util::listToString(buffer.strides.begin(),buffer.strides.end());
		assert(buffer.itemsize==array.bytesPerElem());
		memcpy(array.getRawAddress().get(),buffer.ptr,array.bytesPerElem()*array.getLength());

		auto chk=data::Chunk(array,sizes[0],sizes[1],sizes[2],sizes[3]);

		for(auto &prop:metadata){
//			LOG(Debug,verbose_info) << "Setting property " << prop.first << " as " << prop.second;
			chk.touchProperty(prop.first.c_str())=prop.second;
		}

		auto missing=chk.getMissing();
		if(missing.empty())
			return data::Image(chk);
		else{
			LOG(Runtime,error) << "Cannot create image, following properties are missing: " << missing;
			throw std::runtime_error("Invalid metadata!");
		}

	} else
		throw std::runtime_error("Incompatible data type!");

}
}

