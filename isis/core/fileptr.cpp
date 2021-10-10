//
//  fileptr.cpp
//  isis
//
//  Created by Enrico Reimer on 08.08.11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#include "fileptr.hpp"

#ifdef WIN32
#else
#include <sys/mman.h>
#endif

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include "singletons.hpp"

#include <sys/time.h>
#include <sys/resource.h>

namespace isis::data
{

rlim_t FilePtr::file_count=0;

void FilePtr::Closer::operator()( void *p )
{
	LOG( Debug, verbose_info ) << "Unmapping and closing " << util::MSubject( filename ) << " it was mapped at " << p;
	bool unmapped = false;
#ifdef WIN32
	unmapped = UnmapViewOfFile( p );
#else
	unmapped = !munmap( p, len );
#endif
	LOG_IF( !unmapped, Runtime, warning )
			<< "Unmapping of " << util::MSubject( filename )
			<< " failed, the error was: " << util::MSubject( util::getLastSystemError() );

#ifdef __APPLE__
	if( write && futimes( file, NULL ) != 0 ) {
		LOG( Runtime, warning )
				<< "Setting access time of " << util::MSubject( filename )
				<< " failed, the error was: " << util::MSubject( strerror( errno ) );
	}
#elif WIN32
	if( write ) {
		FILETIME ft;
		SYSTEMTIME st;

		GetSystemTime( &st );
		bool ok = SystemTimeToFileTime( &st, &ft ) && SetFileTime( file, NULL, ( LPFILETIME ) NULL, &ft );
		LOG_IF( !ok, Runtime, warning )
				<< "Setting access time of " << util::MSubject( filename.file_string() )
				<< " failed, the error was: " << util::MSubject( util::getLastSystemError() );
	}
#endif

#ifdef WIN32
	CloseHandle( mmaph );
#endif
	_internal::FileHandle::closeHandle(file,filename);
}

bool FilePtr::map( _internal::FileHandle &&file, size_t len, bool write, const std::filesystem::path &filename )
{
	void *ptr = nullptr;
	FILE_HANDLE mmaph = 0;
	rlimit rlim;
	getrlimit(RLIMIT_DATA,&rlim);
	
	if(rlim.rlim_cur<len){
		if(rlim.rlim_max>len){
			rlim.rlim_cur=len;
			setrlimit(RLIMIT_NOFILE, &rlim);
		} else {
			LOG(Runtime,warning) << "Can't increase the limit for for mapped file size to " << len << ", this will crash..";
		}
	}
	assert(file.good());

#ifdef WIN32 //mmap is broken on Windows - workaround stolen from http://crupp.de/2007/11/14/howto-port-unix-mmap-to-win32/
	mmaph = CreateFileMapping( file, 0, write ? PAGE_READWRITE : PAGE_WRITECOPY, 0, 0, NULL );

	if( mmaph ) {
		ptr = MapViewOfFile( mmaph, write ? FILE_MAP_WRITE : FILE_MAP_COPY, 0, 0, 0 );
	}

#else
	ptr = mmap( 0, len, PROT_WRITE | PROT_READ, write ? MAP_SHARED : MAP_PRIVATE , file, 0 ); // yes we say PROT_WRITE here also if the file is opened ro - It's for the mapping, not for the file
#endif

	if( ptr == nullptr || ptr == reinterpret_cast<void*>(-1) ) {
		switch(errno){
			case ENOMEM:
				LOG( Runtime, error ) << "Exceeded limit of mapping size or amount of mapped files";break;
			default:
				LOG( Runtime, error ) << "Failed to map " << util::MSubject( filename ) << ", error was " << util::getLastSystemError();
		}
		return false;
	} else {
		const Closer cl = {file.release(), mmaph, len, filename, write};
		writing = write;
		static_cast<ByteArray&>( *this ) = ByteArray( std::shared_ptr<uint8_t>(static_cast<uint8_t * const>( ptr ),cl), len );
		return true;
	}
}

size_t FilePtr::checkSize(bool write, const std::filesystem::path &filename, size_t size)
{
	const auto currSize = std::filesystem::file_size( filename );
	if ( std::numeric_limits<size_t>::max() < currSize ) {
		LOG( Runtime, error )
			<< "Sorry cannot map files larger than " << std::numeric_limits<size_t>::max()
			<< " bytes on this platform";
		return 0;
	}

	if( write ) { // if we're writing
		assert( size > 0 );

		if( size > currSize ) { // and the file is shorter than requested, resize it
			std::error_code ec;
			std::filesystem::resize_file(filename,size,ec);
			if(ec){
				 LOG( Runtime, error )
					<< "Failed to resize " << util::MSubject( filename )
					<< " to the requested size " << size << ", the error was: " << util::MSubject( ec.message() );
					return 0; // fail
			}
		}
		return std::filesystem::file_size(filename);
	} else { // if we're reading
		if( size == 0 )
			return currSize; // automatically select size of the file
		else if( size <= currSize )
			return size; // keep the requested size (will fit into the file)
		else { // size will not fit into the file (and we cannot resize) => fail
			LOG( Runtime, error ) << "The requested size for readonly mapping of " << util::MSubject( filename )
								  << " is greater than the filesize (" << currSize << ").";
			return 0; // fail
		}
	}
}

FilePtr::FilePtr(const std::filesystem::path &filename, size_t len, bool write, size_t mapsize)
{
	_internal::FileHandle file(filename,!write);
	if(!file.good())
		return;

	const size_t file_size = checkSize(write, filename, len); // get the mapping size

	if(file_size > mapsize || write) {
		m_good = map(std::move(file), file_size, write, filename); //and do the mapping
		LOG(Debug, verbose_info) << "Mapped " << file_size << " bytes of " << filename << " at " << getRawAddress().get();
	}
	else if(file_size>0)
	{
		static_cast<ValueArray*>(this)->operator=(ValueArray::make<uint8_t>(file_size));
		if(!getRawAddress()){
			LOG(Runtime,error) << "Failed creating memory for reading " << filename << ", the error was: " << util::getLastSystemError();
			return;
		}

		size_t read_pos=0;
		while (read_pos<file_size){
			const auto result = read(file,getRawAddress(read_pos).get(),file_size);
			if(result>0) {
				read_pos += result;
			} else if(result==0) {
				LOG(Runtime,warning) << "Unexpected end reading " << filename << " (missing " << file_size-read_pos << " bytes)";
				break;
			} else {
				LOG(Runtime, error) << "Error reading " << filename << ", the error was: " << util::getLastSystemError();
				return;
			}
		}
		m_good = true;
	} else {
		LOG(Runtime,error) << "Not opening empty file";
	}
	// from here on the pointer will be set if mapping succeeded
}

bool FilePtr::good() const {
	return m_good && getRawAddress();
}

void FilePtr::release()
{
	castTo<uint8_t>().reset();
	m_good = false;
}

bool FilePtr::checkLimit(rlim_t additional_files)
{
	rlimit rlim;
	rlim_t needed=file_count+additional_files+50;
	getrlimit(RLIMIT_NOFILE, &rlim);
	if(rlim.rlim_cur<needed){
		if(rlim.rlim_max>needed){
			rlim.rlim_cur=needed;
			setrlimit(RLIMIT_NOFILE, &rlim);
		} else {
			return false;
		}
	}
	return true;
}

_internal::FileHandle::FileHandle(const std::filesystem::path &_filename, bool readonly):filename(_filename)
{
#ifdef WIN32
	const int oflag = write ?
					  GENERIC_READ | GENERIC_WRITE :
					  GENERIC_READ; //open file readonly
	handle =
		CreateFile( filename.file_string().c_str(), oflag, write ? 0 : FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#else
	const int oflag = readonly ?
					  O_RDONLY: //open file readonly
					  O_CREAT | O_RDWR ; //create file if it's not there
	handle =
		open( filename.native().c_str(), oflag, 0666 );
#endif
	if( good() )
		++FilePtr::file_count;
	else
		LOG( Runtime, error ) << "Failed to open " << filename << ", the error was: " << util::getLastSystemError();
}

_internal::FileHandle::~FileHandle(){
	if(handle != invalid_handle)
		closeHandle(handle,filename);
}

bool _internal::FileHandle::good()
{
	return handle != invalid_handle;
}

bool _internal::FileHandle::closeHandle(int handle, const std::filesystem::path &filename)
{
	assert(handle!=invalid_handle);
#ifdef WIN32
	if( CloseHandle( handle ) ) ) {
#else

	if( ::close( handle ) != 0 ) {
#endif
		LOG( Runtime, warning )
			<< "Closing of " << util::MSubject( filename )
			<< " failed, the error was: " << util::MSubject( strerror( errno ) );
		return false;
	}

	--FilePtr::file_count;
	return true;
}
FILE_HANDLE _internal::FileHandle::release()
{
	FILE_HANDLE ret=handle;
	handle=invalid_handle;
	return ret;
}

}
