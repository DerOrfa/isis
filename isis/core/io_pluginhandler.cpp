//
// Created by Enrico Reimer on 25.07.22.
//

#include "io_pluginhandler.hpp"
#include "singletons.hpp"
#include <any>

namespace isis::io
{
IoPluginHandler::IoPluginHandler()
{
	const char *env_path = getenv( "ISIS_PLUGIN_PATH" );
	const char *env_home = getenv( "HOME" );

	auto filter_str = std::string(DL_PREFIX)+"ioprotocol_[\\w]+"+DL_SUFFIX;
	if( env_path ) {
		findPlugins( env_path, filter_str );
	}

	if( env_home ) {
		const std::filesystem::path home = std::filesystem::path( env_home ) / "isis" / "plugins";

		if( std::filesystem::exists( home ) ) {
			findPlugins( home, filter_str );
		} else {
			LOG( Runtime, info ) << home << " does not exist. Won't check for plugins there";
		}
	}

}
unsigned int IoPluginHandler::findPlugins(const std::filesystem::path &path, const std::string &filename_filter_str )
{
	const std::regex filename_filter(filename_filter_str);

	if (!exists(path)) {
		LOG(Runtime, warning) << util::MSubject(path) << " not found";
		return 0;
	}

	if (!std::filesystem::is_directory(path)) {
		LOG(Runtime, warning) << util::MSubject(path) << " is no directory";
		return 0;
	}

	LOG(Runtime, info) << "Scanning " << util::MSubject(path) << " for plugins";
	unsigned int ret = 0;

	for (std::filesystem::directory_iterator itr(path); itr != std::filesystem::directory_iterator(); ++itr) {
		if (std::filesystem::is_directory(*itr))continue;

		if (std::regex_match(itr->path().filename().string(), filename_filter)) {
			load_plugin(itr->path());
		} else {
			LOG(Runtime, verbose_info) << "Ignoring " << itr->path() << " because it doesn't match " << filename_filter_str;
		}
	}

	return ret;
}
IoPluginHandler &IoPluginHandler::get()
{
	return util::Singletons::get<IoPluginHandler, INT_MAX>();
}

bool IoPluginHandler::register_plugin(std::any *plg, HINSTANCE handle, const std::string &name, const std::filesystem::path &file)
{
	if(std::type_index(plg->type())==std::type_index(typeid(PrimaryIoProtocol*)))
	{
		get().primary_io.push_back({
			handle,name,file,
			std::unique_ptr<PrimaryIoProtocol>(any_cast<PrimaryIoProtocol*>(*plg))
		});
	}
	else if(std::type_index(plg->type())==std::type_index(typeid(SecondaryIoProtocol*)))
	{
		get().secondary_io.push_back({
			handle,name,file,
			std::unique_ptr<SecondaryIoProtocol>(any_cast<SecondaryIoProtocol*>(*plg))
		});
	}
	return false;
}
IoPluginHandler::~IoPluginHandler()
{
	while (!secondary_io.empty())
		unload_plugin(secondary_io, secondary_io.begin());
	while (!primary_io.empty())
		unload_plugin(primary_io, primary_io.begin());
}
void IoPluginHandler::load_plugin(const std::filesystem::path &filename)
{
	HINSTANCE handle =
#ifdef WIN32
		LoadLibrary( pluginName.c_str() );
#else
		dlopen(filename.c_str(), RTLD_NOW);
#endif

	if (handle) {
#ifdef WIN32
		auto factory_func = (void *(*)()) GetProcAddress( handle, "init" );
		auto name_func = (const char *(*)()) GetProcAddress( handle, "name" );
#else
		auto factory_func = (void *(*)()) dlsym(handle, "init");
		auto name_func = (const char *(*)()) dlsym(handle, "name");
#endif

		if (factory_func) {
			auto obj = reinterpret_cast<std::any *>(factory_func());
			const char* name = name_func();
			register_plugin(obj,handle,name,filename);
		} else {
#ifdef WIN32
			LOG( Runtime, warning ) << "Could not load library " << util::MSubject( pluginName );
#else
			LOG(Runtime, warning) << "Could not load library " << util::MSubject(filename) << ":" << util::MSubject(dlerror());
#endif
		}
	}
}
}
