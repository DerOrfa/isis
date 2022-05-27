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
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <isis/core/chunk.hpp>
#include <streambuf>

namespace isis::image_io
{

class SftpClient
{
	ssh_session ssh;
	sftp_session sftp;

public:
	bool is_dir(const std::string &path);
	std::list<std::string> get_listing(const std::string &path);
	class streambuf: public std::streambuf
	{
	public:
		explicit streambuf(sftp_file file, std::size_t buff_sz = 1024 * 512);
		// copying not allowed
		streambuf(const streambuf &) = delete;
		streambuf &operator=(const streambuf &) = delete;

	private:
		// overrides base class underflow()
		int_type underflow();

	private:
		sftp_file p_file;
		std::vector<char> buffer_;
	};
	SftpClient(std::string host, std::string username, std::string keyfile);
	~SftpClient();
	std::list<isis::data::Chunk> load_file(std::string remotePath, std::list<util::istring> list) const;
};
}
