//
// Created by enrico on 11.07.21.
//

#include "utils.hpp"
#include <type_traits>
#include "../../core/io_factory.hpp"
#include "../../math/common.hpp"

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


template<typename T> std::enable_if_t<std::is_arithmetic<T>::value, py::buffer_info>
	make_buffer_impl(const std::shared_ptr<T> &ptr,const data::NDimensional<4> &shape){
		std::vector<size_t> shape_v,strides_v;
		for(size_t i=0;i<shape.getRelevantDims();i++){
			shape_v.push_back(shape.getDimSize(i));
			strides_v.push_back(i?
				shape_v[i-1]*strides_v[i-1]:
				sizeof(T)
			);
		}
		//numpy defaults to row major
		if(strides_v.size()>1)std::swap(strides_v[0],strides_v[1]);
		return py::buffer_info(
			const_cast<T*>(ptr.get()),//its ok to drop the const here, we mark the buffer as readonly
			shape_v, strides_v,
			true);
	}
	py::buffer_info make_buffer_impl(const std::shared_ptr<util::color24> &ptr,const data::NDimensional<4> &shape){
		std::vector<size_t> shape_v,strides_v;
		shape_v.push_back(3);strides_v.push_back(1);
		for(size_t i=0;i<shape.getRelevantDims();i++){
			shape_v.push_back(shape.getDimSize(i));
			strides_v.push_back(shape_v[i]*strides_v[i]);
		}
		//numpy defaults to row major
		if(strides_v.size()>2)std::swap(strides_v[1],strides_v[2]);
		return py::buffer_info(
			const_cast<uint8_t*>(&ptr->r),//its ok to drop the const here, we mark the buffer as readonly
			shape_v, strides_v,
			true);
	}
	py::buffer_info make_buffer_impl(const std::shared_ptr<util::color48> &ptr,const data::NDimensional<4> &shape){
		std::vector<size_t> shape_v,strides_v;
		shape_v.push_back(3);strides_v.push_back(2);
		for(size_t i=0;i<shape.getRelevantDims();i++){
			shape_v.push_back(shape.getDimSize(i));
			strides_v.push_back(shape_v[i]*strides_v[i]);
		}
		//numpy defaults to row major
		if(strides_v.size()>2)std::swap(strides_v[1],strides_v[2]);
		return py::buffer_info(
			const_cast<uint16_t*>(&ptr->r),//its ok to drop the const here, we mark the buffer as readonly
			shape_v, strides_v,
			true);
	}
	template<typename T> std::enable_if_t<!std::is_arithmetic_v<T>, py::buffer_info>
	make_buffer_impl(const std::shared_ptr<T> &ptr,const data::NDimensional<4> &shape){
		LOG(Runtime,error) << "Sorry nothing but scalar pixel types supported (for now)";
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
	auto conversion= [](const std::string &s)->util::istring {return s.c_str();};
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
void setup_logging(LogLevel level){
	util::enableLog<python::LoggerProxy>(level);
	data::enableLog<python::LoggerProxy>(level);
	image_io::enableLog<python::LoggerProxy>(level);
	math::enableLog<python::LoggerProxy>(level);
	python::enableLog<python::LoggerProxy>(level);
	LOG(Runtime,verbose_info) << "isis logging will be sent through the python logging module";
}
void free_logging(LogLevel level){
	util::enableLog<util::DefaultMsgPrint>(level);
	data::enableLog<util::DefaultMsgPrint>(level);
	image_io::enableLog<util::DefaultMsgPrint>(level);
	math::enableLog<util::DefaultMsgPrint>(level);
	python::enableLog<util::DefaultMsgPrint>(level);
	LOG(Runtime,verbose_info) << "isis logging will be sent through the default handler";
}

LoggerProxy::LoggerProxy(LogLevel level) : util::MessageHandlerBase(level){
	auto logger= py::module_::import("logging");
	LoggerProxy::severity_map[error]=logger.attr("ERROR");
	LoggerProxy::severity_map[warning]=logger.attr("WARNING");
	LoggerProxy::severity_map[notice]=logger.attr("INFO");
	LoggerProxy::severity_map[info]=logger.attr("INFO");
	LoggerProxy::severity_map[verbose_info]=logger.attr("DEBUG");
	LoggerProxy::log=logger.attr("log");
}
void LoggerProxy::commit(const util::Message &msg)
{
	log(severity_map[msg.m_level],msg.merge(""));
}

}

