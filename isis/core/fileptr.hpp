
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
namespace _internal{
	/// RAII-Object that will either transfer its handle to the mapping shared_ptr or free it after leaving context when conventional reading is done
	class FileHandle{
		std::filesystem::path filename;
#ifdef WIN32
		static const FILE_HANDLE invalid_handle = INVALID_HANDLE_VALUE;
#else
		static const FILE_HANDLE invalid_handle = -1;
#endif
		FILE_HANDLE handle=invalid_handle;

	public:
		FileHandle()=delete;
		FileHandle(const FileHandle &) = delete;
		explicit FileHandle(const std::filesystem::path &_filename);
		~FileHandle();
		operator FILE_HANDLE()const{return handle;}
		bool good();
		static bool closeHandle(FILE_HANDLE handle, const std::filesystem::path &filename);
		FILE_HANDLE release();
	};
}
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
	friend class _internal::FileHandle;
	struct Closer {
		FILE_HANDLE file,mmaph;
		size_t len;
		std::filesystem::path filename;
		bool write;
		void operator()( void *p );
	};
	bool map( _internal::FileHandle &&file, size_t len, bool write, const std::filesystem::path &filename );

	size_t checkSize(bool write, const std::filesystem::path &filename, size_t size);
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

	bool good() const;
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

