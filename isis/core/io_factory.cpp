//
// C++ Implementation: io_factory
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2009
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "io_factory.hpp"
#ifdef WIN32
	#include <windows.h>
	#include <Winbase.h>
#else
	#include <dlfcn.h>
#endif

#include <filesystem>
#include <iostream>
#include <algorithm>
#include <utility>

#include "log.hpp"
#include "common.hpp"
#include "singletons.hpp"
#include "fileptr.hpp"


namespace isis::data
{

IOFactory::io_error::io_error(const char *what,FileFormatPtr format):std::runtime_error(what),p_format(std::move(format)){}
IOFactory::FileFormatPtr IOFactory::io_error::which()const{
	return p_format;
}

IOFactory::IOFactory()
{
	const char *env_path = getenv( "ISIS_PLUGIN_PATH" );
	const char *env_home = getenv( "HOME" );

	if( env_path ) {
		findPlugins( std::filesystem::path( env_path ).native() );
	}

	if( env_home ) {
		const std::filesystem::path home = std::filesystem::path( env_home ) / "isis" / "plugins";

		if( std::filesystem::exists( home ) ) {
			findPlugins( home.native() );
		} else {
			LOG( Runtime, info ) << home << " does not exist. Won't check for plugins there";
		}
	}

#ifdef WIN32
	TCHAR lpExeName[2048];
	DWORD lExeName = GetModuleFileName( NULL, lpExeName, 2048 );
	bool w32_path_ok = false;

	if( lExeName == 0 ) {
		LOG( Runtime, error ) << "Failed to get the process name " << util::MSubject( util::getLastSystemError() );
	} else if( lExeName < 2048 ) {
		lpExeName[lExeName] = '\0';
		std::filesystem::path prog_name( lpExeName );

		if( std::filesystem::exists( prog_name ) ) {
			w32_path_ok = true;
			LOG( Runtime, info ) << "Determined the path of the executable as " << util::MSubject( prog_name.remove_filename().directory_string() ) << " will search for plugins there..";
			findPlugins( prog_name.remove_filename().directory_string() );
		}
	} else
		LOG( Runtime, error ) << "Sorry, the path of the process is to long (must be less than 2048 characters) ";

	LOG_IF( !w32_path_ok, Runtime, warning ) << "Could not determine the path of the executable, won't search for plugins there..";
#else
	findPlugins( std::string( PLUGIN_PATH ) );
#endif
}

bool IOFactory::registerFileFormat( const FileFormatPtr& plugin, bool front ){
	return get().registerFileFormat_impl( plugin, front );
}

bool IOFactory::registerFileFormat_impl( const FileFormatPtr& plugin, bool front )
{
	if ( !plugin )return false;

	io_formats.push_back( plugin );
	std::list<util::istring> suffixes = plugin->getSuffixes(  );
	LOG( Runtime, info )
			<< "Registering io-plugin " << util::MSubject( plugin->getName() ) << " with supported suffixes " << suffixes;
	for( util::istring & it :  suffixes ) {
		if(front)
			io_suffix[it].push_front( plugin );
		else
			io_suffix[it].push_back( plugin );
	}
	return true;
}

unsigned int IOFactory::findPlugins( const std::string &path )
{
	std::filesystem::path p( path );

	if ( !exists( p ) ) {
		LOG( Runtime, warning ) << util::MSubject( p ) << " not found";
		return 0;
	}

	if ( !std::filesystem::is_directory( p ) ) {
		LOG( Runtime, warning ) << util::MSubject( p ) << " is no directory";
		return 0;
	}

	LOG( Runtime, info )   << "Scanning " << util::MSubject( p ) << " for plugins";
	static const std::string pluginFilterStr=std::string(DL_PREFIX)+"isisImageFormat_[\\w]+"+DL_SUFFIX;
	static const std::regex pluginFilter( pluginFilterStr, std::regex_constants::ECMAScript | std::regex_constants::icase);
	unsigned int ret = 0;

	for ( std::filesystem::directory_iterator itr( p ); itr != std::filesystem::directory_iterator(); ++itr ) {
		if ( std::filesystem::is_directory( *itr ) )continue;

		if ( std::regex_match( itr->path().filename().string(), pluginFilter ) ) {
			const std::string pluginName = itr->path().native();
#ifdef WIN32
			HINSTANCE handle = LoadLibrary( pluginName.c_str() );
#else
			void *handle = dlopen( pluginName.c_str(), RTLD_NOW );
#endif

			if ( handle ) {
#ifdef WIN32
				image_io::FileFormat* ( *factory_func )() = ( image_io::FileFormat * ( * )() )GetProcAddress( handle, "factory" );
#else
				image_io::FileFormat* ( *factory_func )() = ( image_io::FileFormat * ( * )() )dlsym( handle, "factory" );
#endif

				auto deleter = [handle, pluginName]( image_io::FileFormat *format ) {
					delete format;
#ifdef WIN32
					if( !FreeLibrary( ( HINSTANCE )handle ) )
						std::cerr << "Failed to release plugin " << pluginName << " (was loaded at " << handle << ")";
					// TODO we cannot use LOG here, because the loggers are gone allready
#else
					if ( dlclose( handle ) != 0 )
						std::cerr << "Failed to release plugin " << pluginName << " (was loaded at " << handle << ")";
					// TODO we cannot use LOG here, because the loggers are gone already
#endif
				};

				if ( factory_func ) {
					FileFormatPtr io_class( factory_func(), deleter );

					if ( registerFileFormat_impl( io_class ) ) {
						io_class->plugin_file = pluginName;
						ret++;
					} else {
						LOG( Runtime, warning ) << "failed to register plugin " << util::MSubject( pluginName );
					}
				} else {
#ifdef WIN32
					LOG( Runtime, warning )
							<< "could not get format factory function from " << util::MSubject( pluginName );
					FreeLibrary( handle );
#else
					LOG( Runtime, warning )
							<< "could not get format factory function from " << util::MSubject( pluginName ) << ":" << util::MSubject( dlerror() );
					dlclose( handle );
#endif
				}
			} else
#ifdef WIN32
				LOG( Runtime, warning ) << "Could not load library " << util::MSubject( pluginName );
#else
				LOG( Runtime, warning ) << "Could not load library " << util::MSubject( pluginName ) << ":" <<  util::MSubject( dlerror() );
#endif
		} else {
			LOG( Runtime, verbose_info ) << "Ignoring " << itr->path() << " because it doesn't match " << pluginFilterStr;
		}
	}

	return ret;
}

IOFactory &IOFactory::get()
{
	return util::Singletons::get<IOFactory, INT_MAX>();
}

std::list<Chunk> IOFactory::load_impl(const load_source &v, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback){
	bool overridden=true;
	const std::filesystem::path* filename = std::get_if<std::filesystem::path>( &v );
	if(formatstack.empty()){
		if(filename){
			overridden=false;
			formatstack=getFormatStack(filename->string());
		} else {
			LOG(Runtime,error) << "I got no format stack and no filename to deduce it from, won't load anything..";
			return {};
		}
	}
	FileFormatList readerList = getFileFormatList(formatstack);
	if ( readerList.empty() ) {
		if(filename && !std::filesystem::exists( *filename ) ) { // if it actually is a filename, but does not exist
			LOG( Runtime, error ) 
				<< util::MSubject( *filename )
				<< " does not exist as file, and no suitable plugin was found to generate data from "
				<< ( overridden ? 
						util::istring( "the requested format stack " ) + util::listToString<util::istring>(formatstack.begin(),formatstack.end()):
						util::istring( "that name" )
				);

		} else if( overridden ) {
			LOG( Runtime, error ) << "No plugin supporting the requested format stack " << formatstack << " was found";
		} else {
			LOG_IF(filename, Runtime, error ) << "No plugin found to read " << *filename;
			LOG_IF(std::holds_alternative<std::streambuf*>(v), Runtime, error ) << "No plugin found to load from stream";
			LOG_IF(std::holds_alternative<ByteArray>(v), Runtime, error ) << "No plugin found to load from memory";
		}
	} else {
		while( !readerList.empty() ) {
			FileFormatPtr format=readerList.front();
			readerList.pop_front();

			std::list<util::istring> use_dialects,format_dialects=format->dialects();
			format_dialects.sort();dialects.sort();
			std::set_intersection(dialects.begin(),dialects.end(),format_dialects.begin(),format_dialects.end(),std::back_inserter(use_dialects));

			const util::NoSubject with_dialect = use_dialects.empty() ?
				util::NoSubject( "" ) : 
				util::NoSubject(util::istring( " with dialects \"" ) + util::listToString<util::istring>(use_dialects.begin(),use_dialects.end()) + "\"");

			LOG_IF(filename, ImageIoDebug, info ) << "plugin to load file" << with_dialect << " " << *filename << ": " << format->getName();
			LOG_IF(!filename, ImageIoDebug, info ) << "plugin to load " << with_dialect << ": " << format->getName();

			try {
				std::list<data::Chunk> loaded =std::visit(
					[&](auto val) { return format->load( val, formatstack, use_dialects, feedback ); },
					v
				);
				if(filename){
					for( Chunk & ref :  loaded ) {
						ref.refValueAsOr( "source", filename->native() ); // set source to filename or leave it if its already set
					}
				}
				return loaded;
			} catch ( const std::runtime_error& e ) {
				if(readerList.empty()) // if it was the last reader drop through the error
					std::throw_with_nested(IOFactory::io_error(e.what(),format));
				else {
					LOG_IF(filename, Runtime, notice )
						<< format->getName()  << " failed to read " << *filename << with_dialect << " with " << e.what() << ", trying " << readerList.front();
				} 
			}
		}
	}
	return {};
}

std::list<util::istring> IOFactory::getFormatStack( const std::string& filename ){
	const std::filesystem::path fname( filename );
	auto ret = util::stringToList<std::string>( fname.filename().string(), '.' ); // get all suffixes (and the basename)
	if( !ret.empty() )ret.pop_front(); // remove the basename
	return util::makeIStringList(ret);
}

IOFactory::FileFormatList IOFactory::getFileFormatList( std::list<util::istring> format)
{
	FileFormatList ret;

	while( !format.empty() ) {
		const auto wholeName=util::listToString<util::istring>( format.begin(), format.end(), ".", "", "" ); // (re)construct the rest of the suffix
		const auto found = get().io_suffix.find( wholeName );

		if( found != get().io_suffix.end() ) {
			LOG( Debug, verbose_info ) << found->second.size() << " plugins support suffix " << wholeName;
			ret.insert( ret.end(), found->second.begin(), found->second.end() );
		}
		format.pop_front(); // remove one suffix, and try again
	}

	return ret;
}

std::list< Image > IOFactory::chunkListToImageList( std::list<Chunk> &src, util::slist* rejected )
{
	LOG_IF(src.empty(),Debug,warning) << "Calling chunkListToImageList with an empty chunklist";
	// throw away invalid chunks
	size_t errcnt=0;
	for(auto i=src.begin();i!=src.end();){
		if(!i->isValid()){
			LOG(image_io::Runtime, error ) << "Rejecting invalid chunk. Missing properties: " << i->getMissing();
			errcnt++;
			if(rejected)
				rejected->push_back(i->getValueAs<std::string>("source")); 
			src.erase(i++);//we must increment i before the removal, otherwise it will be invalid
		} else
			i++;
	}

	std::list< Image > ret;

	while ( !src.empty() ) {
		LOG( Debug, info ) << src.size() << " Chunks left to be distributed.";
		size_t before = src.size();

		std::unique_ptr<util::slist> buff_rejected(rejected? new util::slist:nullptr);
		Image buff( src, rowDim, buff_rejected.get() );

		if ( buff.isClean() ) {
			if( buff.isValid() ) { //if the image was successfully indexed and is valid, keep it
				ret.push_back( buff );
				if(rejected)
					rejected->splice(rejected->begin(),*buff_rejected);
				LOG( Runtime, info ) << "Image " << ret.size() << " with size " << util::MSubject( buff.getSizeAsString() ) << " done.";
			} else {
				LOG_IF( !buff.getMissing().empty(), Runtime, error )
						<< "Cannot insert image. Missing properties: " << buff.getMissing();
				errcnt += before - src.size();
			}
		} else
			LOG( Runtime, info ) << "Dropping non clean Image";
	}

	LOG_IF( errcnt, Runtime, warning ) << "Dropped " << errcnt << " chunks because they didn't form valid images";
	return ret;
}

std::list< Chunk > IOFactory::loadChunks( const load_source &v, std::list<util::istring> formatstack, std::list<util::istring> dialects)
{
	const std::filesystem::path* filename = std::get_if<std::filesystem::path>( &v );
	if(filename)
		assert(!std::filesystem::is_directory( *filename ));
	return get().load_impl( v, std::move(formatstack), std::move(dialects), get().m_feedback );
}


std::list< Image > IOFactory::load( const util::slist &paths, const std::list<util::istring>& formatstack, const std::list<util::istring>& dialects, isis::util::slist* rejected )
{
	std::list<Chunk> chunks;
	for( const std::string & path :  paths ) {
		std::list<Chunk> loaded;
		if(std::filesystem::is_directory( path )){
			loaded=get().loadPath(path,formatstack,dialects,rejected);
		} else {
			try{
				loaded=get().load_impl( path , formatstack, dialects, get().m_feedback);
				if(loaded.empty() && rejected)
					rejected->push_back(path);
			} catch (io_error &e){
				LOG(Runtime,error) << "Failed to load " << path << ", the last failing plugin was " << e.which()->getName() << " with " << e.what();
			}
		}
		chunks.splice(chunks.end(),loaded);
	}
	std::list<data::Image> images = chunkListToImageList( chunks, rejected );
	LOG( Runtime, info ) << "Generated " << images.size() << " images out of " << paths;

	// store paths of red, but rejected chunks
	std::set<std::string> image_rej;
	for(const data::Chunk &ch :  chunks ){
		image_rej.insert(ch.getValueAs<std::string>("source"));
	}
	if(rejected)
		rejected->insert(rejected->end(),image_rej.begin(),image_rej.end());
	
	return images;
}

std::list<data::Image> IOFactory::load( const load_source &source, const std::list<util::istring>& formatstack, const std::list<util::istring>& dialects, isis::util::slist* rejected )
{
	const std::filesystem::path* filename = std::get_if<std::filesystem::path>( &source );
	if(filename)
		return load( util::slist{filename->native()}, formatstack, dialects );
	else {
		try{
			std::list<Chunk> loaded=get().load_impl( source , formatstack, dialects, get().m_feedback);
			std::list<data::Image> images = chunkListToImageList( loaded, rejected );
			LOG( Runtime, info ) << "Generated " << images.size() << " images";
			return images;
		} catch (io_error &e){
			LOG(Runtime,error) << "Failed to load, the last failing plugin was " << e.which()->getName() << " with " << e.what();
			return {};
		}
	}
}

std::list<Chunk> isis::data::IOFactory::loadPath(const std::filesystem::path& path, const std::list<util::istring>& formatstack, const std::list<util::istring>& dialects, util::slist* rejected)
{
	std::list<Chunk> ret;
	const size_t length = std::distance( std::filesystem::directory_iterator( path ), std::filesystem::directory_iterator() ); //@todo this will also count directories
	if( m_feedback ) {
		m_feedback->show( length, std::string( "Reading " ) + std::to_string(length) + " files from " + path.native() );
	}

	bool no_mapping=false;
	// if we can handle the opened plugins plus the additional files
	if(!data::FilePtr::checkLimit(io_formats.size() + length)){
		LOG(Runtime,warning) << "Can't increase the limit for open files to " << length << ", falling back to remapped mode";
		no_mapping=true;
	}

	for ( std::filesystem::directory_iterator i( path ); i != std::filesystem::directory_iterator(); ++i )  {
		if ( std::filesystem::is_directory( *i ) )continue;

		try {
			std::list<Chunk> loaded= load_impl( *i, formatstack, dialects, nullptr );//we already do progress feedback, don't let the plugins do it
			
			if(no_mapping)
				for(data::Chunk &c:loaded) // enforce copy, to get data into memory
					c= c.copyByID(c.getTypeID());

			if(rejected && loaded.empty()){
				rejected->push_back(std::filesystem::path(*i).native());
			}
			ret.splice(ret.end(),loaded);
		} catch(const io_error &e) {
			LOG( Runtime, notice )
				<< "Failed to load " <<  i->path() << " using " <<  e.which()->getName() << " ( " << e.what() << " )";
		}

		if( m_feedback )
			m_feedback->progress();
		
	}

	if( m_feedback )
		m_feedback->close();

	return ret;
}

bool IOFactory::write(const data::Image &image, const std::string &path, const std::list<util::istring> &formatstack, const std::list<
	util::istring> &dialects )
{
	return write( std::list< isis::data::Image >{image}, path, formatstack, dialects );
}


bool IOFactory::write( std::list< isis::data::Image > images, const std::string &path, std::list<util::istring> formatstack, const std::list<util::istring> &dialects )
{
	if(formatstack.empty())
		formatstack=getFormatStack(path);
	const FileFormatList formatWriter = get().getFileFormatList( formatstack );

	for( std::list<data::Image>::reference ref :  images ) {
		ref.checkMakeClean();
	}

	if( !formatWriter.empty() ) {
		for( FileFormatList::const_reference it :  formatWriter ) {
			LOG( Debug, info ) << "plugin to write to " <<  path << ": " << it->getName();

			try {
				it->write( images, path, dialects, get().m_feedback );
				LOG( Runtime, info ) << images.size() << " images written to " << path << " using " <<  it->getName();
				return true;
			} catch ( const std::exception &e ) {
				LOG( Runtime, warning )
						<< "Failed to write " <<  images.size()
						<< " images to " << path << " using " <<  it->getName() << " (" << e.what() << ")";
			}
		}
	} else {
		LOG( Runtime, error ) << "No plugin found to write to: " << path; //@todo error message missing
	}

	return false;
}
void IOFactory::setProgressFeedback( std::shared_ptr<util::ProgressFeedback> feedback )
{
	IOFactory &This = get();

	if( This.m_feedback )This.m_feedback->close();

	This.m_feedback = std::move(feedback);
}

IOFactory::FileFormatList IOFactory::getFormats()
{
	return get().io_formats;
}


} // namespaces data isis
