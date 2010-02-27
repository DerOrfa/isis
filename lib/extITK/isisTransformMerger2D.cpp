/*
 * isisTransformMerger.cpp
 *
 *  Created on: Oct 20, 2009
 *      Author: tuerke
 */
#ifndef ISISTRANSFORMMERGER2D_H_
#define ISISTRANSFORMMERGER2D_H_

#include "isisTransformMerger2D.hpp"

namespace isis {
namespace extitk {

TransformMerger2D::TransformMerger2D() {
    transformType_ = 0;
    tmpTransform_ = BSplineDeformableTransformType::New();
    outputTransform_ = BSplineDeformableTransformType::New();
    addImageFilter_ = AddImageFilterType::New();

}

void TransformMerger2D::merge(
    void) {
	
    DeformationFieldIteratorType fi(temporaryDeformationField_, imageRegion_);
	itk::Transform<double, 2, 2>::InputPointType fixedPoint;
	itk::Transform<double, 2, 2>::OutputPointType movingPoint;
	DeformationFieldType::IndexType index;
	VectorType displacement;
	
    //go through the transform list and create a vector deformation field (temporaryDeformationField_). Then combining this deformation field with the final vector field (deformationField_).
    for (transformIterator_ = this->begin(); transformIterator_ != this->end(); transformIterator_++) {
        fi.GoToBegin();
		itk::Transform<double, 2, 2>* transform =  dynamic_cast<itk::Transform<double, 2, 2>* >(*transformIterator_);
		while(!fi.IsAtEnd())
		{
			index = fi.GetIndex();
			temporaryDeformationField_->TransformIndexToPhysicalPoint(index, fixedPoint);
			movingPoint = transform->TransformPoint(fixedPoint);
			displacement = movingPoint - fixedPoint;
			fi.Set(displacement);
			++fi;	
		}
		addImageFilter_->SetInput1(deformationField_);
		addImageFilter_->SetInput2(temporaryDeformationField_);
		addImageFilter_->Update();
		deformationField_ = addImageFilter_->GetOutput();

    }

}

TransformMerger2D::DeformationFieldType::Pointer TransformMerger2D::getTransform(
    void) {
    return deformationField_;

}

//here we setting up the temporaryDeformationField_ and deformationField_. The properties are defined be the templateImage which is specified by the setTemplateImage method,
void TransformMerger2D::setTemplateImage(itk::ImageBase<2>::Pointer templateImage)
{
    imageRegion_ = templateImage->GetLargestPossibleRegion();
    deformationField_ = DeformationFieldType::New();
    deformationField_->SetRegions(imageRegion_.GetSize());
    deformationField_->SetOrigin(templateImage->GetOrigin());
    deformationField_->SetSpacing(templateImage->GetSpacing());
    deformationField_->SetDirection(templateImage->GetDirection());
    deformationField_->Allocate();

    temporaryDeformationField_ = DeformationFieldType::New();
    temporaryDeformationField_->SetRegions(imageRegion_.GetSize());
    temporaryDeformationField_->SetOrigin(templateImage->GetOrigin());
    temporaryDeformationField_->SetSpacing(templateImage->GetSpacing());
    temporaryDeformationField_->SetDirection(templateImage->GetDirection());
    temporaryDeformationField_->Allocate();
	
}

}//end namespace extitk
}//end namespace isis

#endif //ISISTRANSFORMMERGER2D_H_
