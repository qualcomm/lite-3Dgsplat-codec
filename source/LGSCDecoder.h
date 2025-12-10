/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#ifndef LGSCDECODER_H
#define LGSCDECODER_H

#include "LGSCMisc.h"

int read_header(GS& gs, uint8_t* bPtr, CoderParams& coderParams);

void read_attributes(GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, const std::vector<Quantizer>& attrQuantizer);

void read_positions(GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, const std::vector<Quantizer>& posQuantizer);

int readBitstream(char*& compBuf, std::vector<uint8_t>& decompressed, const bool useGzip);

void decode(GS& gs, CoderParams& coderParams, std::vector<char> &compressed);

void decompressFrame(std::vector<char>& compressed, GS& gs);
void decompressFrame(std::vector<char>& compressed, int &numGaussians, 
  std::vector<float>& positions,
  std::vector<float>& rotations,
  std::vector<float>& scales,
  std::vector<float>& opacities,
  std::vector<float>& features_diffuse,
  std::vector<float>& features_specular);
void decompressFrame(std::vector<char>& compressed, int& numGaussians,
  float** positions,
  float** rotations,
  float** scales,
  float** opacities,
  float** features_diffuse,
  float** features_specular);
#endif