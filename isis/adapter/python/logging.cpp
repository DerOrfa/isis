#include "logging.hpp"
#include "../../core/log.hpp"
#include "../../core/log_modules.hpp"

namespace isis::python
{
template<typename MODULE>
void enableProxyLoging(LogLevel level)
{
	typedef LoggerProxy<MODULE::name> this_proxy;
	if(!MODULE::use); else util::_internal::Log<MODULE>::template enable<this_proxy>(level);
}

void setLogging(const util::Selection &lvl_select,util::istring module){
	LogLevel lvl= (LogLevel)(int)lvl_select;
	if(module.substr(0,5)=="isis.")
		module.erase(0,5);
	if(module=="all"){ // set all modules
		for(auto &s:setters)
			s.second(lvl);
	} else { //find the requested module
		auto found=setters.find(module);
		if(found!=setters.end())
			found->second(lvl);
		else
			LOG(Runtime,error) << "logging module " << module << " not found";
	}
}
void setup_logging(LogLevel level)
{
	setters["core"] = [](LogLevel level)
	{
		enableProxyLoging<CoreLog>(level);
		enableProxyLoging<CoreDebug>(level);
	};
	setters["data"] = [](LogLevel level)
	{
		enableProxyLoging<DataLog>(level);
		enableProxyLoging<DataDebug>(level);
	};
	setters["imageio"] = [](LogLevel level)
	{
		enableProxyLoging<ImageIoLog>(level);
		enableProxyLoging<ImageIoDebug>(level);
	};
	setters["math"] = [](LogLevel level)
	{
		enableProxyLoging<MathLog>(level);
		enableProxyLoging<MathDebug>(level);
	};
	setters["python"] = [](LogLevel level)
	{
		enableProxyLoging<Runtime>(level);
		enableProxyLoging<Debug>(level);
	};

	setLogging(util::Selection({"error", "warning", "notice", "info", "verbose_info"}, "notice" ),"all");
}
void free_logging(LogLevel level)
{
	LOG(Runtime, verbose_info) << "isis logging will be sent through the default handler again";
	ENABLE_LOG(CoreLog, util::DefaultMsgPrint, level);
	ENABLE_LOG(CoreDebug, util::DefaultMsgPrint, level);
	ENABLE_LOG(DataLog, util::DefaultMsgPrint, level);
	ENABLE_LOG(DataDebug, util::DefaultMsgPrint, level);
	ENABLE_LOG(ImageIoLog, util::DefaultMsgPrint, level);
	ENABLE_LOG(ImageIoDebug, util::DefaultMsgPrint, level);
	ENABLE_LOG(MathLog, util::DefaultMsgPrint, level);
	ENABLE_LOG(MathDebug, util::DefaultMsgPrint, level);
	ENABLE_LOG(Runtime, util::DefaultMsgPrint, level);
	ENABLE_LOG(Debug, util::DefaultMsgPrint, level);
}
std::map<util::istring,std::function<void(LogLevel)>> setters;

}
