#include "fftw.hxx"
#include <fftw3.h>
#include <type_traits>
#include "../../core/valuearray_typed.hpp"

#ifdef HAVE_FFTW
class PlanObj{
	fftw_plan plan;
	isis::data::TypedArray<std::complex< double >> buffer;
	std::array<size_t,4> shape;
public:
	PlanObj(const isis::data::TypedChunk<std::complex< double >>& data, int sign):buffer(data),shape(data.getSizeAsVector()){
		fftw_complex *ptr=(fftw_complex*)buffer.getRawAddress().get();
		plan=fftw_plan_dft_3d(shape[0],shape[1],shape[2],ptr,ptr,sign, FFTW_ESTIMATE);// TODO this is NOT thread safe consider adding fftw_make_planner_thread_safe (needs FFTW-3.3.5)
	}
	~PlanObj(){
		fftw_destroy_plan(plan);
	}
	operator fftw_plan(){return plan;}
};
void isis::math::fftw::_internal::fft_impl(isis::data::TypedChunk< std::complex< double > > &data, bool inverse)
{
	PlanObj p(data,inverse ? FFTW_BACKWARD:FFTW_FORWARD);
	fftw_execute(p);
}
#endif

#ifdef HAVE_FFTWf
class PlanObjF{
	fftwf_plan plan;
	isis::data::TypedArray<std::complex< float >> buffer;
	std::array<size_t,4> shape;
public:
	PlanObjF(const isis::data::TypedChunk<std::complex< float >>& data, int sign):buffer(data),shape(data.getSizeAsVector()){
		fftwf_complex *ptr=(fftwf_complex*)buffer.getRawAddress().get();
		plan=fftwf_plan_dft_3d(shape[0],shape[1],shape[2],ptr,ptr,sign, FFTW_ESTIMATE);
	}
	~PlanObjF(){
		fftwf_destroy_plan(plan);
	}
	operator fftwf_plan(){return plan;}
};
void isis::math::fftw::_internal::fft_impl(isis::data::TypedChunk< std::complex< float > > &data, bool inverse)
{
	PlanObjF p(data,inverse ? FFTW_BACKWARD:FFTW_FORWARD);
	fftwf_execute(p);
}
#endif