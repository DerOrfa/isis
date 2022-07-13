#include "imageFormat_Dicom.hpp"
#include <openjpeg.h>
#include <thread>

namespace isis
{
namespace io
{

namespace _internal
{
class ByteStream:public data::ByteArray{
	size_t read_idx;
public:
	ByteStream(const data::ByteArray &src):data::ByteArray(src),read_idx(0){}
	OPJ_SIZE_T read(void * p_buffer, OPJ_SIZE_T p_nb_bytes){
		uint8_t *start=begin()+read_idx;
		OPJ_OFF_T len=skip(p_nb_bytes);
		if(len>0){
			memcpy(p_buffer,start,len);
			return len;
		} else 
			return 0;
	}
	OPJ_OFF_T skip(OPJ_OFF_T p_nb_bytes){
		OPJ_OFF_T len = std::min<OPJ_OFF_T>(p_nb_bytes,getLength()-read_idx); //make sure we don't skip beyond the data block
		if(len>0){ //negative skipping is forbidden
			read_idx+=len;
			return len;
		} else 
			return -1;//and thus an error
	}
	OPJ_BOOL seek(OPJ_OFF_T p_nb_bytes){
		skip(p_nb_bytes);
		return read_idx<getLength();
	}
	
};
void jp2_err(const char *msg, void */*client_data*/){
	std::string no_endl_msg(msg);
	no_endl_msg=no_endl_msg.substr(0,no_endl_msg.find_last_not_of("\r\n")+1);
    LOG(Runtime,error) << "Got error " << no_endl_msg << " when decoding jp2 stream";
}
void jp2_warn(const char *msg, void */*client_data*/){
	std::string no_endl_msg(msg);
	no_endl_msg=no_endl_msg.substr(0,no_endl_msg.find_last_not_of("\r\n")+1);
    LOG(Runtime,warning) << "Got warning " << no_endl_msg << " when decoding jp2 stream";
}
void jp2_info(const char *msg, void */*client_data*/){
	std::string no_endl_msg(msg);
	no_endl_msg=no_endl_msg.substr(0,no_endl_msg.find_last_not_of("\r\n")+1);
	LOG(Runtime,info) << "Got info " << no_endl_msg << " when decoding jp2 stream";
}
OPJ_SIZE_T opj_stream_read_mem(void * p_buffer, OPJ_SIZE_T p_nb_bytes, void * p_user_data){
	return reinterpret_cast<ByteStream*>(p_user_data)->read(p_buffer,p_nb_bytes); 
}
OPJ_OFF_T opj_stream_skip_mem(OPJ_OFF_T p_nb_bytes, void * p_user_data){
	return reinterpret_cast<ByteStream*>(p_user_data)->skip(p_nb_bytes);
}
OPJ_BOOL opj_stream_seek_mem(OPJ_OFF_T p_nb_bytes, void * p_user_data){
	return reinterpret_cast<ByteStream*>(p_user_data)->seek(p_nb_bytes);
}

data::Chunk getj2k(data::ByteArray bytes){
	// set up stream
	_internal::ByteStream stream(bytes);

	struct stream_delete{
		void operator()(opj_stream_t *p){opj_stream_destroy(p);}
	};
	struct codec_delete{
		void operator()(opj_codec_t *p){opj_destroy_codec(p);}
	};
	struct image_delete{
		void operator()(opj_image_t *p){opj_image_destroy(p);}
	};

	std::unique_ptr<opj_stream_t,stream_delete> l_stream(opj_stream_default_create(true));
	opj_stream_set_user_data(l_stream.get(),&stream,nullptr);
	opj_stream_set_user_data_length(l_stream.get(),bytes.getLength());

	std::unique_ptr<opj_codec_t,codec_delete> l_codec(opj_create_decompress(OPJ_CODEC_J2K));
	
	opj_set_info_handler(l_codec.get(), jp2_info, 00);
	opj_set_warning_handler(l_codec.get(), jp2_warn, 00);
	opj_set_error_handler(l_codec.get(), jp2_err, 00);
	
	const int threads=std::thread::hardware_concurrency();
	if(threads)
		opj_codec_set_threads(l_codec.get(), std::min(threads,4));

	opj_stream_set_read_function(l_stream.get(), opj_stream_read_mem);
	opj_stream_set_skip_function(l_stream.get(), opj_stream_skip_mem);
	// see https://github.com/uclouvain/openjpeg/issues/613
	// opj_stream_set_seek_function(l_stream, opj_stream_seek_mem);


	/* set decoding parameters to default values */
	opj_dparameters_t parameters;
	opj_set_default_decoder_parameters(&parameters);
	
	if (!opj_setup_decoder(l_codec.get(), &parameters)) {
		FileFormat::throwGenericError("failed to setup the j2k decoder");
	}

	opj_image_t* pimage = NULL;
	std::unique_ptr<opj_image_t> image;
	/* Read the main header of the codestream and if necessary the JP2 boxes*/
	if (! opj_read_header(l_stream.get(), l_codec.get(), &pimage)) {
		opj_image_destroy(pimage);
		FileFormat::throwGenericError("failed to read the j2k header");
	} else
		image.reset(pimage);
	
	if (!(opj_decode(l_codec.get(), l_stream.get(), image.get()) && opj_end_decompress(l_codec.get(),   l_stream.get()))) {
		FileFormat::throwGenericError("failed to decode the j2k image!\n");
	}
	if (image->comps[0].data == NULL) {
		FileFormat::throwGenericError("no j2k image data!");
	}
	if (image->color_space != OPJ_CLRSPC_SYCC
			&& image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy
			&& image->comps[1].dx != 1) {
		image->color_space = OPJ_CLRSPC_SYCC;
	} else if (image->numcomps <= 2) {
		image->color_space = OPJ_CLRSPC_GRAY;
	}
	
	if(image->numcomps!=1)
		FileFormat::throwGenericError("Only grayscale j2k data supportet");

	
	if(image->comps[0].prec>8){
		return data::MemChunk<uint16_t>(image->comps[0].data, image->comps[0].w,image->comps[0].h);
	} else {
		return data::MemChunk<uint8_t>(image->comps[0].data, image->comps[0].w,image->comps[0].h);
	} 
}
}}}
