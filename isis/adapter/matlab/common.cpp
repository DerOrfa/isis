//
// Created by Enrico Reimer on 16.08.20.
//

#include "common.hpp"

#include <utility>
#include "MatlabDataArray.hpp"

void isis::mat::MatMessagePrint::commit(const isis::util::Message &msg)
{
	static matlab::data::ArrayFactory f;

	std::stringstream formatted;
	formatted
		<< "[" << msg.strTime("%T") << "]" << msg.m_module << ":" << util::logLevelName( msg.m_level )
		<< " " << msg.merge("") << "(" << msg.m_file.filename().native() << ":" << msg.m_line << ")"
		<< std::endl;

		enginePtr->feval(u"fprintf",0,{f.createScalar(formatted.str())});
}
isis::mat::MatMessagePrint::MatMessagePrint(isis::LogLevel level) : MessageHandlerBase(level)
{}

template<> std::string isis::mat::_internal::getType<std::string>(const matlab::data::Array arg,  const std::string& num_word)
{
	static matlab::data::ArrayFactory f;

	if (arg.getType() == matlab::data::ArrayType::CHAR) {
		return matlab::data::CharArray(arg).toAscii();
	}
	else if(arg.getType() == matlab::data::ArrayType::MATLAB_STRING)
	{
		matlab::data::TypedArray<matlab::data::MATLABString> stringArray(arg);
		return std::string(stringArray[0]);
	} else {
		LOG(MatlabLog, error)
			<< "The " << util::MSubject(num_word) << " parameter is not a string";
	}
	return "";
}
template<> isis::util::slist isis::mat::_internal::getTypeList<std::string>(const matlab::data::Array arg,  const std::string& num_word){
	static matlab::data::ArrayFactory f;

	if (arg.getType() == matlab::data::ArrayType::CHAR) {
		return {matlab::data::CharArray(arg).toAscii()};
	}
	else if(arg.getType() == matlab::data::ArrayType::MATLAB_STRING)
	{
		matlab::data::TypedArray<matlab::data::MATLABString> stringArray(arg);
		return util::slist(stringArray.begin(),stringArray.end());
	} else {
		LOG(MatlabLog, error)
			<< "The " << util::MSubject(num_word) << " parameter is not a string";
	}
	return {};
}
template<> std::list<isis::util::istring> isis::mat::_internal::getTypeList<isis::util::istring>(const matlab::data::Array arg,  const std::string& num_word){
	auto source = getTypeList<std::string>(arg, num_word);
	std::list<isis::util::istring> ret(source.size());
	std::transform(source.begin(), source.end(), ret.begin(), [](const std::string &src)->util::istring {return src.c_str();});
	return ret;
}

std::shared_ptr<matlab::engine::MATLABEngine> isis::mat::MatMessagePrint::enginePtr= nullptr;
