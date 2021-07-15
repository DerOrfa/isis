
#pragma once

#include <filesystem>
#include <sys/resource.h>
#include "bytearray.hpp"
#include "endianess.hpp"
#include <sys/resource.h>

#ifdef WIN32
#include <windows.h>
#define FILE_HANDLE HANDLE
#else
#define FILE_HANDLE int
#endif

namespace isis::data
{
/**
 * Class to map files into memory.
 * This can be used read only, or for read/write.
 *
 * Writing to a FilePtr mapping a file read-only is valid. It will not change the mapped file.
 *
 * This is inheriting from ValueArray. Thus this, and all ValueArray created from it will be managed.
 * The mapped file will automatically unmapped and closed after all pointers are deleted.
 */
class FilePtr: public ByteArray
{
	struct Closer {
		FILE_HANDLE file, mmaph;
		size_t len;
		std::filesystem::path filename;
		bool write;
		void operator()( void *p );
	};
	bool map( FILE_HANDLE file, size_t len, bool write, const std::filesystem::path &filename );

	size_t checkSize( bool write, FILE_HANDLE file, const std::filesystem::path &filename, size_t size = 0 );
	bool m_good=false;
	static rlim_t file_count;
public:
	/// empty creator - result will not be useful until filled
	FilePtr() = default;
	/**
	 * Create a FilePtr, mapping the given file.
	 * if the write is true:
	 * - the file will be created if it does not exist
	 * - the file will be resized to len if it is shorter than len
	 * - a resize after this creation is NOT possible
	 *
	 * if write is false:
	 * - the length of the pointer will become the size of the file if len is 0 (so the whole file will be mapped)
	 * - if len is not 0 but less then the filesize, only len bytes of the file will be mapped
	 *
	 * creation will fail (good()!=true afterwards) if:
	 * - open/mmap fail on the given file for any reason
	 * - if file does not exist, write is true and len is 0
	 * - if write is false and len is greater then the length of the file or the file does not exist
	 *
	 * \param filename the file to map into memory
	 * \param len the requested length of the resulting ValueArray in bytes (automatically set if 0)
	 * \param write the file be opened for writing (writing to the mapped memory will write to the file, otherwise it will cause a copy-on-write)
	 * \param mapsize size below which the file will actually be copied into memory (instead of being mapped). Applies only when write is false.
	 */
	explicit FilePtr(const std::filesystem::path &filename, size_t len = 0, bool write = false, size_t mapsize = 2*1024*1024);

	bool good();
	void release();

	/**
	 * Check if additional files can be opened.
	 * This uses getrlimit to check if the already open FilePtr's plus the given amount would exceed the systems limits.
	 * @param additional_files amount of files expected to be opened
	 * @return true if amount is within the limit or the limit could be raised sufficiently, false otherwise
	 */
	static bool checkLimit(rlim_t additional_files);
};
}
