/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#ifndef LGSCENCODER_H
#define LGSCENCODER_H

#include "LGSCMisc.h"
#include "LGSCCommon.h"

int write_header(GS& gs, uint8_t* bPtr, const CoderParams& coderParams);

void writeBitstream(std::ostringstream& fout, std::vector<uint8_t>& buf, int nBytes, const bool useGzip);

void write_positions(const GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, 
                     const std::vector<Quantizer>& posQuantizer, std::vector<MortonCodeWithIndex>& packedVoxel);

void write_attributes(const GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, 
                      const std::vector<Quantizer>& attrQuantizer, std::vector<MortonCodeWithIndex>& packedVoxel);

void encode(GS& gs, CoderParams& coderParams, std::ostringstream& bitstreamFileName);

void compressFrame(GS gs, std::vector<char>& compressed, int compLevel);
void compressFrame(int numGaussians, std::vector<float>& positions,
  std::vector<float>& rotations,
  std::vector<float>& scales,
  std::vector<float>& opacities,
  std::vector<float>& features_diffuse,
  std::vector<float>& features_specular, int shDegree, int compLevel, std::vector<char>& compressed);
void compressFrame(int numGaussians, const  float* positions,
  const float* rotations,
  const float* scales,
  const float* opacities,
  const float* features_diffuse,
  const float* features_specular, int shDegree, int compLevel, std::vector<char>& compressed);
#endif