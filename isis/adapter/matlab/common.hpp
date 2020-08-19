//
// Created by Enrico Reimer on 16.08.20.
//

#pragma once

#include "isis/core/propmap.hpp"

#include "MatlabDataArray.hpp"
#include "cppmex/mexMatlabEngine.hpp"

namespace isis{
struct MatlabLog   {static constexpr char name[]="IsisMatlab";      static constexpr bool use = _ENABLE_LOG;};
struct MatlabDebug {static constexpr char name[]="IsisMatlabDebug"; static constexpr bool use = _ENABLE_DEBUG;};

namespace mat{

namespace _internal{
template<typename T> T getType(const matlab::data::Array arg,  const std::string& num_word)
{
	static matlab::data::ArrayFactory f;
	static auto reference = f.createScalar<T>({1,1});

	if (arg.getType() != reference.getType()) {
		LOG(MatlabLog, error)
			<< "The " << util::MSubject(num_word) << " parameter does not have the expected type ("
			<< (int) arg.getType() << "!=" << (int)reference.getType() << ")";
	} else {
		return matlab::data::TypedArray<T>(arg)[0];
	}
}
template<> std::string getType<std::string>(const matlab::data::Array arg,  const std::string& num_word);
}

class MatMessagePrint : public util::MessageHandlerBase
{
public:
	static std::shared_ptr<matlab::engine::MATLABEngine> enginePtr;
	explicit MatMessagePrint(isis::LogLevel level);
	void commit(const isis::util::Message &msg) override;
};

template<typename T, typename ARG_TYPE> T getArgument(ARG_TYPE args, size_t at){
	const auto num_word = at==0 ? "first":at==1?"second":at==2?"third":std::to_string(at)+"th";
	if(args.size()<=at){
		LOG(MatlabLog,error) << "There is no " << util::MSubject(num_word) <<  " parameter";
		return T();
	} else
		return _internal::getType<T>(args[at],num_word);
}

}
}



