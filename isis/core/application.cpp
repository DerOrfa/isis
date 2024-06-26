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

#include <clocale>
#include <memory>
#include "application.hpp"
#include "fileptr.hpp"
#include "console_progress_bar.hpp"

#define STR(s) _xstr_(s)
#define _xstr_(s) std::string(#s)

namespace isis::util
{

Application::Application( std::string_view name, std::string_view cfg): m_name( name )
{
	addLogging<CoreLog>("Core");
	addLogging<CoreDebug>("Core");
	addLogging<DataLog>("Data");
	addLogging<DataDebug>("Data");
	addLogging<ImageIoLog>("ImageIO");
	addLogging<ImageIoDebug>("ImageIO");
	addLogging<MathLog>("Math");
	addLogging<MathDebug>("Math");

	parameters["help"] = false;
	parameters["help"].setDescription( "Print help" );
	parameters["help"].setNeeded(false);

	parameters["locale"] = std::string("C");
	parameters["locale"].setDescription( "locale to use for parsing/printing (use empty string to enforce use of system locale)");
	parameters["locale"].setNeeded(false);

	if(cfg.length()){
		parameters["cfg"]=std::string(cfg);
		parameters["cfg"].setDescription("File to read configuration from");
		parameters["cfg"].setNeeded(false);
	}

	parameters["np"] = false;
	parameters["np"].setNeeded(false);
	parameters["np"].setDescription( "suppress progress bar" );
	parameters["np"].hidden() = true;
}

void Application::addLoggingParameter( const std::string& name )
{
	static const Selection dbg_levels(
		std::map<LogLevel,std::string>{
			{error,"error"},
			{warning,"warning"},
			{notice,"notice"},
			{info,"info"},
			{verbose_info,"verbose_info"}
		},
		notice
	);

	if( parameters.find( "d"s + name ) == parameters.end() ) { //only add the parameter if it does not exist yet
		parameters["d"s + name] = dbg_levels;

		if( name.empty() )
			parameters["d"s + name].setDescription( "Log level for \"" + m_name + "\" itself" );
		else
			parameters["d"s + name].setDescription( "Log level for the \"" + name + "\" module(s)" );

		parameters["d"s + name].hidden() = true;
		parameters["d"s + name].setNeeded(false);
	}
}
void Application::removeLogging( const std::string& name )
{
	parameters.erase( "d"s + name );
	logs.erase( name );
}

bool Application::addConfigFile(const std::string& filename)
{
	data::FilePtr f(filename);
	if(f.good()){
		const data::ValueArray buffer=f.at<uint8_t>(0);
		if(configuration.readJson(buffer.beginTyped<uint8_t>(),buffer.endTyped<uint8_t>(),'/')==0){
			auto param=configuration.queryBranch("parameters");
			// if there is a "parameters" section in the file, use that as default parameters for the app
			if(param){
				for(PropertyMap::PropPath p:param->getLocal<PropertyValue>()){
					assert(p.size()==1);
					ProgParameter &dst=parameters[p.front().c_str()];
					PropertyValue &src=param->touchProperty(p);
					if(!dst.isParsed()){ // don't touch it, if It's not just the default
						LOG(Runtime,verbose_info) << "Setting parameter " << std::make_pair(p,src) << " from configuration";
						if(dst.isEmpty())
							dst.swap(src);
						else if(!dst.front().apply(src.front())){ //replace builtin default with value from config if there is one already
							LOG(Runtime,warning) << "Failed to apply parameter " << std::make_pair(p,src) << " from configuration, skipping ..";
							continue;
						}
					}
					dst.setNeeded(false); //param has got its default from the configuration, so its not needed anymore
					param->remove(p);
				}
			}
			return true;
		} else {
			LOG(Runtime,warning) << "Failed to parse configuration file " << util::MSubject(filename);
		}
	}
	return false;
}
const PropertyMap& Application::config() const{return configuration;}


void Application::addExample ( const std::string& params, const std::string& desc )
{
	m_examples.emplace_back( params, desc );
}

bool Application::initBase( int argc, char **argv )
{
	m_filename=argv[0];
	if ( !parameters.parse( argc, argv ) ){
		LOG( Runtime, error ) << "Failed to parse the command line";
		return false;
	}

	if ( parameters["help"] ) {
		printHelp( true );
		exit( 0 );
	}

	auto cfg=parameters.find("cfg");
	if(cfg!=parameters.end()){
		if(!addConfigFile(cfg->second.as<std::string>()))
			return false;
	}

	resetLogging();

	if(!parameters["np"])
		feedback() = std::make_shared<util::ConsoleProgressBar>();


	const std::string loc=parameters["locale"];
	if(!loc.empty())
		std::setlocale(LC_ALL,loc.c_str());

	return true;
}

bool Application::init( int argc, char **argv, bool exitOnError )
{
	bool good = initBase(argc,argv);

	if ( ! parameters.isComplete() ) {
		std::cerr << "Missing parameters: ";

		for ( const auto &P: parameters ) {
			if ( P.second.isNeeded() ) {std::cerr << "-" << P.first << "  ";}
		}

		std::cerr << std::endl;
		printHelp();
		good = false;
	}

	if ( !good && exitOnError ) { //we did printHelp already above
		std::cerr << "Exiting..." << std::endl;
		exit( 1 );
	}

	return good;
}
std::list<std::shared_ptr<MessageHandlerBase>> Application::resetLogging()
{
	std::list<std::shared_ptr<MessageHandlerBase>> ret;
	for( auto &ref: logs ) {
		const std::string dname = "d"s + ref.first;
		assert( !parameters[dname].isEmpty() ); // this must have been set by addLoggingParameter (called via addLogging)
		auto level = ( LogLevel )static_cast<uint8_t>(parameters[dname].as<Selection>());
		for( setLogFunction setter: ref.second ) {
			ret.push_back(std::move((this->*setter )( level )));
		}
	}
	return std::move(ret);
}

void Application::printHelp( bool withHidden )const
{
	typedef std::list<std::pair<std::string, std::string> >::const_reference example_type;
	std::cerr << this->m_name << " (using isis " << getCoreVersion() << ")" << std::endl;
	std::cerr << "Usage: " << this->m_filename << " <options>" << std::endl << "Where <options> includes:" << std::endl;;

	for (const auto & parameter : parameters) {
		std::string pref;

		if ( parameter.second.isNeeded() ) {
			pref = ". Required.";
		} else if( parameter.second.isHidden() ) {
			if( !withHidden )
				continue; // if its hidden, not needed, and wie want the short version skip this parameter
		}

		if ( ! parameter.second.isNeeded() ) {
			pref = ". Default: \"" + parameter.second.toString() + "\".";
		}

		std::cerr
		        << "\t-" << parameter.first << " <" << parameter.second.getTypeName() << ">" << std::endl
		        << "\t\t" << parameter.second.description() << pref << std::endl;

		if ( parameter.second.is<Selection>() ) {
			const Selection &ref = parameter.second.as<Selection>();
			const std::list< istring > entries = ref.getEntries();
			auto i = entries.begin();
			std::cerr << "\t\tOptions are: \"" << *i << "\"";

			for( i++ ; i != entries.end(); i++ ) {
				auto dummy = i;
				std::cout << ( ( ++dummy ) != entries.end() ? ", " : " or " ) << "\"" << *i << "\"";
			}

			std::cerr << "." << std::endl;
		}
	}

	if( !m_examples.empty() ) {
		std::cout << "Examples:" << std::endl;

		for( example_type ex :  m_examples ) {
			std::cout << '\t' << m_filename << " " << ex.first << '\t' << ex.second << std::endl;
		}
	}
}

std::shared_ptr<MessageHandlerBase> Application::makeLogHandler(isis::LogLevel level) const
{
	return std::shared_ptr< MessageHandlerBase >( level ? new util::DefaultMsgPrint( level ) : 0 );
}
std::string Application::getCoreVersion( void )
{
#ifdef ISIS_RCS_REVISION
	return STR( _ISIS_VERSION_MAJOR ) + "." + STR( _ISIS_VERSION_MINOR ) + "." + STR( _ISIS_VERSION_PATCH ) + " [" + STR( ISIS_RCS_REVISION ) + " " + __DATE__ + "]";
#else
	return STR( _ISIS_VERSION_MAJOR ) + "." + STR( _ISIS_VERSION_MINOR ) + "." + STR( _ISIS_VERSION_PATCH ) + " [" + __DATE__ + "]";;
#endif
}

std::shared_ptr<util::ProgressFeedback>& Application::feedback(){
	static std::shared_ptr<util::ProgressFeedback> fbk;
	return fbk;
}
}

#undef STR
#undef _xstr_

