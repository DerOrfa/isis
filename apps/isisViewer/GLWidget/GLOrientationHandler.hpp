#ifndef ORIENTATIONHANDLER_HPP
#define ORIENTATIONHANDLER_HPP

#include "GLCommon.hpp"
#include "ImageHolder.hpp"
#include "common.hpp"
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/io.hpp>
#include <boost/numeric/ublas/lu.hpp>
#include <boost/concept_check.hpp>


namespace isis
{
namespace viewer
{


class GLOrientationHandler
{
	static const unsigned short matrixSize;
public:
	typedef boost::numeric::ublas::matrix<float> MatrixType;
	typedef boost::numeric::ublas::vector<float> VectorType;

	static MatrixType transformToPlaneView( const MatrixType &origMatrix, PlaneOrientation orientation, bool back = false );

	static void recalculateViewport( size_t w, size_t h, util::fvector4 mappedVoxelSize, util::ivector4 mappedImageSize , GLint *viewport, size_t border = 0 );

	static util::ivector4 transformObject2VoxelCoords( const util::fvector4 objectCoords, const boost::shared_ptr<ImageHolder> image, PlaneOrientation orientation );

	static util::dvector4 transformVoxel2ObjectCoords( const util::ivector4 voxelCoords, const boost::shared_ptr<ImageHolder> image, MatrixType orientation );

	//some helper functions
	static void boostMatrix2Pointer( const MatrixType &boostMatrix, GLdouble *pointerMatrix );

	static MatrixType addOffset( const MatrixType &matrix );

	static void makeIdentity( GLdouble *matrix );

	template<typename TYPE>
	static util::FixedVector<TYPE, 4> transformVector( util::FixedVector<TYPE, 4> inVec, boost::numeric::ublas::matrix<TYPE> transform ) {
		boost::numeric::ublas::vector<TYPE> tempVec( matrixSize );

		for( size_t i = 0; i < matrixSize; i++ ) {
			tempVec( i ) = inVec[i];
		}

		boost::numeric::ublas::vector<TYPE> result = boost::numeric::ublas::prod( transform, tempVec );
		util::FixedVector<TYPE, 4> retVec;

		for( size_t i = 0; i < matrixSize; i++ ) {
			retVec[i] = result( i );
		}

		return retVec;
	}

};



}
}// end namespace



#endif