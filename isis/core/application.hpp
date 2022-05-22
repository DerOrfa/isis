// -*- fill-column: 120; c-basic-offset: 4 -*-
/****************************************************************
 *
 * Copyright (C) ISIS Dev-Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Author: Enrico Reimer, <reimer@cbs.mpg.de>, 2010
 *
 *****************************************************************/

#pragma once

#include "progparameter.hpp"
#include "propmap.hpp"
#include "progressfeedback.hpp"

namespace isis::util
{

/**
 * The ISIS base application class.
 *
 * To speed up the creation of new command line tools, the ISIS core library contains a number of helper classes based
 * on Application. An ISIS application class provides a parameter parser that tokenizes the program argument list
 * according to given parameter names and types and saves the results in an internal parameter map.
 *
 * An application needs to be initialized explicitly with a call to init().
 *
 * Usage instructions and a short parameter list can be shown with the method printHelp().
 *
 * See also: isis::data::IOApplication, isis::util::ParameterMap and \link isis::LogLevel isis::LogLevel \endlink
 */
class Application
{
	const std::string m_name;
	std::filesystem::path m_filename;
	PropertyMap configuration;
	std::list<std::pair<std::string, std::string> > m_examples;
	void addLoggingParameter( const std::string& name );

protected:
	typedef std::shared_ptr<MessageHandlerBase>( Application::*setLogFunction )( LogLevel level )const;
	std::map<std::string, std::list<setLogFunction> > logs;
	virtual std::shared_ptr<MessageHandlerBase> makeLogHandler(isis::LogLevel level) const;
	/**
	 * Base initialisation parameter parsing
	 * - stores argv[0] as m_filename
	 * - parses parameters and returns false if that fails
	 * - calls printHelp() and exits when "help" is found in parameters
	 * - reads configuration file if "cfg"  is found in parameters
	 * - returns false if that fails
	 * - (re)sets logging according to parameters
	 * - sets util::ConsoleProgressBar as logging sink unless "np" is found in parameters
	 * - sets locale according to parameter "locale" if given ("C" is isis default)
	 * @return false if parameter parsing fails (from cfg or command line), true otherwise
	 */
	bool initBase( int argc, char **argv );

public:

	ParameterMap parameters;

	/**
	 * Default Constructor.
	 *
	 * Creates the application and its parameter map.
	 * No program parameter is parsed here. To do that use init()
	 *
	 * \param name name of the application.
	 * \param cfg the default path of the config file. If empty, no config file will be loaded
	 * \note set \code parameters["cfg"].needed()=true \endcode to prevent the program from running without given config file
	 */
	explicit Application( std::string_view name, std::string_view cfg="" );

	/**
	 * Add a logging module.
	 * This enables a logging module MODULE and adds related program parameters to control its logging level.
	 * This logging level then applies to LOG-commands using that specific module.
	 *
	 * The MODULE must be a struct like
	 * \code
	 * struct MyLogModule {
	 *     static constexpr char name[]="MyModuleLog";
	 *     static constexpr bool use = _ENABLE_LOG;
	 * };
	 * \endcode
	 * then \code addLogging<MyLogModule>("MyLog"); \endcode will install a parameter "-dMyLog" which will control all calls to
	 * \code LOG(MyLogModule,...) << ... \endcode if _ENABLE_LOG was set for the compilation. Otherwise this commands will not have an effect.
	 *
	 * Multiple MODULES can have the same parameter name, so
	 * \code
	 * struct MyLogModule   {static constexpr char name[]="MyModuleLog";static constexpr bool use = _ENABLE_LOG;};
	 * struct MyDebugModule {static constexpr char name[]="MyModuleDbg";static constexpr bool use = _ENABLE_LOG;};
	 *
	 * addLogging<MyLogModule>("MyLog");
	 * addLogging<MyDebugModule>("MyLog");
	 * \endcode will control \code LOG(MyLogModule,...) << ... \endcode and \code LOG(MyDebugModule,...) << ... \endcode through the parameter "-dMyLog".
	 *
	 * \note This does not set the logging handler. That is done by reimplementing makeLogHandler( std::string module, isis::LogLevel level )const.
	 */
	template<typename MODULE> void addLogging(const std::string& parameter_name) {
		addLoggingParameter( parameter_name );
		logs[parameter_name].push_back( &Application::setLog<MODULE> );
	}
	/**
	 * Removes a logging parameter from the application.
	 * \param name the parameter name without "-d"
	 * \note the logging module cannot be removed at runtime - its usage is controlled by the _ENABLE_LOG / _ENABLE_DEBUG defines at compile time.
	 */
	void removeLogging( const std::string& name );

	bool addConfigFile(const std::string &filename);
	[[nodiscard]] const PropertyMap &config()const;

	/**
	 * Add an example for the programs usage.
	 * \param parameters the parameter string for the example call
	 * \param desc the description of the example
	 */
	void addExample( const std::string& parameters, const std::string& desc );

	/**
	 * Initializes the program parameter map using argc/argv.
	 * For every entry in parameters an corresponding command line parameter is searched and parsed.
	 * A command line parameter corresponds to an entry of parameters if it string-equals caseless to this entry and is preceded by "-".
	 *
	 * \param argc amount of command line parameters in argv
	 * \param argv array of const char[] containing the command line parameters
	 * \param exitOnError if true the program will exit, if there is a problem during the initialisation (like missing parameters).
	 */
	virtual bool init( int argc, char **argv, bool exitOnError = true );

	/**
	 * (re)set log Handlers by calling setLog for each registered module.
	 * Useful if Application::makeLogHandler was reimplemented and its behavior changes during runtime.
	 */
	std::list<std::shared_ptr<MessageHandlerBase>> resetLogging();
	/**
	 * Virtual function to display a short help text.
	 * Ths usually shall print the program name plus all entries of parameters with their description.
	 */
	virtual void printHelp( bool withHidden = false )const;
	/// Set the logging level for the specified module
	template<typename MODULE>
	std::shared_ptr<MessageHandlerBase> setLog(LogLevel level ) const {
		LOG( Debug, info ) << "Setting logging for module " << MSubject( MODULE::name ) << " to level " << level;
		if ( !MODULE::use )return {};
		else {
			auto handle=makeLogHandler(level);
			_internal::Log<MODULE>::setHandler(handle);
			return handle;
		}

	}
	//get the version of the coreutils
	static std::string getCoreVersion( );
	
	static std::shared_ptr<util::ProgressFeedback>& feedback();
};
}
