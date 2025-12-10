# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.  
# SPDX-License-Identifier: Apache-2.0

# L-GSC v1.0
Library for coding Gaussian Splat representations


This code may be used to compress Gaussian Splat (GS) representations. Currently, 
the INRIA format is supported as input for the GS representation.

The lgsc library may be built with the project to access the APIs. A sample implementation
of the API (compression and decompression) is available.

## To obtain the code:

`git clone https://github.com/qualcomm/lite-3Dgsplat-codec.git`

The dependencies are already included and will be built as part of the project.


## 🛠️ Build Instructions (Windows)
📋 Requirements: CMake

	mkdir build
	cd build
	cmake .. -G "Visual Studio 16 2019"
	# OR
	cmake .. -G "Visual Studio 17 2022"
	cd ..

### To compile lgsc (Windows)

1. Open the lgsc project solution in Visual Studio
2. Compile using Debug/Release mode
3. The code now uses static library only for dependence (zlib).

## 🛠 To build and compile the project in Linux:
    #!/bin/bash
    export CC=/gnu/gcc/13.2.0/bin/gcc
    export CXX=/gnu/gcc/13.2.0/bin/g++

    rm -rf build/

    mkdir build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Clean
    cmake .. -DCMAKE_BUILD_TYPE=Release

    make -j 7
    cd ..

## Run the sample implementation that uses the API

In test\ folder, sample config files encoder-inria.cfg and decoder-inria.cfg are available to test. Use the following commands after
updating the path to a Gaussian Splat PLY.

    cd test
    ..\build\source\Release\lgsc-api-example1.exe -c encoder-inria.cfg 	# Run encoder
    ..\build\source\Release\lgsc-api-example1.exe -c decoder-inria.cfg	# Run decoder

The compression level compLevel (0-2) may be specified in the encoder config file.

The encoder will output a compressed binary file and the decoder will output the decoded PLY file. 

By default, the geometry and attributes are coded using zlib.

# API

The library provides API for compression and decompression.

## API for compression

	void compressFrame(int numGausians, 
			const float* positions,
			const float* rotations,
			const float* scales,
			const float* opacties,
			const float* features_diffuse,
			const float* features_specular, 
			int shDegree, int compLevel, std::vector<char>& compressed)

### 📥 API Inputs
The inputs to the API are as follows:
- numGausians - number of Gaussians that are to be compressed (say N)
- positions - vector of positions stored in linear array (0, 0) (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (2, 0) ... (size Nx3)
- rotations - vector of rotations stored in linear array (0, 0) (0, 1), (0, 2), (0, 3), (1, 0), (1, 1), (1, 2) ... (size Nx4)
- scales    - vector of scales    stored in linear array (0, 0) (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (2, 0) ... (size Nx3)
- opacities - vector of opacities stored in linear array (0, 0) (1, 0), (2, 0) ... (size Nx1)
- features_diffuse    - vector of RGB DC stored in linear array (0, 0) (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (2, 0) ... (size Nx3)
- features_specular   - vector of RGB spherical harmonics higher order
                        coefficients stored in array of (N, 3, F) that is linearized
                        F = (shDegree + 1) * (shDegree + 1) - 1
- shDegree            - Order of spherical harmonics stored in features_specular
- compLevel           - compression levels 0/1/2
                        0 indicates less compression, 1 indicates more compression, 2 is highest compression

### 📤 API Output
The output of the API is a vector of bytes compressed.


## API for decompression

	void decompressFrame(std::vector<char>& compressed, int &numGaussians, 
			float** positions,
			float** rotations,
			float** scales,
			float** opacties,
			float** features_diffuse,
			float** features_specular)

### 📥 API Input
The input to the API is a vector of compressed bytes (that was compressed by the compressFrame() API.)

### 📤 API Output
The outputs of the API are as follows:
- numGausians - number of Gaussians that are decompressed (say N)
- positions - vector of positions stored in linear array (0, 0) (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (2, 0) ... (size Nx3)
- rotations - vector of rotations stored in linear array (0, 0) (0, 1), (0, 2), (0, 3), (1, 0), (1, 1), (1, 2) ... (size Nx4)
- scales    - vector of scales    stored in linear array (0, 0) (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (2, 0) ... (size Nx3)
- opacities - vector of opacities stored in linear array (0, 0) (1, 0), (2, 0) ... (size Nx1)
- features_diffuse    - vector of RGB DC stored in linear array (0, 0) (0, 1), (0, 2), (1, 0), (1, 1), (1, 2), (2, 0) ... (size Nx3)
- features_specular   - vector of RGB spherical harmonics higher order
                        coefficients stored in array of (N, 3, 15) that is linearized
                        If original point cloud had fewer than order 3 coefficients, the unavailable coefficients are filled with 0s.
