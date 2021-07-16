//
// Created by enrico on 11.07.21.
//

#include "utils.hpp"
#include <type_traits>
#include "../../core/io_factory.hpp"

namespace isis::python{
namespace _internal{
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
	template<typename T> std::enable_if_t<!std::is_arithmetic_v<T>, py::buffer_info>
	make_buffer_impl(const std::shared_ptr<T> &ptr,const data::NDimensional<4> &shape){
		std::cerr << "Sorry nothing but scalar pixel types supported (for now)" << std::endl;
		return {};
	}
}

py::buffer_info make_buffer(const data::Chunk &ch)
{
	return ch.visit(
		[&ch](auto ptr)->py::buffer_info{return _internal::make_buffer_impl(ptr,ch);}
	);
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

}

