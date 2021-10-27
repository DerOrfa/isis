//
// Created by enrico on 11.07.21.
//

#include "utils.hpp"
#include "pybind11/stl.h"
#include "pybind11/chrono.h"
#include <type_traits>
#include "../../core/io_factory.hpp"
#include "logging.hpp"

namespace isis::python{
namespace _internal{
typedef std::variant< //a reduced adapter to prevent ambiguous conversions (e.g. signed/unsigned)
		  bool, int, double
		, util::dvector3, util::ivector3
		, util::dvector4, util::ivector4
		, std::string
		, std::complex<double>
		, util::timestamp, util::duration
	> reduced_adapter_wo_lists;
typedef std::variant< //a reduced adapter to prevent ambiguous conversions (e.g. signed/unsigned)
bool, int, double
	, util::dvector3, util::ivector3
	, util::dvector4, util::ivector4
	, std::string
	, util::slist, util::ilist, util::dlist
	, std::complex<double>
	, util::timestamp, util::duration
	> reduced_adapter_w_lists;

	py::capsule make_capsule(const std::shared_ptr<void> &ptr)
	{
		return py::capsule(new std::shared_ptr<void>(ptr), [](void *f)
		{
			LOG(Debug,info) << "Freeing shared_ptr capsule at " << f;
			delete reinterpret_cast<std::shared_ptr<void> *>(f);
		});
	}

	//generic typecast plus some special cases
	template<typename T> py::object type2object(const T &v){
		return py::cast(v);
	}
	template<> py::object type2object<util::Selection>(const util::Selection &v){
		return py::str(static_cast<std::string>(v));
	}
	template<> py::object type2object<util::date>(const util::date &v){
		//pybind has no special conversion for date
		// Lazy initialise the PyDateTime import
		if (!PyDateTimeAPI) { PyDateTime_IMPORT; }

		time_t tme(std::chrono::duration_cast<std::chrono::seconds>(v.time_since_epoch()).count());
		std::tm localtime = *std::localtime(&tme);

		py::handle handle(PyDate_FromDate(
			localtime.tm_year + 1900,
			localtime.tm_mon + 1,
			localtime.tm_mday));
		return py::reinterpret_steal<py::object>(handle);
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
			const_cast<uint8_t*>(&ptr->r),//It's ok to drop the const here, we mark the buffer as readonly
			shape_v, strides_v,
			true);
	}
	py::buffer_info make_buffer_impl(const std::shared_ptr<util::color48> &ptr,const data::NDimensional<4> &shape){
		auto [shape_v, strides_v] = make_shape(shape,sizeof(util::color48));
		shape_v.push_back(3);
		strides_v.push_back(sizeof(uint16_t));
		return py::buffer_info(
			const_cast<uint16_t*>(&ptr->r),//It's ok to drop the const here, we mark the buffer as readonly
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
	//@todo maybe create with py::array_t<T,py::array::c_style> so we could skip the reshape
	auto info = ch.visit([&ch](auto ptr){return _internal::make_buffer_impl(ptr,ch);});
	auto array = py::array(info,_internal::make_capsule(ch.getRawAddress()));
	return array;
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
bool write(std::list<data::Image> images, std::string path, util::slist sFormatstack,util::slist sDialects, py::object repn){
	if(!repn.is_none()){
		util::Selection sRepn(util::getTypeMap( true ));
		if(sRepn.set(repn.cast<std::string>().c_str())){
			for( auto &ref :  images ) {
				ref.convertToType( static_cast<unsigned short>(sRepn) );
			}
		} else
			return false;
	}
	return data::IOFactory::write(images,path,util::makeIStringList(sFormatstack),util::makeIStringList(sDialects));
}

py::object value2object(const util::ValueTypes &val)
{
	return std::visit([](const auto &value)->py::object{
		return _internal::type2object(value);
	},val);
}
py::object property2object(const util::PropertyValue &val)
{
	switch(val.size()){
		case 0:return py::none();
		case 1:return value2object(val.front());
		default:
			py::list ret;
			for(const auto &value:val)
				ret.append(value2object(value));
			return ret;
	}
}
util::Value object2value(pybind11::handle ob)
{
	return std::visit([](auto &&v)->util::Value{return v;},ob.cast<_internal::reduced_adapter_w_lists>());
}
util::PropertyValue object2property(pybind11::handle ob)
{
	//tell pybind to try to make it one of the supported value types (except lists)
	// @todo this is messy, maybe better expose PropertyValue to python directly
	try{
		return std::visit([](auto &&v)->util::Value{return v;},ob.cast<_internal::reduced_adapter_wo_lists>());
	} catch(py::cast_error&){} //ignore error

	//try again as list
	util::PropertyValue ret;
	for(auto v:ob)
		ret.push_back(object2value(v));
	return ret;
}


py::dict getMetaDataFromPropertyMap(const util::PropertyMap &ob) {
	py::dict ret;
	for(auto &set:ob.getFlatMap()){
		ret[set.first.toString().c_str()]=python::property2object(set.second);
	}
	LOG(Debug,verbose_info) << "Transferred " << ret.size() << " properties from a PropertyMap";
	return ret;
}
py::dict getMetaDataFromImage(const data::Image &img, bool merge_chunk_data) {
	py::dict ret= getMetaDataFromPropertyMap(img);
	if(merge_chunk_data){
		for(const auto &set:img.getChunkAt(0,false).getFlatMap()){
			// create an empty PropertyValue to collect all props
			util::PropertyValue p;
			for(auto &props:img.getChunksProperties(set.first)){ // for each PropertyValue "key" from each chunk
				if(props.isEmpty()){
					LOG(Runtime,warning) << "Not adding empty chunk property for " << set.first;
				} else {
					p.transfer(p.end(),props); //move all its contents into p
				}
			}
			ret[set.first.toString().c_str()]=python::property2object(p);//make p dictionary entry
			LOG(Debug,verbose_info) << "Added " << p.size() << " distinct properties from chunks for " << set.first;
		}
	}
	return ret;
}

data::Image makeImage(py::buffer b, py::dict metadata)
{
	static const TypeMap type_map;
	/* Request a buffer descriptor from Python */
	py::buffer_info buffer = b.request();
	if(buffer.shape.size()>4)
		throw std::runtime_error("Only up to 4 dimensions are allowed!");

	if(buffer.shape.size()>1){
		std::swap(buffer.shape[0],buffer.shape[1]);
		std::swap(buffer.strides[0],buffer.strides[1]);
	}

	if(auto found_type=type_map.find(buffer.format);found_type!=type_map.end()) //a supported type was found
	{

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
			if(py::isinstance<py::str>(prop.first)){
				LOG(Debug,verbose_info) << "Setting property " << prop.first;
				chk.insert({prop.first.cast<std::string>(),object2property(prop.second)});
			}
			else
				LOG(Runtime,error) << "Ignoring key " << prop.first << " as its not a string";
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

