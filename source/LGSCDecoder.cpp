/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#include "LGSCCommon.h"
#include "LGSCDecoder.h"
#include <iostream>
#include <sstream>
#include <cstring>

#include "dependencies/zlib-1.3.1/zlib.h"

using namespace std;
//----------------------------------------------------------------------------
// read_header()
//----------------------------------------------------------------------------
auto read4Byte = [](uint32_t* val, uint8_t* b, const int N) {
  *val = 0;
  uint8_t* v = reinterpret_cast<uint8_t*>(val);
  *v = *b;
  *(v + 1) = *(b + 1);
  *(v + 2) = *(b + 2);
  *(v + 3) = *(b + 3);
};
auto readNByte = [](uint32_t* val, uint8_t* b, const int N) {
  *val = 0;
  uint8_t* v = reinterpret_cast<uint8_t*>(val);
  switch (N) {
  case 1:
    *v = *b;
    break;
  case 2:
    *v = *b;
    *(v + 1) = *(b + 1);
    break;
  case 3:
    *v = *b;
    *(v + 1) = *(b + 1);
    *(v + 2) = *(b + 2);
    break;
  case 4:
    std::cerr << "To be checked whether data types in the code can support four byte attributes\n";
    std::exit(1);
    *v = *b;
    *(v + 1) = *(b + 1);
    *(v + 2) = *(b + 2);
    *(v + 3) = *(b + 3);
    break;
  default:
    std::cerr << "unsupported " << N << " bytes per position\n";
    std::exit(1);
  }
};
auto readFloat = [](uint8_t* b) {
  float val;
  std::memcpy(&val, b, sizeof(float));
  return val;
};

int read_header(GS& gs, uint8_t* bPtr, CoderParams& coderParams)
{
  uint32_t gsCnt = 0;
  int ctr = 0;
  read4Byte(&gsCnt, bPtr, sizeof(uint32_t));
  bPtr += 4;
  ctr += 4;

  // Update GS
  gs.resize(gsCnt);

  uint32_t sz;
  readNByte(&sz, bPtr, 1);
  std::vector<uint8_t> data;
  data.resize(sz);
  std::memcpy(data.data(), bPtr + 1, sz);
  coderParams.setHeaderParams(data);
  bPtr += (sz + 1);
  ctr += (sz + 1);
  // Update coderParams

  auto& attrStats = gs.getAttributeStats();
  attrStats.initINRIA();

  for (const auto& attr : attrStats.attributes) {
     for (int dim = 0; dim < attr.numDims; ++dim) {
        const std::string tag = attr.tags[dim];
        attrStats.minVals[tag] = readFloat(bPtr);
        attrStats.maxVals[tag] = readFloat(bPtr + 4);
        bPtr += 8;
        ctr += 8;
     }
  }
  
  return ctr;
}


void read_attributes(GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, const std::vector<Quantizer>& attrQuantizer){
  auto gaussianCnt = gs.getGaussianCount();
  auto attrStats = gs.getAttributeStats();
  uint32_t val;
  int qId = 0;
  if (coderParams.INTERLEAVE_ATTR_DIMS) {
    for (const auto& attr : attrStats.attributes) {
       const auto attrName = attr.name;
       if (attrName!=AttrType::POS){
          int bd = coderParams.numBits.getNumBits(attrName);
          int bytePerPos = (bd + 7) >> 3;
          auto numDims = attr.numDims;
          for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
            for (auto dim = 0; dim < numDims; dim++, bPtr += bytePerPos, ctr += bytePerPos) {
                if (attrName == AttrType::SPH_REST) {
                int ord = dim < 9 ? 1 : dim < 24 ? 2 : 3;
                bd = coderParams.numBits.getNumBits(attrName, ord);
                bytePerPos = (bd + 7) >> 3;
              }
              auto const& tag = attr.tags[dim];
              auto q = attrQuantizer[qId + dim];
              readNByte(&val, bPtr, bytePerPos);
              gs.getAttr(attrName, cnt, dim) = q.invQuantize(val);
              if (coderParams.SKIP_ROT_DIM && attrName == AttrType::ROT4 && dim == 3) gs.getAttr(attrName, cnt, dim) = (float)val;
            }
          }
          qId += attr.numDims;
       }
    }
  }
  else {
     for (const auto& attr : attrStats.attributes) {
        const auto attrName = attr.name;
        if (attrName!=AttrType::POS){
          int bd = coderParams.numBits.getNumBits(attrName);
          int bytePerPos = (bd + 7) >> 3;
          auto numDims = attr.numDims;
          for (auto dim = 0; dim < numDims; dim++) {
             if (attrName == AttrType::SPH_REST) {
                int ord = dim < 9 ? 1 : dim < 24 ? 2 : 3;
                bd = coderParams.numBits.getNumBits(attrName, ord);
                bytePerPos = (bd + 7) >> 3;
             }
             auto const& tag = attr.tags[dim];
             auto q = attrQuantizer[qId + dim];
             for (auto cnt = 0; cnt < gaussianCnt; cnt++, bPtr += bytePerPos, ctr += bytePerPos) {
                readNByte(&val, bPtr, bytePerPos);
                gs.getAttr(attrName, cnt, dim) = q.invQuantize(val);
                if (coderParams.SKIP_ROT_DIM && attrName == AttrType::ROT4 && dim == 3) gs.getAttr(attrName, cnt, dim) = (float)val;
             }
          }
          qId += attr.numDims;
        }
     }
  }
}

void read_positions(GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, const std::vector<Quantizer>& posQuantizer)
{
  auto gaussianCnt = gs.getGaussianCount();
  
  // Geometry 
  int bytePerPos = (coderParams.numBits.geom  + 7) >> 3;
  uint32_t val;
  if (coderParams.INTERLEAVE_POS_DIMS) {
    for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
      for (auto i = 0; i < 3; i++, bPtr += bytePerPos, ctr += bytePerPos) {
        readNByte(&val, bPtr, bytePerPos);
        gs.positions[cnt][i] = posQuantizer[i].invQuantize(val);
      }
    }
  }
  else {
    for (auto i = 0; i < 3; i++) {
      for (auto cnt = 0; cnt < gaussianCnt; cnt++, bPtr += bytePerPos, ctr += bytePerPos) {
        readNByte(&val, bPtr, bytePerPos);
        gs.positions[cnt][i] = posQuantizer[i].invQuantize(val);
      }
    }
  }
}
bool decompressGzippedImpl(
  const uint8_t* compressed, size_t size, int windowSize, std::vector<uint8_t>* out) {
  std::vector<uint8_t> buffer(8192);
  z_stream stream = {};
  stream.next_in = const_cast<Bytef*>(compressed);
  stream.avail_in = static_cast<int>(size);
  if (inflateInit2(&stream, windowSize) != Z_OK) {
    return false;
  }
  out->clear();
  bool success = false;
  while (true) {
    stream.next_out = buffer.data();
    stream.avail_out = static_cast<int>(buffer.size());
    int res = inflate(&stream, Z_NO_FLUSH);
    if (res != Z_OK && res != Z_STREAM_END) {
      break;
    }
    out->insert(out->end(), buffer.data(), buffer.data() + buffer.size() - stream.avail_out);
    if (res == Z_STREAM_END) {
      success = true;
      break;
    }
  }
  inflateEnd(&stream);
  return success;
}

bool decompressGzipped(const uint8_t* compressed, size_t size, std::vector<uint8_t>* out) {
  // Here 16 means enable automatic gzip header detection; consider switching this to 32 to enable
  // both automated gzip and zlib header detection.
  return decompressGzippedImpl(compressed, size, 16 | MAX_WBITS, out);
}
int readBitstream(char*& compBuf, std::vector<uint8_t>& decompressed, const bool useGzip)
{
  int sz;
  if (useGzip) {
    std::memcpy(reinterpret_cast<char*>(&sz), compBuf, sizeof(int));
    compBuf += sizeof(int);
    std::vector<uint8_t> data;
    data.resize(sz);
    std::memcpy(reinterpret_cast<char*>(data.data()), compBuf, sz);
    compBuf += sz;
    if (!decompressGzipped(data.data(), data.size(), &decompressed))
    {
      return 1;
    }
  }
  else {
    std::memcpy(reinterpret_cast<char*>(&sz), compBuf, sizeof(int));
    compBuf += sizeof(int);
    decompressed.resize(sz);
    std::memcpy(reinterpret_cast<char*>(decompressed.data()), compBuf, sz);
    compBuf += sz;
  }
  return 0;
}
void decode(GS& gs, CoderParams& coderParams, std::vector<char> &compressed)
{
  char* compBuf = compressed.data();

  std::vector<uint8_t> decompressed;
  if (readBitstream(compBuf, decompressed, false)) {
    std::cerr << "Unable to decompress\n";
    std::exit(1);
  }

  uint8_t* bPtr = decompressed.data();
  int ctr = read_header(gs, bPtr, coderParams);

  // Set up quantizers
  std::vector<Quantizer> posQuantizer;
  std::vector<Quantizer> attrQuantizer;
  initializeQuantizers(posQuantizer, attrQuantizer, coderParams, gs);

  ctr = 0;
  if (readBitstream(compBuf, decompressed, coderParams.USE_GZIP)) {
    std::cerr << "Unable to decompress\n";
    std::exit(1);
  }
  bPtr = decompressed.data();
  read_positions(gs, bPtr, ctr, coderParams, posQuantizer);
  
  if (coderParams.SEPARATE_GEOM_ATTR_CODING) {
    if (readBitstream(compBuf, decompressed, coderParams.USE_GZIP)) {
      std::cerr << "Unable to decompress\n";
      std::exit(1);
    }
    bPtr = decompressed.data();
  }
  else
    bPtr += ctr;
  
  read_attributes(gs, bPtr, ctr, coderParams, attrQuantizer);
}

void decompressFrame(std::vector<char>& compressed, GS& gs) {
    CoderParams coderParams;
    decode(gs, coderParams, compressed);

    processGaussian(gs, coderParams, false);
}
void decompressFrame(std::vector<char>& compressed, int &numGaussians, 
  std::vector<float>& positions,
  std::vector<float>& rotations,
  std::vector<float>& scales,
  std::vector<float>& opacities,
  std::vector<float>& features_diffuse,
  std::vector<float>& features_specular)
{
  CoderParams coderParams;
  GS gs;
  decode(gs, coderParams, compressed);

  processGaussian(gs, coderParams, false);

  gs.populateGSAttributes(numGaussians, positions, rotations, scales, opacities, features_diffuse, features_specular);
}
void decompressFrame(std::vector<char>& compressed, int& numGaussians,
  float** positions,
  float** rotations,
  float** scales,
  float** opacities,
  float** features_diffuse,
  float** features_specular)
{
  CoderParams coderParams;
  GS gs;
  decode(gs, coderParams, compressed);

  processGaussian(gs, coderParams, false);

  gs.populateGSAttributes(numGaussians, positions, rotations, scales, opacities, features_diffuse, features_specular);
}