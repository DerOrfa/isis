//
// Created by Enrico Reimer on 25.07.22.
//

#pragma once
#include "io_protocol.hpp"
#include <filesystem>

#ifdef WIN32
#include <windows.h>
	#include <Winbase.h>
#else
#include <dlfcn.h>
#endif

namespace isis::io{
class IoPluginHandler{
public:
#ifndef WIN32
	typedef void* HINSTANCE;
#endif
	template<class T> struct loaded_plugin{
		HINSTANCE handle;
		std::string name;
		std::filesystem::path file;
		std::unique_ptr<T> object;
	};
private:
	std::list<loaded_plugin<PrimaryIoProtocol>> primary_io;
	std::list<loaded_plugin<SecondaryIoProtocol>> secondary_io;

	static IoPluginHandler &get();
	static unsigned int findPlugins( const std::filesystem::path &path, const std::string &filename_filter_str );
	static void load_plugin(const std::filesystem::path &filename);
	template<class T> static void unload_plugin(std::list<loaded_plugin<T>>& container, typename std::list<loaded_plugin<T>>::iterator at){
#ifdef WIN32
		if( !FreeLibrary( ( HINSTANCE )handle ) )
#else
		if (dlclose(at->handle) != 0)
#endif
		{LOG(Runtime,warning) << "Failed to release plugin " << at->name << " (was loaded at " << at->handle << ")";}
		container.erase(at);
	}
	static bool register_plugin(std::any *plg, HINSTANCE handle, const std::string &name, const std::filesystem::path &file);
public:
	IoPluginHandler();
	~IoPluginHandler();
};
}
