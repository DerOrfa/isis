/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2013  Enrico Reimer <reimer@cbs.mpg.de>

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


#pragma once

#include <string>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <isis/core/chunk.hpp>
#include <streambuf>

extern "C"
{
#include <libssh2.h>
#include <libssh2_sftp.h>

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
}

#include <mutex>
#include <sstream>
#include <iostream>
#include <atomic>
#include <iomanip>

namespace isis::image_io
{
namespace _internal{
struct ssh_session_deleter{void operator()(LIBSSH2_SESSION *p){libssh2_session_free(p);}};
struct sftp_session_deleter{void operator()(LIBSSH2_SFTP *p){libssh2_sftp_shutdown(p);}};
}
class SftpClient
{
	int _sock;
	std::unique_ptr<LIBSSH2_SESSION,_internal::ssh_session_deleter> session;
	std::unique_ptr<LIBSSH2_SFTP, _internal::sftp_session_deleter> sftp;

public:
	bool is_dir(const std::string &path);
	std::list<std::string> get_listing(const std::string &path);
	class streambuf: public std::streambuf
	{
	public:
		explicit streambuf(LIBSSH2_SFTP_HANDLE* file, std::size_t buff_sz = 1024 * 512);
		// copying not allowed
		streambuf(const streambuf &) = delete;
		streambuf &operator=(const streambuf &) = delete;

	private:
		// overrides base class underflow()
		int_type underflow();

	private:
		LIBSSH2_SFTP_HANDLE* p_file;
		std::vector<char> buffer_;
	};
	SftpClient(const std::string& host, const std::string& username, const std::string& keyfile);
	~SftpClient();
	[[nodiscard]] std::list<isis::data::Chunk> load_file(std::string remotePath, std::list<util::istring> list) const;
protected:
	bool open(std::string host, std::string username, std::string keyfile_name);
	LIBSSH2_SESSION *init();
	bool ok_or_throw(int err);
};
}
