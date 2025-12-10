/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "parameters.h"
#include "lgsc_chrono.h"
#include "LGSCCommon.h"
#include "LGSCEncoder.h"
#include "LGSCDecoder.h"
#include "dependencies/zlib-1.3.1/zlib.h"

using namespace std;

//============================================================================

int main(int argc, char* argv[])
{
  cout << "L-GSC version 1.0" << endl;

  Parameters params;

  try {
    if (!ParseParameters(argc, argv, params))
      return 1;
  }
  catch (const std::exception& e) {
    std::cerr << "Error parsing option \"" << e.what() << std::endl;
    return 1;
  }

  // Timers to count elapsed wall/user time
  lgsc::chrono::Stopwatch<std::chrono::steady_clock> clock_wall;

  int ret = 0;
  
  if (!params.isDecoder) {
    // Encoder
    GS gs;
    readFile(params.uncompressedDataPath, gs);
    std::vector<float> positions, rotations, scales, opacities, features_diffuse, features_specular;
    int numGaussians = 0, shDegree = params.sphericalOrder;
    gs.populateGSAttributes(numGaussians, positions, rotations, scales, opacities, features_diffuse, features_specular, shDegree);
    
    clock_wall.start();
    std::vector<char> compressed;
    // ----------------------------------------------------------
    /* API - example 1 (encoder) */
    // ----------------------------------------------------------
    // Pass the attributes as individual vectors and the compressed 
    // stream is returned as vector<char> which can be used for 
    // further processing
    compressFrame(numGaussians, positions.data(),
      rotations.data(),
      scales.data(),
      opacities.data(),
      features_diffuse.data(),
      features_specular.data(), shDegree, params.compLevel, compressed);
    /* End of API - example 1 (encoder) */
    // ----------------------------------------------------------
    clock_wall.stop();

    std::ofstream fout(params.compressedStreamPath, std::ofstream::out | std::ofstream::binary);
    fout.write(compressed.data(), compressed.size());
    fout.close();
  }
  else {
    // Decoder
    DecoderParams decoderParams;
    setDecoderParmas(decoderParams, params);

    std::vector<char> compressed;
    std::ifstream fin(decoderParams.compressedStreamPath, std::ifstream::in | std::ifstream::binary);
    fin.seekg(0, std::ios::end);
    std::streamsize size = fin.tellg();
    fin.seekg(0, std::ios::beg);
    compressed.resize(size);
    fin.read(compressed.data(), size);
    fin.close();

    float* positions, *rotations, *scales, *opacities, *features_diffuse, *features_specular;
    int numGaussians;

    clock_wall.start();
    // ----------------------------------------------------------
    /* API - example 1 (decoder) */
    // ----------------------------------------------------------
    decompressFrame(compressed, numGaussians, &positions, &rotations, &scales, &opacities, &features_diffuse, &features_specular);
    /* End of API - example 1 (decoder) */
    // ----------------------------------------------------------
    clock_wall.stop();

    GS gs;
    gs.parseAttributes(numGaussians, positions, rotations, scales, opacities, features_diffuse, features_specular, 3);
    delete[] positions, rotations, scales, opacities, features_diffuse, features_specular;
    
    if (decoderParams.outputBinaryPly){
        writeFile(decoderParams.reconstructedDataPath, gs);
    }else{
        writeFile(decoderParams.reconstructedDataPath, gs, PlyFormat::Ascii);
    }
  }

  using namespace std::chrono;
  auto total_wall = duration_cast<microseconds>(clock_wall.count()).count();
  auto old_precision = std::cout.precision(6);
  std::cout << std::fixed;
  std::cout << "Processing time (wall): " << total_wall / 1000000.0 << " s\n";
  std::cout << std::defaultfloat << std::setprecision(old_precision);

  return ret;
}
