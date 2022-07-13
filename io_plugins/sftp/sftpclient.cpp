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
#include <netdb.h>

namespace isis::io
{

SftpClient::streambuf::streambuf(LIBSSH2_SFTP_HANDLE *fptr, size_t buff_sz)
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
	size_t n = libssh2_sftp_read(p_file, start, buffer_.size() - (start - base));
	if (n == 0)
		return traits_type::eof();

	// Set buffer pointers
	setg(base, start, start + n);

	return traits_type::to_int_type(*gptr());
}

SftpClient::SftpClient(const std::string& host, const std::string& username, const std::string& keyfile)	: session(init())
{
	if (session) {
		libssh2_session_set_blocking(session.get(), 1);
		open(host,username, keyfile);
	}
	else LOG(Runtime, error) << "Failed to create ssh session";
}

SftpClient::~SftpClient()
{
	libssh2_exit();
#ifdef WIN32
	closesocket(_sock);
#else
	close(_sock);
#endif
}

std::list<isis::data::Chunk> SftpClient::load_file(std::string remotePath, std::list<util::istring> formatstack) const
{
	std::list<isis::data::Chunk> ret;
	if (session && sftp) {
		auto file = libssh2_sftp_open_ex(sftp.get(),
										 remotePath.c_str(),
										 remotePath.length(),
										 LIBSSH2_FXF_READ,
										 0,
										 LIBSSH2_SFTP_OPENFILE);
		if (file) {
			streambuf buff(file);
			if (formatstack.empty()) formatstack = util::stringToList<util::istring>(remotePath, '.');
			try {
				ret = isis::data::IOFactory::loadChunks(&buff, formatstack);
			}
			catch (isis::data::IOFactory::io_error &e) {
				LOG(Runtime, error) << e.what() << " while reading " << remotePath << " from sftp.";
				libssh2_sftp_close(file);
				throw;
			}
			libssh2_sftp_close(file);
		}
		else {
			LOG(Runtime, warning) << "Failed to open remote file " << remotePath;
		}
	}
	else {
		LOG(Runtime, error) << "sftp connection was not established, will return empty list";
	}
	return ret;
}
std::list<std::string> SftpClient::get_listing(const std::string &remotePath)
{
	auto handle = libssh2_sftp_open_ex(sftp.get(), remotePath.c_str(), remotePath.length(), 0, 0, LIBSSH2_SFTP_OPENDIR);
	std::list<std::string> ret;
	char filename_buffer[2048];
	LIBSSH2_SFTP_ATTRIBUTES attr;
	int err;
	if (handle) {
		while ((err = libssh2_sftp_readdir(handle, filename_buffer, 2048, &attr))) {
			ok_or_throw(err);
			if (filename_buffer[0] == '.')continue; //skip hidden
			ret.push_back(remotePath + "/" + filename_buffer);
		}
	}
	else {
		FileFormat::throwGenericError(std::string("Failed to open sftp path ") + remotePath);
	}
	return ret;
}
bool SftpClient::is_dir(const std::string &path)
{
	LIBSSH2_SFTP_ATTRIBUTES stat;
	ok_or_throw(libssh2_sftp_stat_ex(sftp.get(), path.c_str(), path.length(), LIBSSH2_SFTP_STAT, &stat));
	return LIBSSH2_SFTP_S_ISDIR(stat.flags);
}
LIBSSH2_SESSION *SftpClient::init()
{
	// Initialize stupid windows systems
#ifdef WIN32
	WSADATA wsadata;
		int err;

		err = WSAStartup(MAKEWORD(2, 0), &wsadata);
		if(err != 0) {
			fprintf(stderr, "WSAStartup failed with error: %d\n", err);
			return 1;
		}
#endif
	// Initialize libssh2 on a thread safe manner, count the session instances.
	auto err = libssh2_init(0);
	if (err) {
		LOG(Runtime, error) << "libssh2 initialization failed (" << err << ")";
		return nullptr;
	}

	_sock = socket(AF_INET, SOCK_STREAM, 0);
	return libssh2_session_init();
}

bool SftpClient::open(std::string host, std::string username, std::string keyfile_name)
{
	auto hst = gethostbyname(host.c_str());
	auto addr= inet_ntoa(**(in_addr**)hst->h_addr_list);

	sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(22);
	sin.sin_addr.s_addr = inet_addr(addr);
	if (connect(_sock, (struct sockaddr *) (&sin), sizeof(struct sockaddr_in)) != 0)
		io::FileFormat::throwSystemError(errno);

	ok_or_throw(libssh2_session_handshake(session.get(), _sock));

	/* check what authentication methods are available */
	auto userauthlist = libssh2_userauth_list(session.get(), username.c_str(), username.length());
	util::slist
		avail_auth = userauthlist ? util::stringToList<std::string>(std::string(userauthlist), ',') : util::slist();

	while (!avail_auth.empty()) {
		std::string pub_key=keyfile_name+".pub";
		if(std::find(avail_auth.begin(), avail_auth.end(),"publickey")!=avail_auth.end()){
			if(!ok_or_throw(libssh2_userauth_publickey_fromfile_ex(
				session.get(),
				username.c_str(), username.length(),
				pub_key.c_str(), keyfile_name.c_str(),
				nullptr)))
			{
				avail_auth.remove("publickey");
				LOG(Runtime,info) << util::MSubject("publickey") << " authentication failed, moving on to " << avail_auth.front();
			}
			break;
		}
		//if we got here no known options are left
		LOG(Runtime,warning) << "Ignoring unknown auth method " << avail_auth.front();
		avail_auth.pop_front();
	}
	if (avail_auth.empty()) {
		LOG(Runtime, error) << "Exhausted all available authentication methods on " << host << "(" << userauthlist
							<< ")";
		FileFormat::throwGenericError("authentication failed");
	}

	sftp.reset(libssh2_sftp_init(session.get()));
	return sftp.operator bool();
}
bool SftpClient::ok_or_throw(int err)
{
	switch (err) {
		case 0:
			return true;//success
		case LIBSSH2_ERROR_SOCKET_NONE:
			FileFormat::throwGenericError("The socket is invalid.");
			break;
		case LIBSSH2_ERROR_ALLOC:
			FileFormat::throwGenericError("An internal memory allocation call failed.");
			break;
		case LIBSSH2_ERROR_BANNER_SEND:
			FileFormat::throwGenericError("Unable to send banner to remote host.");
			break;
		case LIBSSH2_ERROR_KEX_FAILURE:
			FileFormat::throwGenericError("Encryption key exchange  with  the  remote host failed.");
			break;
		case LIBSSH2_ERROR_SOCKET_SEND:
			FileFormat::throwGenericError("Unable to send data on socket.");
			break;
		case LIBSSH2_ERROR_SOCKET_TIMEOUT:
			FileFormat::throwGenericError("Socket timeout");
			break;
		case LIBSSH2_ERROR_SOCKET_DISCONNECT:
			FileFormat::throwGenericError("The socket was disconnected.");
			break;
		case LIBSSH2_ERROR_PROTO:
			FileFormat::throwGenericError("An invalid SSH protocol response was received on the socket.");
			break;
		case LIBSSH2_ERROR_PUBLICKEY_UNVERIFIED:
			FileFormat::throwGenericError("The username/public-key combination was invalid.");
			break;
		default:
			FileFormat::throwGenericError("Unknown ssh error");
	}
	return false;
}
}
