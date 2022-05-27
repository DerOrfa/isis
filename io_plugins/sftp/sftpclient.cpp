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

#include <isis/core/log.hpp>
#include "sftpclient.hpp"
#include <fcntl.h>
#include <libssh2_sftp.h>
#include <isis/core/io_factory.hpp>

namespace isis::image_io
{

SftpClient::streambuf::streambuf(sftp_file fptr, size_t buff_sz)
	: p_file(fptr), buffer_(buff_sz + 1)
{
	char *end = &buffer_.front() + buffer_.size();
	setg(end, end, end);
}
std::streambuf::int_type SftpClient::streambuf::underflow()
{
	if (gptr() < egptr()) // buffer not exhausted
		return traits_type::to_int_type(*gptr());

	char *base = &buffer_.front();
	char *start = base;

	if (eback() == base) // true when this isn't the first fill
	{
		// Make arrangements for putback characters
		std::memmove(base, egptr() - 1, 1);
		start += 1;
	}

	// start is now the start of the buffer, proper.
	// Read from p_file in to the provided buffer
	size_t n = sftp_read(p_file, start, buffer_.size() - (start - base));
	if (n == 0)
		return traits_type::eof();

	// Set buffer pointers
	setg(base, start, start + n);

	return traits_type::to_int_type(*gptr());
}

SftpClient::SftpClient(std::string host, std::string username, std::string keyfile):ssh(ssh_new()), sftp(nullptr)
{
	if (ssh) {
		ssh_options_set(ssh, SSH_OPTIONS_HOST, host.c_str());
		ssh_options_set(ssh, SSH_OPTIONS_USER, username.c_str());

		if (ssh_connect(ssh) == SSH_OK) {
			const ssh_private_key priv = privatekey_from_file(ssh, keyfile.c_str(), 0, NULL);
			const ssh_string pub = publickey_to_string(publickey_from_privatekey(priv));
			if (ssh_userauth_pubkey(ssh, username.c_str(), pub, priv) == SSH_AUTH_SUCCESS) {
				if ((sftp = sftp_new(ssh))) {
					if (sftp_init(sftp) == SSH_OK)return; //only good way out
					else LOG(Runtime, error) << "Error initializing SFTP session " << sftp_get_error(sftp);
				}
				else LOG(Runtime, error) << "Error allocating SFTP session: " << ssh_get_error(ssh);

			}
			else LOG(Runtime, error) << "Cannot authenticate with " << host;
		}
		else LOG(Runtime, error) << "Error connecting to hsm: " << ssh_get_error(ssh);
	}
	else LOG(Runtime, error) << "Failed to create ssh session";
	ssh = nullptr;
	sftp = nullptr;
}

SftpClient::~SftpClient()
{
	sftp_free(sftp);
	ssh_free(ssh);
}

std::list<isis::data::Chunk> SftpClient::load_file(std::string remotePath, std::list<util::istring> formatstack) const
{
	std::list<isis::data::Chunk> ret;
	if (ssh && sftp) {
		sftp_file file = sftp_open(sftp, remotePath.c_str(), O_RDONLY, 0);
		if (file) {
			streambuf buff(file);
			if(formatstack.empty()) formatstack = util::stringToList<util::istring>(remotePath, '.');
			try {
				ret = isis::data::IOFactory::loadChunks(&buff,
														formatstack);// this is actually not yielding any data, as the matcher's "load" always returns an empty list
			}
			catch (isis::data::IOFactory::io_error &e) {
				LOG(Runtime, error) << e.what() << " while reading " << remotePath << " from sftp.";
				sftp_close(file);
				throw;
			}
			sftp_close(file);
		}
		else {
			LOG(Runtime, warning) << "Failed to open remote file " << remotePath << " " << ssh_get_error(ssh) << " "
								   << ssh_get_error(sftp);
		}
	}
	else {
		LOG(Runtime, error) << "sftp connection was not established, will return empty list";
	}
	return ret;
}
std::list<std::string> SftpClient::get_listing(const std::string &remotePath)
{
	sftp_dir handle = sftp_opendir(sftp, remotePath.c_str());
	std::list<std::string> ret;
	if (handle) {
		for (sftp_attributes found = sftp_readdir(sftp, handle); found; found = sftp_readdir(sftp, handle)) {
			if (found->name[0] == '.')continue; //skip hidden
			ret.push_back(remotePath + "/" + found->name);
		}
	}
	else {
		const std::string ssh_error=ssh_get_error(ssh);
		LOG(Runtime, error) << "Failed to open sftp path " << remotePath << " " << ssh_error;
		FileFormat::throwGenericError(ssh_error);
	}
	return ret;
}
bool SftpClient::is_dir(const std::string &path)
{
	auto stat=sftp_stat(sftp,path.c_str());
	return stat->type == LIBSSH2_SFTP_TYPE_DIRECTORY;
}
}
