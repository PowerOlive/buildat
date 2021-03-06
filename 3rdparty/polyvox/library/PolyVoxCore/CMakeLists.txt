# Copyright (c) 2008-2012 Matt Williams
# Copyright (c) 2008-2012 David Williams
# 
# This software is provided 'as-is', without any express or implied
# warranty. In no event will the authors be held liable for any damages
# arising from the use of this software.
# 
# Permission is granted to anyone to use this software for any purpose,
# including commercial applications, and to alter it and redistribute it
# freely, subject to the following restrictions:
# 
# 1. The origin of this software must not be misrepresented; you must not
# claim that you wrote the original software. If you use this software
# in a product, an acknowledgment in the product documentation would be
# appreciated but is not required.
# 
# 2. Altered source versions must be plainly marked as such, and must not be
# misrepresented as being the original software.
# 
# 3. This notice may not be removed or altered from any source
# distribution.

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(PolyVoxCore)

#Projects source files
SET(CORE_SRC_FILES
	source/ArraySizes.cpp
	source/AStarPathfinder.cpp
	source/Log.cpp
	source/MeshDecimator.cpp
	source/Region.cpp
	source/SimpleInterface.cpp
	source/VertexTypes.cpp
)

#Projects headers files
SET(CORE_INC_FILES
	include/PolyVoxCore/AmbientOcclusionCalculator.h
	include/PolyVoxCore/AmbientOcclusionCalculator.inl
	include/PolyVoxCore/Array.h
	include/PolyVoxCore/Array.inl
	include/PolyVoxCore/ArraySizes.h
	include/PolyVoxCore/AStarPathfinder.h
	include/PolyVoxCore/AStarPathfinder.inl
	include/PolyVoxCore/BaseVolume.h
	include/PolyVoxCore/BaseVolume.inl
	include/PolyVoxCore/BaseVolumeSampler.inl
	include/PolyVoxCore/ConstVolumeProxy.h
	include/PolyVoxCore/CubicSurfaceExtractor.h
	include/PolyVoxCore/CubicSurfaceExtractor.inl
	include/PolyVoxCore/CubicSurfaceExtractorWithNormals.h
	include/PolyVoxCore/CubicSurfaceExtractorWithNormals.inl
	include/PolyVoxCore/DefaultIsQuadNeeded.h
	include/PolyVoxCore/DefaultMarchingCubesController.h
	include/PolyVoxCore/Density.h
	include/PolyVoxCore/GradientEstimators.h
	include/PolyVoxCore/GradientEstimators.inl
	include/PolyVoxCore/Interpolation.h
	include/PolyVoxCore/IteratorController.h
	include/PolyVoxCore/IteratorController.inl
	include/PolyVoxCore/LargeVolume.h
	include/PolyVoxCore/LargeVolume.inl
	include/PolyVoxCore/LargeVolumeSampler.inl
	include/PolyVoxCore/Log.h
	include/PolyVoxCore/LowPassFilter.h
	include/PolyVoxCore/LowPassFilter.inl
	include/PolyVoxCore/MarchingCubesSurfaceExtractor.h
	include/PolyVoxCore/MarchingCubesSurfaceExtractor.inl
	include/PolyVoxCore/Material.h
	include/PolyVoxCore/MaterialDensityPair.h
	include/PolyVoxCore/MeshDecimator.h
	include/PolyVoxCore/MeshDecimator.inl
	include/PolyVoxCore/PolyVoxForwardDeclarations.h
	include/PolyVoxCore/RawVolume.h
	include/PolyVoxCore/RawVolume.inl
	include/PolyVoxCore/RawVolumeSampler.inl
	include/PolyVoxCore/Raycast.h
	include/PolyVoxCore/Raycast.inl
	include/PolyVoxCore/Region.h
	include/PolyVoxCore/SimpleInterface.h
	include/PolyVoxCore/SimpleVolume.h
	include/PolyVoxCore/SimpleVolume.inl
	include/PolyVoxCore/SimpleVolumeBlock.inl
	include/PolyVoxCore/SimpleVolumeSampler.inl
	include/PolyVoxCore/SurfaceMesh.h
	include/PolyVoxCore/SurfaceMesh.inl
	include/PolyVoxCore/Vector.h
	include/PolyVoxCore/Vector.inl
	include/PolyVoxCore/VertexTypes.h
	include/PolyVoxCore/VolumeResampler.h
	include/PolyVoxCore/VolumeResampler.inl
	include/PolyVoxCore/VoxelFilters.h
	include/PolyVoxCore/VoxelFilters.inl
)

SET(IMPL_SRC_FILES
	source/Impl/MarchingCubesTables.cpp
	source/Impl/RandomUnitVectors.cpp
	source/Impl/RandomVectors.cpp
	source/Impl/Utility.cpp
)

SET(IMPL_INC_FILES
	include/PolyVoxCore/Impl/ArraySizesImpl.h
	include/PolyVoxCore/Impl/ArraySizesImpl.inl
	include/PolyVoxCore/Impl/AStarPathfinderImpl.h
	include/PolyVoxCore/Impl/Block.h
	include/PolyVoxCore/Impl/Block.inl
	include/PolyVoxCore/Impl/MarchingCubesTables.h
	include/PolyVoxCore/Impl/RandomUnitVectors.h
	include/PolyVoxCore/Impl/RandomVectors.h
	include/PolyVoxCore/Impl/SubArray.h
	include/PolyVoxCore/Impl/SubArray.inl
	include/PolyVoxCore/Impl/TypeDef.h
	include/PolyVoxCore/Impl/Utility.h
)

#NOTE: The following line should be uncommented when building shared libs.

#"Sources" and "Headers" are the group names in Visual Studio.
#They may have other uses too...
SOURCE_GROUP("Sources" FILES ${CORE_SRC_FILES})
SOURCE_GROUP("Headers" FILES ${CORE_INC_FILES})

SOURCE_GROUP("Sources\\Impl" FILES ${IMPL_SRC_FILES})
SOURCE_GROUP("Headers\\Impl" FILES ${IMPL_INC_FILES})

#Tell CMake the paths
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/include)

#Core
#Build
IF(LIBRARY_TYPE STREQUAL "STATIC")
	ADD_LIBRARY(PolyVoxCore STATIC ${CORE_SRC_FILES} ${CORE_INC_FILES} ${IMPL_SRC_FILES} ${IMPL_INC_FILES})
	IF(UNIX)
		SET_TARGET_PROPERTIES(PolyVoxCore PROPERTIES COMPILE_FLAGS -fPIC)
	ENDIF()
ENDIF()
IF(LIBRARY_TYPE STREQUAL "DYNAMIC")
	ADD_LIBRARY(PolyVoxCore SHARED ${CORE_SRC_FILES} ${CORE_INC_FILES} ${IMPL_SRC_FILES} ${IMPL_INC_FILES})
	SET_TARGET_PROPERTIES(PolyVoxCore PROPERTIES COMPILE_FLAGS "-DPOLYVOX_SHARED_EXPORTS")
ENDIF()
SET_PROPERTY(TARGET PolyVoxCore PROPERTY FOLDER "Library")

SET_TARGET_PROPERTIES(PolyVoxCore PROPERTIES VERSION ${POLYVOX_VERSION} SOVERSION ${POLYVOX_VERSION_MAJOR})
IF(MSVC)
		SET_TARGET_PROPERTIES(PolyVoxCore PROPERTIES COMPILE_FLAGS "/W4 /wd4251 /wd4127") #Disable warning on STL exports
ENDIF(MSVC)

