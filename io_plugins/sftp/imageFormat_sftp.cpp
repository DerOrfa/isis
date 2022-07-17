
#include <isis/core/io_interface.h>
#include <isis/core/io_factory.hpp>
#include <regex>

#include <filesystem>

#include "sftpclient.hpp"
#include <isis/core/fileptr.hpp>


namespace isis::io
{

class ImageFormat_Sftp: public FileFormat{
protected:
	std::list<util::istring> suffixes( io_modes modes )const override {return {"sftp"};}
public:
	std::string getName()const override {return "sftp reading proxy";};
	void write( const data::Image &image, const std::string &filename, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback )override {
		throwGenericError( "Not implemented (yet)" );
	}
	std::list<data::Chunk>	load( const std::filesystem::path &filename, std::list<util::istring> formatstack, std::list<util::istring> dialects, std::shared_ptr<util::ProgressFeedback> feedback )override{
		static const std::regex accepted_url("sftp:\\/\\/(\\w+)@(\\w+)(\\/.*)",std::regex::ECMAScript|std::regex::optimize);
		std::list<data::Chunk> ret;

		std::cmatch match;
		if(std::regex_match(filename.c_str(),match,accepted_url)){
			std::string host = match[2];
			std::string user = match[1];
			std::string remote_path = match[3];
			std::string keyfile = std::string(std::getenv("HOME"))+"/.ssh/id_rsa";//@todo actually look for the keyfile
			SftpClient client(host,user,keyfile);

			if(formatstack.back()=="sftp")
				formatstack.pop_back(); //remove the "sftp"

			if(client.is_dir(remote_path)) {
				for(auto file:client.get_listing(remote_path)){
					ret.splice(ret.end(),client.load_file(remote_path, formatstack));
				}

			} else
				ret= client.load_file(remote_path, formatstack);

		} else
			throwGenericError("Filename must an url like sftp://<username>@<host>/<path on the server>");

		return ret;
	}
};

}
isis::io::FileFormat *factory()
{
	return new isis::io::ImageFormat_Sftp();
}
