#include "fft.hpp"
#include "common.hpp"

#ifdef HAVE_GSL
#include "gsl/fft.hxx"
#endif // HAVE_GSL

#ifdef HAVE_CLFFT
#include "details/clfft.hxx"
#endif //HAVE_CLFFT

#if defined(HAVE_FFTW) || defined(HAVE_FFTWf)
#include "details/fftw.hxx"
#endif //HAVE_FFTW

isis::data::Chunk isis::math::fft(isis::data::Chunk data, bool inverse, double scale)
{
	switch(data.getTypeID()){
	case util::typeID<uint8_t>():
	case util::typeID<uint16_t>():
	case util::typeID<int8_t>():
	case util::typeID<int16_t>():
	case util::typeID<float>():
	case util::typeID<std::complex< float >>():
		return fft_single(data,inverse,scale);
	default:
		return fft_double(data,inverse,scale);
	}
}

isis::data::TypedChunk< std::complex< float > > isis::math::fft_single(isis::data::MemChunk< std::complex< float > > data, bool inverse, float scale)
{
#ifdef HAVE_CLFFT
	LOG(Runtime,info) << "Using single precision clfft to transform " << data.getSizeAsString() << " data";
	if(cl::fft(data,inverse,scale))//if it fails, fall through
		return data;
#endif
#if HAVE_FFTWf
	LOG(Runtime,info) << "Using single precision fftw to transform " << data.getSizeAsString() << " data";
	if(fftw::fft(data,inverse,scale))//if it fails fall through
		return data;
#endif
	LOG(Runtime,error) << "Sorry, no single precision fft support compiled in (enable clFFT and/or fftw)";
	return isis::data::TypedChunk< std::complex< float > >();
}

isis::data::TypedChunk< std::complex< double > > isis::math::fft_double(isis::data::MemChunk< std::complex< double > > data, bool inverse, double scale)
{
#ifdef HAVE_FFTW
	LOG(Runtime,info) << "Using double precision fftw to transform " << data.getSizeAsString() << " data";
	if(fftw::fft(data,inverse,scale))
		return data;
#endif
#if HAVE_GSL
	LOG(Runtime,info) << "Using double precision gsl_fft_complex_transform to transform " << data.getSizeAsString() << " data";
	if(gsl::fft(data,inverse,scale))
		return data;
#endif
	LOG(Runtime,error) << "Sorry, no double precision fft support compiled in (enable gsl and/or fftw)";
	return data;
}

isis::data::Image isis::math::fft(isis::data::Image img, bool inverse, double scale)
{
	auto chunks=img.copyChunksToVector(true);
	for(data::Chunk &ch:chunks){
		ch=fft(ch);
	}
	return data::Image(chunks);
}


