#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "chunk.hpp"
#include "image.hpp"

/*
 * Add needed properties for Property-objects.
 * Objects which lack any of these properties will be rejected by the system.
 * see PropertyObject::sufficient()
 */

/// @cond _internal

// Stuff needed for every Chunk
std::list<isis::util::PropertyMap::PropPath>
    isis::data::Chunk::neededProperties = {"indexOrigin","acquisitionNumber","voxelSize","rowVec","columnVec"};

// Stuff needed for any Image
std::list<isis::util::PropertyMap::PropPath>
	isis::data::Image::neededProperties = {"voxelSize","rowVec","columnVec","sliceVec","sequenceNumber"};

std::list<isis::util::PropertyMap::PropPath>
    isis::data::Image::defaultChunkEqualitySet = {
		"DICOM/SeriesInstanceUID", "sequenceNumber","voxelSize","rowVec","columnVec","sliceVec","coilChannelMask","echoTime",
		"DICOM/EchoNumbers","DICOM/SIEMENS CSA HEADER/ImaCoilString"
	};

/// @endcond _internal
