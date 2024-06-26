/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "io_application.hpp"
#include "io_factory.hpp"


namespace isis::data
{
IOApplication::IOApplication( std::string_view name, bool have_input, bool have_output, std::string_view cfg ):
	Application( name, cfg ), m_input( have_input )
{
	if ( have_input )
		addInput();

	if ( have_output )
		addOutput();

	parameters["help-io"] = false;
	parameters["help-io"].setNeeded(false);
	parameters["help-io"].setDescription( "List all loaded IO plugins and their supported formats, exit after that" );
}

IOApplication::~IOApplication()
{
	data::IOFactory::setProgressFeedback( std::shared_ptr<util::ProgressFeedback>() );
}

bool IOApplication::init( int argc, char **argv, bool exitOnError )
{
	bool ok = initBase( argc, argv);

	if(parameters["help-io"]){
		printIOHelp();
		exit(0);
	}

	if ( ! parameters.isComplete() ) {
		std::cerr << "Missing parameters: ";

		for ( const auto &P: parameters ) {
			if ( P.second.isNeeded() ) {std::cerr << "-" << P.first << "  ";}
		}

		std::cerr << std::endl;
		printHelp();
		ok = false;
	}

	if ( !ok  ){
		if(exitOnError){
			std::cerr << "Exiting..." << std::endl;
			exit(1);
		}
		else
			return false;
	}

	if ( m_input ) {
		images.splice(images.end(),autoload( exitOnError ));
	}

	return true;
}

void IOApplication::addInput ( util::ParameterMap &parameters, const std::string &desc, const std::string &suffix, bool needed)
{
	parameters[std::string( "in" ) + suffix] = util::slist();
	parameters[std::string( "in" ) + suffix].setDescription( std::string( "input file(s) or directory(s)" ) + desc );
	parameters[std::string( "in" ) + suffix].setNeeded(needed);

	parameters[std::string( "rf" ) + suffix] = util::slist();
	parameters[std::string( "rf" ) + suffix].setNeeded(false);
	parameters[std::string( "rf" ) + suffix].hidden() = true;

	parameters[std::string( "rf" ) + suffix].setDescription( std::string( "Override automatic detection of file suffix for reading" + desc + " with given value" ) );
	parameters[std::string( "rdialect" ) + suffix] = util::slist();
	parameters[std::string( "rdialect" ) + suffix].setNeeded(false);
	parameters[std::string( "rdialect" ) + suffix].setDescription(
		std::string( "choose dialect(s) for reading" ) + desc + ". The available dialects depend on the capabilities of the used IO plugin" );
}
void IOApplication::addInput(const std::string& desc, const std::string& suffix, bool needed)
{
	addInput(parameters,desc,suffix,needed);
}


void IOApplication::addOutput( util::ParameterMap &parameters, const std::string &desc, const std::string &suffix, bool needed)
{
	parameters[std::string( "out" ) + suffix] = std::string();
	parameters[std::string( "out" ) + suffix].setDescription( "output filename" + desc );
	parameters[std::string( "out" ) + suffix].setNeeded(needed);

	parameters[std::string( "wf" ) + suffix] = util::slist();
	parameters[std::string( "wf" ) + suffix].setNeeded(false);
	parameters[std::string( "wf" ) + suffix].setDescription( "Override automatic detection of file suffix for writing" + desc + " with given value" );
	parameters[std::string( "wf" ) + suffix].hidden() = true;

	parameters[std::string( "wdialect" ) + suffix] = util::slist();
	parameters[std::string( "wdialect" ) + suffix].setNeeded(false);
	parameters[std::string( "wdialect" ) + suffix].setDescription( "Choose dialect(s) for writing" + desc + ". Use \"--help-io\" for a list of the plugins and their supported dialects" );

	parameters[std::string( "repn" ) + suffix] = util::Selection( util::getTypeMap( true ) );
	parameters[std::string( "repn" ) + suffix].setNeeded(false);
	parameters[std::string( "repn" ) + suffix].setDescription(
		"Representation in which the data" + desc + " shall be written" );

	if( parameters.find( "np" ) == parameters.end() ) {
		parameters["np"] = false;
		parameters["np"].setNeeded(false);
		parameters["np"].setDescription( "suppress progress bar" );
		parameters["np"].hidden() = true;
	}
}
void IOApplication::addOutput(const std::string& desc, const std::string& suffix, bool needed)
{
	addOutput(parameters,desc,suffix,needed);
}


void IOApplication::printIOHelp() const
{
	std::cerr << std::endl << "Available IO Plugins:" << std::endl;
	data::IOFactory::FileFormatList plugins = data::IOFactory::getFormats();
	for( data::IOFactory::FileFormatList::const_reference pi :  plugins ) {
		std::cerr << std::endl << "\t" << pi->getName() << " (" << pi->plugin_file << ")" << std::endl;
		std::cerr << "\t=======================================" << std::endl;
		const std::list<util::istring> suff = pi->getSuffixes();
		const std::list<util::istring> dialects = pi->dialects();
		std::cerr << "\tsupported suffixes: " << util::listToString<util::istring>( suff.begin(), suff.end(), "\", \"", "\"", "\"" ).c_str()  << std::endl;

		if( !dialects.empty() )
			std::cerr << "\tsupported dialects: " << util::listToString( dialects.begin(), dialects.end(), "\", \"", "\"", "\"" )  << std::endl;
	}
}

std::list< Image > IOApplication::autoload( bool exitOnError,const std::string &suffix,util::slist* rejected)
{
	return autoload( parameters, exitOnError, suffix, feedback(),rejected );
}
std::list< Image > IOApplication::autoload ( const util::ParameterMap &parameters, bool exitOnError, const std::string &suffix,  std::shared_ptr<util::ProgressFeedback> feedback, util::slist* rejected)
{
	const util::slist input = parameters[std::string( "in" ) + suffix];
	const util::slist rf = parameters[std::string( "rf" ) + suffix];
	const util::slist dl = parameters[std::string( "rdialect" ) + suffix];
	
	const bool no_progress = parameters["np"];

	if( !no_progress && feedback ) {
		data::IOFactory::setProgressFeedback( feedback );
	}

	const std::list<util::istring> formatstack=util::makeIStringList(rf);

	std::list< Image > tImages;
	if(input.size()==1 && input.front()=="-"){
		LOG( Runtime, info )
			<< "loading from stdin" 
			<< util::NoSubject( rf.empty() ? "" : std::string( " using the format stack: " ) + util::listToString(rf.begin(),rf.end()) )
			<< util::NoSubject( ( !rf.empty() && !dl.empty() ) ? " and" : "" )
			<< util::NoSubject( dl.empty() ? "" : std::string( " using the dialect: " ) + util::listToString(dl.begin(),dl.end()) );
		tImages = data::IOFactory::load( std::cin.rdbuf(), formatstack, util::makeIStringList(dl),rejected );
	} else {
		LOG( Runtime, info )
			<< "loading " << util::MSubject( input )
			<< util::NoSubject( rf.empty() ? "" : std::string( " using the format stack: " ) + util::listToString(rf.begin(),rf.end()) )
			<< util::NoSubject( ( !rf.empty() && !dl.empty() ) ? " and" : "" )
			<< util::NoSubject( dl.empty() ? "" : std::string( " using the dialect: " ) + util::listToString(dl.begin(),dl.end()) );
		tImages = data::IOFactory::load( input, formatstack, util::makeIStringList(dl),rejected );
	}

	if ( tImages.empty() ) {
		if ( exitOnError ) {
			LOG( Runtime, notice ) << "No data acquired, exiting...";
			exit( 1 );
		} 
	} else {
		for( auto a = tImages.begin(); a != tImages.end(); a++ ) {
			for( auto b = a; ( ++b ) != tImages.end(); ) {
				const data::Image &aref = *a, bref = *b;
				LOG_IF( aref.getDifference( bref ).empty(), Runtime, warning ) << "The metadata of the images "
						<< aref.identify(true,false) << ":" << std::distance<std::list<Image> ::const_iterator>( tImages.begin(), a )
						<< " and " << bref.identify(true,false) << ":" << std::distance<std::list<Image> ::const_iterator>( tImages.begin(), b )
						<< " are equal. Maybe they are duplicates.";
			}
		}
	}

	return tImages;
}


bool IOApplication::autowrite( Image out_image, bool exitOnError ) {return autowrite( std::list<Image>( 1, out_image ), exitOnError );}
bool IOApplication::autowrite( std::list<Image> out_images, bool exitOnError )
{
	return autowrite( parameters, out_images, exitOnError, "");
}

bool IOApplication::autowrite ( const util::ParameterMap &parameters, Image out_image, bool exitOnError, const std::string &suffix )
{
	return autowrite( parameters, std::list<Image>( 1, out_image ), exitOnError, suffix );
}
bool IOApplication::autowrite ( const util::ParameterMap &parameters, std::list< Image > out_images, bool exitOnError, const std::string &suffix)
{
	std::optional<util::Selection> repn;
	if(parameters[std::string( "repn" ) + suffix].isParsed())
		repn = parameters[std::string( "repn" ) + suffix].as<util::Selection>();

	const std::string output = parameters[std::string( "out" ) + suffix];
	const util::slist wf = parameters[std::string( "wf" ) + suffix];
	const util::slist dl = parameters[std::string( "wdialect" ) + suffix];
	LOG( Runtime, info )
			<< "Writing " << out_images.size() << " images"
			<< ( repn ? std::string( " as " ) + static_cast<std::string>(*repn) : "" )
			<< " to " << util::MSubject( output )
			<< ( wf.empty() ? "" : std::string( " using the format: " ) + util::listToString(wf.begin(),wf.end()) )
			<< ( ( !wf.empty() && !dl.empty() ) ? " and" : "" )
			<< ( dl.empty() ? "" : std::string( " using the dialect: " ) + util::listToString(dl.begin(),dl.end()) );

	if( repn ) {
		for( std::list<Image>::reference ref :  out_images ) {
			ref.convertToType( static_cast<unsigned short>(*repn) );
		}
	}

	if( feedback() )
		data::IOFactory::setProgressFeedback( feedback() );

	if ( ! IOFactory::write( out_images, output, util::makeIStringList(wf), util::makeIStringList(dl) ) ) {
		if ( exitOnError ) {
			LOG( Runtime, notice ) << "Failed to write, exiting...";
			exit( 1 );
		}
		return false;
	} else
		return true;
}

Image IOApplication::fetchImage()
{
	assert( !images.empty() );
	Image ret = images.front();
	images.pop_front();
	return ret;
}

}

