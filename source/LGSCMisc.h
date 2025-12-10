/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include <vector>
#include <iostream>
#include <climits>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <algorithm>

#include "attributes.h"

struct Quantizer {
  double minVal;
  double maxVal;
  uint32_t maxSymbol;
  int bd;
  int maxPossibleVal;
  Quantizer(double m, double M, uint32_t sym, int bitdepth) : minVal(m), maxVal(M), maxSymbol(sym), bd(bitdepth) {
    maxPossibleVal = std::min(static_cast<int>(maxSymbol), (1 << bd) - 1);
  }
  inline uint32_t quantize(const float valF) const {
    int x = static_cast<int>(std::round((valF - minVal) / (maxVal - minVal) * maxSymbol));
    x = (x < 0) ? 0 : (x > maxPossibleVal ? maxPossibleVal : x);
    return x;
  }
  inline float invQuantize(uint32_t val) const
  {
    return static_cast<float>(minVal + val * (maxVal - minVal) / maxSymbol);
  }
};
struct NumBits {
  int geom = 16;
  int scale = 8;
  int opac = 8;
  int rot = 8;
  int sh0 = 8;
  int sh1 = 8;
  int sh2 = 8;
  int sh3 = 8;
  int getNumBits(AttrType attrName, int order = 0) const {
    switch (attrName) {
        case AttrType::POS:
            return geom;
        case AttrType::SCALE:
            return scale;
        case AttrType::OPACITY:
            return opac;
        case AttrType::ROT4:
            return rot;
        case AttrType::SPH_DC:
            return sh0;
        case AttrType::SPH_REST:
            switch (order) {
                case 0: return sh0;
                case 1: return sh1;
                case 2: return sh2;
                case 3: return sh3;
                default:
                    std::cerr << "Incorrect SH order: " << order << "\n";
                    std::exit(EXIT_FAILURE);
            }
        default:
            std::cerr << "Unknown attribute type\n";
            std::exit(EXIT_FAILURE);
    }
  }

  int getMax() const {
    int m = -INT_MAX;
    m = std::max(m, scale);
    m = std::max(m, opac);
    m = std::max(m, rot);
    m = std::max(m, sh0);
    m = std::max(m, sh1);
    m = std::max(m, sh2);
    m = std::max(m, sh3);
    return m;
  }
  void setAttr(const int n) {
    scale = opac = rot = sh0 = sh1 = sh2 = sh3 = n;
  }
};
struct DecoderParams {
  std::string reconstructedDataPath;
  std::string compressedStreamPath;
  bool outputBinaryPly;
};
struct CoderParams {
  NumBits numBits;
  bool USE_GZIP;
  bool SKIP_ROT_DIM;
  bool APPLY_SIGMOID_OPACITY;
  bool REARRANGE_SPHREST;
  bool INTERLEAVE_POS_DIMS;
  bool INTERLEAVE_ATTR_DIMS;
  int SET_SPH_MIN_MAX_TO_1;
  bool YUV_CODING;
  std::string uncompressedDataPath;
  std::string compressedStreamPath;

  bool SEPARATE_GEOM_ATTR_CODING = true;
  int SORT_INPUT_POINTS;
  bool RESERVED_BIT1 = false;
  bool RESERVED_BIT2 = false;
  std::string cfgFileName;
  int sphericalOrder;
  
  bool CLAMP_SCALE_VALUES = true;
  double CLAMP_SCALE_MIN = -30;
  double CLAMP_SCALE_MAX = 30;

  CoderParams() { init(); }
  void init() {
    
    USE_GZIP = true;
    SKIP_ROT_DIM = true;
    APPLY_SIGMOID_OPACITY = false;
    REARRANGE_SPHREST = true;
    INTERLEAVE_POS_DIMS = false;
    INTERLEAVE_ATTR_DIMS = true;
    SET_SPH_MIN_MAX_TO_1 = 1;
    YUV_CODING = true;
    if (YUV_CODING)
      REARRANGE_SPHREST = false;

    SORT_INPUT_POINTS = 1;
    sphericalOrder = 3;
  }
  CoderParams(int compLevel) { init(compLevel); }
  void init(int compLevel) {
    init();
    switch (compLevel) {
    case 0: numBits = { 18, 12, 12, 12, 12, 8, 8, 8 }; break;
    case 1: numBits = { 16, 8, 8, 8, 8, 6, 6, 6 }; break;
    case 2: numBits = { 16, 8, 8, 8, 8, 5, 5, 5 }; break;
    default:
      std::cerr << "Unsupported compression level\n";
      std::exit(1);
    }
  }

  std::vector<uint8_t> getHeaderParams() const {
    std::vector<uint8_t> data;
    data.push_back(numBits.geom);
    data.push_back(numBits.scale);
    data.push_back(numBits.rot);
    data.push_back(numBits.opac);
    data.push_back(numBits.sh0);
    data.push_back(numBits.sh1);
    data.push_back(numBits.sh2);
    data.push_back(numBits.sh3);
    data.push_back(0);
    data.back() |= (RESERVED_BIT1 ? 1 : 0) << 7;
    data.back() |= (SKIP_ROT_DIM ? 1 : 0) << 6;
    data.back() |= (APPLY_SIGMOID_OPACITY ? 1 : 0) << 5;
    data.back() |= (REARRANGE_SPHREST ? 1 : 0) << 4;
    data.back() |= (INTERLEAVE_POS_DIMS ? 1 : 0) << 3;
    data.back() |= (INTERLEAVE_ATTR_DIMS ? 1 : 0) << 2;
    data.back() |= (YUV_CODING ? 1 : 0) << 1;
    data.back() |= (RESERVED_BIT2 ? 1 : 0);
    data.push_back(SET_SPH_MIN_MAX_TO_1);

    int sz = static_cast<int>(data.size());

    data.insert(data.begin(), sz);
    return data;
  }
  void setHeaderParams(const std::vector<uint8_t> data) {
    int ctr = 0;
    numBits.geom = data[ctr++];
    numBits.scale = data[ctr++];
    numBits.rot = data[ctr++];
    numBits.opac = data[ctr++];
    numBits.sh0 = data[ctr++];
    numBits.sh1 = data[ctr++];
    numBits.sh2 = data[ctr++];
    numBits.sh3 = data[ctr++];

    RESERVED_BIT1 = (data[ctr] >> 7) & 0x1; 
    SKIP_ROT_DIM = (data[ctr] >> 6) & 0x1;
    APPLY_SIGMOID_OPACITY = (data[ctr] >> 5) & 0x1;
    REARRANGE_SPHREST = (data[ctr] >> 4) & 0x1;
    INTERLEAVE_POS_DIMS = (data[ctr] >> 3) & 0x1;
    INTERLEAVE_ATTR_DIMS = (data[ctr] >> 2) & 0x1;
    YUV_CODING = (data[ctr] >> 1) & 0x1;
    RESERVED_BIT2 = data[ctr++] & 0x1;
    SET_SPH_MIN_MAX_TO_1 = data[ctr++];
  }
};


// -----------------------------------------------------------------------------
// ************************
// *** Morton Coding ***
// ************************

struct MortonCodeWithIndex {
  int64_t mortonCode;

  // The position used to generate the mortonCode
  // Vec3<int32_t> position;
  int32_t index;

  bool operator<(const MortonCodeWithIndex& rhs) const
  {
    // NB: index used to maintain stable sort
    if (mortonCode == rhs.mortonCode)
      return index < rhs.index;
    return mortonCode < rhs.mortonCode;
  }
};

inline int64_t
mortonAddr(const int32_t x, const int32_t y, const int32_t z, int bitdepth)
{
  uint64_t code = 0;
  for (int i = 0; i < bitdepth; ++i) {
     code |= ((z >> i) & 1ULL) << (3 * i);
     code |= ((y >> i) & 1ULL) << (3 * i + 1);
     code |= ((x >> i) & 1ULL) << (3 * i + 2);
  }
  return code;
}

