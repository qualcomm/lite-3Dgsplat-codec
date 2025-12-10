/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#include "LGSCCommon.h"
#include "LGSCEncoder.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include "dependencies/zlib-1.3.1/zlib.h"

using namespace std;

auto write4Byte = [](uint8_t* b, const uint32_t v, const int N) {
  *b = v & 0xFF;
  *(b + 1) = (v >> 8) & 0xFF;
  *(b + 2) = (v >> 16) & 0xFF;
  *(b + 3) = (v >> 24) & 0xFF;
};
auto writeNByte = [](uint8_t* b, const uint32_t v, const int N) {
  switch (N) {
  case 1:
    *b = v & 0xFF;
    break;
  case 2:
    *b = v & 0xFF;
    *(b + 1) = (v >> 8) & 0xFF;
    break;
  case 3:
    *b = v & 0xFF;
    *(b + 1) = (v >> 8) & 0xFF;
    *(b + 2) = (v >> 16) & 0xFF;
    break;
  case 4:
    std::cerr << "To be checked whether data types in the code can support four byte attributes\n";
    std::exit(1);
    *b = v & 0xFF;
    *(b + 1) = (v >> 8) & 0xFF;
    *(b + 2) = (v >> 16) & 0xFF;
    *(b + 3) = (v >> 24) & 0xFF;
    break;
  default:
    std::cerr << "unsupported " << N << " bytes per position\n";
    std::exit(1);
  }
};

auto writeFloat = [](float val, uint8_t* b) {
  std::memcpy(b, &val, sizeof(float));
};
bool compressGzipped(const uint8_t* data, size_t size, std::vector<uint8_t>* out) {
  std::vector<uint8_t> buffer(8192);
  z_stream stream = {};
  if (
    deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 16 + MAX_WBITS, 9, Z_DEFAULT_STRATEGY)
    != Z_OK) {
    return false;
  }
  out->clear();
  out->reserve(size / 4);
  stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data));
  stream.avail_in = static_cast<uInt>(size);
  bool success = false;
  while (true) {
    stream.next_out = buffer.data();
    stream.avail_out = static_cast<uInt>(buffer.size());
    int res = deflate(&stream, Z_FINISH);
    if (res != Z_OK && res != Z_STREAM_END) {
      break;
    }
    out->insert(out->end(), buffer.data(), buffer.data() + buffer.size() - stream.avail_out);
    if (res == Z_STREAM_END) {
      success = true;
      break;
    }
  }
  deflateEnd(&stream);
  return success;
}
int write_header(GS& gs, uint8_t* bPtr, const CoderParams& coderParams)
{ 
  const auto gaussianCnt = gs.getGaussianCount();
  auto attrStats = gs.getAttributeStats();
  
  int ctr = 0;
  const uint32_t val = static_cast<int>(gaussianCnt);
  write4Byte(bPtr, val, sizeof(uint32_t));
  bPtr += 4;
  ctr += 4;

  auto dataParams = coderParams.getHeaderParams();

  std::memcpy(bPtr, dataParams.data(), dataParams.size());
  bPtr += dataParams.size();
  ctr += static_cast<int>(dataParams.size());

  for (const auto& attr : attrStats.attributes) {
    for (int dim = 0; dim < attr.numDims; ++dim) {
        const std::string tag = attr.tags[dim];
        writeFloat(static_cast<float>(attrStats.minVals.at(tag)), bPtr);
        writeFloat(static_cast<float>(attrStats.maxVals.at(tag)), bPtr + 4);
        bPtr += 8;
        ctr += 8;
    }
  }
  return ctr;
}

void writeBitstream(std::ostringstream& fout, std::vector<uint8_t>& buf, int nBytes, const bool useGzip)
{
  if (useGzip) {
    std::vector<uint8_t> bufZip;
    compressGzipped(buf.data(), nBytes, &bufZip);
    int sz = static_cast<int>(bufZip.size());
    fout.write(reinterpret_cast<char*>(&sz), sizeof(int));
    fout.write(reinterpret_cast<char*>(bufZip.data()), sz);
  }
  else
  {
    fout.write(reinterpret_cast<char*>(&nBytes), sizeof(int));
    fout.write(reinterpret_cast<char*>(buf.data()), nBytes);
  }
}

void write_positions(const GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, 
                     const std::vector<Quantizer>& posQuantizer, std::vector<MortonCodeWithIndex>& packedVoxel) {
  const auto gaussianCnt = gs.getGaussianCount();
  const int bytePerPos = (coderParams.numBits.geom + 7) >> 3;
  auto& q = posQuantizer;

  if (coderParams.SORT_INPUT_POINTS)
    packedVoxel.resize(gaussianCnt);

  std::vector<std::array<int32_t, 3>> quantizedPos;
  quantizedPos.resize(gaussianCnt);
  int bd = coderParams.numBits.getNumBits(AttrType::POS);
  for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
    auto& pos = quantizedPos[cnt];
    for (auto i = 0; i < 3; i++){
      pos[i] = q[i].quantize(gs.positions[cnt][i]);}
    if (coderParams.SORT_INPUT_POINTS)
    {
      packedVoxel[cnt].mortonCode = mortonAddr(pos[0], pos[1], pos[2], bd);
      packedVoxel[cnt].index = cnt;
    }
  }

  // Sort Gaussians
  if (coderParams.SORT_INPUT_POINTS)
    sort(packedVoxel.begin(), packedVoxel.end());

  // Encode Gaussians
  if (coderParams.INTERLEAVE_POS_DIMS){
    for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
      const auto idx = coderParams.SORT_INPUT_POINTS ? packedVoxel[cnt].index : cnt;
      for (auto i = 0; i < 3; i++, bPtr += bytePerPos, ctr += bytePerPos){
        writeNByte(bPtr, quantizedPos[idx][i], bytePerPos);
      }
    }
  }
  else{
    for (auto i = 0; i < 3; i++){
      for (auto cnt = 0; cnt < gaussianCnt; cnt++, bPtr += bytePerPos, ctr += bytePerPos) {
        const auto idx = coderParams.SORT_INPUT_POINTS ? packedVoxel[cnt].index : cnt;
        writeNByte(bPtr, quantizedPos[idx][i], bytePerPos);
      }
    }
  }
}


void write_attributes(const GS& gs, uint8_t* bPtr, int& ctr, const CoderParams& coderParams, 
                      const std::vector<Quantizer>& attrQuantizer, std::vector<MortonCodeWithIndex>& packedVoxel)
{
  const auto gaussianCnt = gs.getGaussianCount();
  auto attrStats = gs.getAttributeStats();
    
  int qId = 0;
  if (coderParams.INTERLEAVE_ATTR_DIMS) {
    for (const auto& attr : attrStats.attributes) {
       const auto attrName = attr.name;
       if (attrName!=AttrType::POS){
          int bd = coderParams.numBits.getNumBits(attrName);
          int bytePerPos = (bd + 7) >> 3;
          auto numDims = attr.numDims;
          for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
            const auto idx = coderParams.SORT_INPUT_POINTS ? packedVoxel[cnt].index : cnt;
            for (auto dim = 0; dim < numDims; dim++, bPtr += bytePerPos, ctr += bytePerPos) {
               if (attrName == AttrType::SPH_REST) {
                  int ord = dim < 9 ? 1 : dim < 24 ? 2 : 3;
                  bd = coderParams.numBits.getNumBits(attrName, ord);
                  bytePerPos = (bd + 7) >> 3;
               }
               const std::string tag = attr.tags[dim];
               auto q = attrQuantizer[qId + dim];
               float valF = gs.getAttr(attrName, idx, dim);
               uint32_t val = q.quantize(valF);
               if (coderParams.SKIP_ROT_DIM && attrName == AttrType::ROT4 && dim == 3) val = (uint32_t)valF;
               writeNByte(bPtr, val, bytePerPos);
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
                const std::string tag = attr.tags[dim];
                auto q = attrQuantizer[qId + dim];
                for (auto cnt = 0; cnt < gaussianCnt; cnt++, bPtr += bytePerPos, ctr += bytePerPos) {
                    const auto idx = coderParams.SORT_INPUT_POINTS ? packedVoxel[cnt].index : cnt;
                    float valF = gs.getAttr(attrName, idx, dim);
                    uint32_t val = q.quantize(valF);
                    if (coderParams.SKIP_ROT_DIM && attrName == AttrType::ROT4 && dim == 3) val = (uint32_t)valF;
                    writeNByte(bPtr, val, bytePerPos);
                }
            }
            qId += attr.numDims;
        }
     }
  }
}

int getMaxHdrSize(const CoderParams& coderParams, int gaussianCnt) {
  int numHdrBytes = 0;
  numHdrBytes += 4;  // gaussianCnt
  numHdrBytes += 32; // other parameters - upper bound
  numHdrBytes += 2 * sizeof(float) * 59;  // min/max values
  return numHdrBytes;
}
int getMaxGeomDataSize(const CoderParams& coderParams, int gaussianCnt) {
  int bpp = (coderParams.numBits.geom + 7) >> 3;
  int bytesPerPoint = bpp * 3;
  return bytesPerPoint * gaussianCnt;
}
int getMaxAttrDataSize(const CoderParams& coderParams, int gaussianCnt) {
   int bpa = (coderParams.numBits.getMax() + 7) >> 3;
   int bytesPerPoint = bpa * (3 + 4 + 1 + 3 + 45);
   return bytesPerPoint * gaussianCnt;
}
void encode(GS& gs, CoderParams& coderParams, std::ostringstream& bitstreamFileName)
{
  const auto gaussianCnt = gs.getGaussianCount();
  const int maxHdrSize = getMaxHdrSize(coderParams, static_cast<int>(gaussianCnt));
  const int maxGeomDataSize = getMaxGeomDataSize(coderParams, static_cast<int>(gaussianCnt));
  const int maxAttrDataSize = getMaxAttrDataSize(coderParams, static_cast<int>(gaussianCnt));
  
  std::vector<uint8_t> buf(maxHdrSize);
  auto bPtr = buf.data();
  int ctr = write_header(gs, bPtr, coderParams);
  writeBitstream(bitstreamFileName, buf, ctr, false);
  std::vector<Quantizer> posQuantizer;
  std::vector<Quantizer> attrQuantizer;
  initializeQuantizers(posQuantizer, attrQuantizer, coderParams, gs);

  std::vector<MortonCodeWithIndex> packedVoxel = {};
  ctr = 0;
  buf.resize(maxGeomDataSize + maxAttrDataSize);
  bPtr = buf.data();
  write_positions(gs, bPtr, ctr, coderParams, posQuantizer, packedVoxel);
  if (coderParams.SEPARATE_GEOM_ATTR_CODING) {
    writeBitstream(bitstreamFileName, buf, ctr, coderParams.USE_GZIP);
    bPtr = buf.data();
    std::fill(buf.begin(), buf.end(), 128);
    ctr = 0;
  }
  else {
    bPtr = bPtr + ctr;
  }

  write_attributes(gs, bPtr, ctr, coderParams, attrQuantizer, packedVoxel);
  writeBitstream(bitstreamFileName, buf, ctr, coderParams.USE_GZIP);
}

void update_stats(GS& gs, CoderParams& coderParams)
{
  if (0)
    return;
  
  auto& attrStats = gs.getAttributeStats();

  if (coderParams.APPLY_SIGMOID_OPACITY) {
    int attrIdx = attrStats.getAttrIdx(AttrType::OPACITY);
    if (attrIdx != -1) {
        const auto& tag = attrStats.attributes[attrIdx].tags[0];
        float minVal = static_cast<float>(attrStats.minVals.at(tag));
        minVal = 1 / (1 + std::exp(-minVal));
        attrStats.minVals.at(tag) = minVal;
        float maxVal = static_cast<float>(attrStats.maxVals.at(tag));
        maxVal = 1 / (1 + std::exp(-maxVal));
        attrStats.maxVals.at(tag) = maxVal;
    }
  }

  if (coderParams.SKIP_ROT_DIM){
    int attrIdx = attrStats.getAttrIdx(AttrType::ROT4);
    if (attrIdx != -1) {
        const auto& tags = attrStats.attributes[attrIdx].tags;
        for (const auto& tag : tags) {
            attrStats.minVals.at(tag) = -1.;
            attrStats.maxVals.at(tag) = 1.;
        }
    }
  }
  if (coderParams.SET_SPH_MIN_MAX_TO_1) {
    int attrIdx = attrStats.getAttrIdx(AttrType::SPH_REST);
    if (attrIdx != -1) {
        const auto& tags = attrStats.attributes[attrIdx].tags;
        for (const auto& tag : tags) {
            attrStats.minVals.at(tag) = -coderParams.SET_SPH_MIN_MAX_TO_1;
            attrStats.maxVals.at(tag) = coderParams.SET_SPH_MIN_MAX_TO_1;
        }
    }
  }

  if (coderParams.CLAMP_SCALE_VALUES) {
    int attrIdx = attrStats.getAttrIdx(AttrType::SCALE);
    if (attrIdx != -1) {
        const auto& tags = attrStats.attributes[attrIdx].tags;
        for (const auto& tag : tags) {
            double& minVal = attrStats.minVals.at(tag);
            double& maxVal = attrStats.maxVals.at(tag);
            
            minVal = (minVal < coderParams.CLAMP_SCALE_MIN) ? coderParams.CLAMP_SCALE_MIN : minVal;
            maxVal = (maxVal > coderParams.CLAMP_SCALE_MAX) ? coderParams.CLAMP_SCALE_MAX : maxVal;
        }
    }
  }
}

void compressFrame(GS gs, std::vector<char>& compressed, int compLevel)
{
  CoderParams coderParams(compLevel);
  std::ostringstream sout;
  
  processGaussian(gs, coderParams, true);

  /* Update stats (min/max) and other coder params */
  gs.deriveStats();
  update_stats(gs, coderParams);

  /* Compress */
  encode(gs, coderParams, sout);

  compressed.resize(sout.str().size());
  std::memcpy(compressed.data(), sout.str().data(), sout.str().size());
}


void compressFrame(int numGaussians, std::vector<float>& positions,
  std::vector<float>& rotations,
  std::vector<float>& scales,
  std::vector<float>& opacities,
  std::vector<float>& features_diffuse,
  std::vector<float>& features_specular, int shDegree, int compLevel, std::vector<char>& compressed)
{
  GS gs;
  gs.parseAttributes(numGaussians, positions, rotations, scales, opacities, features_diffuse, features_specular, shDegree);
  
  CoderParams coderParams(compLevel);
  std::ostringstream sout;
  
  processGaussian(gs, coderParams, true);

  /* Update stats (min/max) and other coder params */
  gs.deriveStats();
  update_stats(gs, coderParams);
  
  /* Compress */
  encode(gs, coderParams, sout);

  compressed.resize(sout.str().size());
  std::memcpy(compressed.data(), sout.str().data(), sout.str().size());
}

void compressFrame(int numGaussians, const float* positions,
  const float* rotations,
  const float* scales,
  const float* opacities,
  const float* features_diffuse,
  const float* features_specular, int shDegree, int compLevel, std::vector<char>& compressed)
{
  GS gs;
  gs.parseAttributes(numGaussians, positions, rotations, scales, opacities, features_diffuse, features_specular, shDegree);
  
  CoderParams coderParams(compLevel);
  std::ostringstream sout;
  
  processGaussian(gs, coderParams, true);
  
  /* Update stats (min/max) and other coder params */
  gs.deriveStats();
  update_stats(gs, coderParams);

  /* Compress */
  encode(gs, coderParams, sout);

  compressed.resize(sout.str().size());
  std::memcpy(compressed.data(), sout.str().data(), sout.str().size());
}