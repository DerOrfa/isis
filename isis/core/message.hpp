//
// C++ Interface: message
//
// Description:
//
//
// Author: Enrico Reimer<reimer@cbs.mpg.de>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//

#pragma once

#include <sstream>
#include <string>
#include <ctime>
#include <list>
#include <cstdio>

#include <chrono>
#include <filesystem>
#include <mutex>

namespace isis
{
enum LogLevel {error = 1, warning, notice, info, verbose_info};
namespace util
{

/**
 * Wrapper to mark the "Subject" in a logging message.
 * Use this to mark the a volatile part of a logging message.
 * eg. \code LOG(Debug,info) << "Loading File " << MSubject(filename); \endcode
 * This will then be ignored when looking for repeating log-messages or can be used for text highlighting.
 * \note anything which is no string literal will automatically used wrapped as Subject. Use NoSubject to prevent this.
 */
class MSubject : public std::string
{
public:
	template<typename T> MSubject( const T &cont ) {
		std::ostringstream text;
		if constexpr(std::is_pointer_v<T>){
			text << std::hex << cont;
		} else if constexpr(std::is_same_v<T,std::filesystem::path>){
			text << cont.native();
		} else {
			text << cont;
		}
		assign( text.str() );
	}
	MSubject( const std::string &cont );
	MSubject( std::string &&cont );
};
/**
 * Wrapper to explicitly mark something as non-"Subject" in a logging message.
 * See MSubject for the opposite.
 */
class NoSubject : public std::string
{
public:
	template<typename T> explicit NoSubject( const T &cont ) {
		std::ostringstream text;
		text << cont;
		assign( text.str() );
	}
	explicit NoSubject( const std::string &cont );
	explicit NoSubject( std::string &&cont );
};

const char *logLevelName( LogLevel level );

template<class MODULE> class Log;
class Message;

///Abstract base class for message output handlers
class MessageHandlerBase
{
	static LogLevel m_stop_below;
	std::mutex mutex;
protected:
	explicit MessageHandlerBase( LogLevel level ): m_level( level ) {}
	virtual ~MessageHandlerBase() = default;
public:
	LogLevel m_level;
	void guardedCommit(const Message &msg );
	virtual void commit( const Message &msg ) = 0;
	/**
	 * Set loglevel below which the system should stop the process.
	 * Sets the severity at which the OS should send a SIGSTOP to the process to halt it.
	 * This can be usefull for debugging usage.
	 * Example: \code
	 * util::DefaultMsgPrint::stopBelow(warning); //anything more severe than warning will raise SIGSTOP
	 * ...
	 * LOG(Runtime,error) << "This is realy bad.. we should stop and debug .."; //here for example
	 * \endcode
	 * \note available only on UNIX-Systems
	 * \note ignored on release builds
	 */
	static void stopBelow( LogLevel );
	bool requestStop( LogLevel _level );
};

class Message: protected std::ostringstream
{
	std::weak_ptr<MessageHandlerBase> commitTo;
public:
	std::string m_object, m_module;
	std::filesystem::path m_file;
	std::list<std::string> m_subjects;
	std::time_t m_timeStamp;
	int m_line;
	LogLevel m_level;
	Message( std::string object, std::string module, std::string file, int line, LogLevel level, std::weak_ptr<MessageHandlerBase> _commitTo );
	Message( const Message &src )=delete;
	Message( Message &&src ) noexcept ;
	~Message()override;
	std::string merge(const std::string& color_code)const;
	std::string strTime(const char *formatting="%c")const;
	template<size_t SIZE> Message &operator << ( const char (&str)[SIZE] ) { //send string literals as text
		*( ( std::ostringstream * )this ) << str;
		return *this;
	}
	template<typename T> Message &operator << (const T& val ) { // for everything else default to MSubject
		m_subjects.push_back( MSubject( val ) );
		*( ( std::ostringstream * )this ) << "{s}";
		return *this;
	}
	Message &operator << (const NoSubject& subj ) { // explicitly not a subject
		*( ( std::ostringstream * )this ) << subj;
		return *this;
	}
	bool shouldCommit()const;
	std::string str()const;
};

/**
 * Default message output class.
 * Will print any issued message to the given output stream in the format:  "LOG_MODULE_NAME:LOG_LEVEL_NAME[LOCATION] MESSAGE"
 * The default output stream is std::cout. But can be set using setStream.
 * Location is the calling Object/Method if compiled without debug infos (NDEBUG is set) or FILENAME:LINE_NUMER if compiled with debug infos.
 */
class DefaultMsgPrint : public MessageHandlerBase
{
	bool istty;
public:
	explicit DefaultMsgPrint( LogLevel level );
	void commit( const Message &mesg )final;
	void commit_tty(const Message &mesg);
	void commit_pipe(const Message &mesg);
};

}
}
