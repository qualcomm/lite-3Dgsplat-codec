/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#include "LGSCCommon.h"
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

void initializeQuantizers(std::vector<Quantizer>& posQuantizer, std::vector<Quantizer>& attrQuantizer, const CoderParams& coderParams, const GS& gs){
  auto attrStats = gs.getAttributeStats();
  {
    const std::vector<std::string> s = { "x", "y", "z" };
    const int bytePerPos = (coderParams.numBits.geom + 7) >> 3;
    const int bd = coderParams.numBits.geom;
    const uint32_t maxSymbol = (1 << bd) - 1;
    double minVal, maxVal;
    for (auto i = 0; i < 3; i++) {
      minVal = attrStats.minVals.at(std::string(s[i]));
      maxVal = attrStats.maxVals.at(std::string(s[i]));
      posQuantizer.emplace_back(minVal, maxVal, maxSymbol, bd);
    }
  }
  {
    for (const auto& attr : attrStats.attributes) {
      const auto attrName = attr.name;
      if (attrName!=AttrType::POS){
        int bd = coderParams.numBits.getNumBits(attrName);
        for (int dim = 0; dim < attr.numDims; ++dim) {
        const std::string tag = attr.tags[dim];
        double minVal = attrStats.minVals.at(tag);
        double maxVal = attrStats.maxVals.at(tag);
        uint32_t maxSymbol = (1 << bd) - 1;
        if (attrName == AttrType::SPH_REST) {
            int ord = dim < 9 ? 1 : dim < 24 ? 2 : 3;
            bd = coderParams.numBits.getNumBits(attrName, ord);
            maxSymbol = 1 << bd;
        }
        attrQuantizer.emplace_back(minVal, maxVal, maxSymbol, bd);
        }
      }
    }
  }
}

void rearrangeSphericalHarmonics(GS& gs, const bool forward)
{ 
  const auto gaussianCnt = gs.getGaussianCount();
  for (size_t cnt = 0; cnt < gaussianCnt; cnt++){
    std::array<float, 45> temp = gs.shN[cnt];
    for (size_t i = 0; i < 15; i++){
      for (size_t j = 0; j < 3; j++){
        if (forward)
          gs.shN[cnt][3 * i + j] = temp[15 * j + i];
        else
          gs.shN[cnt][15 * j + i] = temp[3 * i + j];
      }
    }
  }
}

void applySigmoid(GS& gs, bool forward) {
    const std::size_t gaussianCnt = gs.getGaussianCount();
    auto* opacity = gs.opacity.data(); // Assuming gs.opacity is a vector or similar

    if (forward) {
        for (std::size_t i = 0; i < gaussianCnt; ++i) {
            float x = opacity[i];
            if (x >= 0) {
                float z = std::exp(-x);
                opacity[i] = 1.0f / (1.0f + z);
            } else {
                float z = std::exp(x);
                opacity[i] = z / (1.0f + z);
            }
        }
    } else {
        for (std::size_t i = 0; i < gaussianCnt; ++i) {
            float y = std::max(1e-6f, std::min(opacity[i], 1.0f - 1e-6f));
            opacity[i] = std::log(y / (1.0f - y));
        }
    }
}

void skipRotationDimension(GS& gs, const bool forward)
{
  const auto gaussianCnt = gs.getGaussianCount();
  if (forward) {
      for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
          auto a0 = gs.quats[cnt][0];
          auto a1 = gs.quats[cnt][1];
          auto a2 = gs.quats[cnt][2];
          auto a3 = gs.quats[cnt][3];

          // Normalize
          auto m = std::sqrt(a0 * a0 + a1 * a1 + a2 * a2 + a3 * a3);
          a0 /= m;
          a1 /= m;
          a2 /= m;
          a3 /= m;

      auto largest = std::abs(a0);
      int32_t largest_idx = 0;

      if (std::abs(a1) > largest) {
          largest = std::abs(a1); largest_idx = 1;
      }
      if (std::abs(a2) > largest) {
          largest = std::abs(a2); largest_idx = 2;
      }
      if (std::abs(a3) > largest) {
          largest_idx = 3;
      }

      switch (largest_idx) {
      case 0:
          if (a0 < 0) {
              a0 = -a0; a1 = -a1; a2 = -a2; a3 = -a3;
          }
          a0 = a1;
          a1 = a2;
          a2 = a3;
          a3 = 0;
          break;
      case 1:
          if (a1 < 0) {
              a0 = -a0; a1 = -a1; a2 = -a2; a3 = -a3;
          }
          a1 = a2;
          a2 = a3;
          a3 = 1;
          break;
      case 2:
          if (a2 < 0) {
              a0 = -a0; a1 = -a1; a2 = -a2; a3 = -a3;
          }
          a2 = a3;
          a3 = 2;
          break;
      case 3:
          if (a3 < 0) {
              a0 = -a0; a1 = -a1; a2 = -a2; a3 = -a3;
          }
          a3 = 3;
      }

      gs.quats[cnt][0] = a0;
      gs.quats[cnt][1] = a1;
      gs.quats[cnt][2] = a2;
      gs.quats[cnt][3] = a3;
    }
  }
  else {
    for (auto cnt = 0; cnt < gaussianCnt; cnt++) {

        auto a0 = gs.quats[cnt][0];
        auto a1 = gs.quats[cnt][1];
        auto a2 = gs.quats[cnt][2];
        auto a3 = gs.quats[cnt][3];

        auto largest_idx = (int32_t)gs.quats[cnt][3];

        auto ai = std::sqrt(std::max(0.0f, 1.0f - (a0 * a0 + a1 * a1 + a2 * a2)));

        switch (largest_idx) {
        case 0:
            a3 = a2;
            a2 = a1;
            a1 = a0;
            a0 = ai;
            break;
        case 1:
            a3 = a2;
            a2 = a1;
            a1 = ai;
            break;
        case 2:
            a3 = a2;
            a2 = ai;
            break;
        case 3:
            a3 = ai;
        }

        gs.quats[cnt][0] = a0;
        gs.quats[cnt][1] = a1;
        gs.quats[cnt][2] = a2;
        gs.quats[cnt][3] = a3;
    }
  }
}

void zeroOutSphericalHarmonics(GS& gs, const int sphericalOrder) {
    int totalSphericalHarmonics = 15;
    switch (sphericalOrder) {
        case 0:
            totalSphericalHarmonics = 0;  // Set every SPH_REST to zero
            break;
        case 1:
            totalSphericalHarmonics = 3;  // Only the first 9, rest are zero
            break;
        case 2:
            totalSphericalHarmonics = 8;  // Only the first 24, rest are zero
    }
    // set the extra SPH_REST to zero
    const auto gaussianCnt = gs.getGaussianCount();
    for (auto cnt = 0; cnt < gaussianCnt; cnt++) {
      for (auto ch = 0; ch < 3; ch++){
        for (auto k = totalSphericalHarmonics, s = ch * 15 + totalSphericalHarmonics; k < 15; k++, s++) {
            gs.shN[cnt][s] = 0;
        }
      }
    }
}

void yuvCoding(GS& gs, bool forward) {
    const std::size_t gaussianCnt = gs.getGaussianCount();
    for (std::size_t cnt = 0; cnt < gaussianCnt; ++cnt) {
        for (std::size_t n = 0; n < 15; ++n) {
            if (forward) {
                float r = gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 0));
                float g = gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 15));
                float b = gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 30));

                float y = static_cast<float>(0.2126 * r + 0.7152 * g + 0.0722 * b);
                float u = static_cast<float>(-0.1146 * r - 0.3854 * g + 0.5000 * b);
                float v = static_cast<float>(0.5000 * r - 0.4542 * g - 0.0458 * b);

                gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 0)) = y;
                gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 15)) = u;
                gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 30)) = v;
            } else {
                float y = gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 0));
                float u = gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 15));
                float v = gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 30));

                float r = static_cast<float>(y + 1.5748 * v);
                float g = static_cast<float>(y - 0.1873 * u - 0.4681 * v);
                float b = static_cast<float>(y + 1.8556 * u);

                gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 0)) = r;
                gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 15)) = g;
                gs.getAttr(AttrType::SPH_REST, static_cast<int>(cnt), static_cast<int>(n + 30)) = b;
            }
        }
    }
}

void processGaussian(GS& gs, const CoderParams& coderParams, bool forward)
{
  if (forward) {
    if (forward && coderParams.sphericalOrder != 3)
        zeroOutSphericalHarmonics(gs, coderParams.sphericalOrder);
    
    if (coderParams.REARRANGE_SPHREST)
      rearrangeSphericalHarmonics(gs, forward);

    if (coderParams.SKIP_ROT_DIM)
      skipRotationDimension(gs, forward);

    if (coderParams.APPLY_SIGMOID_OPACITY)
      applySigmoid(gs, forward);

    if (coderParams.YUV_CODING)
      yuvCoding(gs, forward);
  }
  else {
    if (coderParams.YUV_CODING)
      yuvCoding(gs, forward);
   
    if (coderParams.APPLY_SIGMOID_OPACITY)
      applySigmoid(gs, forward);
   
    if (coderParams.SKIP_ROT_DIM)
      skipRotationDimension(gs, forward);

    if (coderParams.REARRANGE_SPHREST)
      rearrangeSphericalHarmonics(gs, forward);
  }
}

//  ************************
//  Read and Write Functions
//  ************************


bool readFile(const string& srcName, GS& gaussian)
{
    cout << "Opening file " << srcName << "\n";
    ifstream in(srcName, ios::binary);
    if (!in) {
        cout << "Cannot open file: " << srcName << endl;
        return false;
    }
  
    string format;
    size_t vertex_count = 0;
    vector<PropertyInfo> props;

    // --- Parse Header ---
    string line;
    while (std::getline(in, line)) {
        istringstream iss(line);
        string word;
        iss >> word;

        if (word == "format") {
            iss >> format;
            if (format != "binary_little_endian"){
                cout << "Only binary_little_endian PLY is supported." << endl;
                in.close();
                return false;
            }
        } else if (word == "element") {
            string element_name;
            size_t count;
            iss >> element_name >> count;
            if (element_name == "vertex") vertex_count = count;
        } else if (word == "property") {
            string type, name;
            iss >> type >> name;
            props.push_back({name, type});
        } else if (word == "end_header") {
            break;
        }
    }

    // --- Parse Binary Data ---
    size_t sh0_count = 0, shN_count = 0;
    for (const auto &p : props) {
        if (p.name.find("f_dc_") == 0) sh0_count++;
        if (p.name.find("f_rest_") == 0) shN_count++;
    }
    
    size_t n = 0;
    size_t read_count = (n == 0 || n > vertex_count) ? vertex_count : n;

    for (size_t i = 0; i < read_count; ++i) {
        Vec3f positions, scale;
        Vec4f quat;
        float opacity = 0;
        Vec3f sh0;
        std::array<float, 45> shN;
        size_t sh0_idx = 0, shN_idx = 0;
        
        for (const auto& prop : props) {
            float value;
            in.read(reinterpret_cast<char*>(&value), 4);
            const string &name = prop.name;

            if (name == "x") positions[0] = value;
            else if (name == "y") positions[1] = value;
            else if (name == "z") positions[2] = value;
            else if (name == "opacity") opacity = value;
            else if (name == "scale_0") scale[0] = value;
            else if (name == "scale_1") scale[1] = value;
            else if (name == "scale_2") scale[2] = value;
            else if (name == "rot_0") quat[0] = value;
            else if (name == "rot_1") quat[1] = value;
            else if (name == "rot_2") quat[2] = value;
            else if (name == "rot_3") quat[3] = value;
            else if (name.find("f_dc_") == 0) {
                sh0[sh0_idx++] = value;
            } else if (name.find("f_rest_") == 0) {
                shN[shN_idx++] = value;
            }
        }
        
        gaussian.positions.push_back(positions);
        gaussian.opacity.push_back(opacity);
        gaussian.scales.push_back(scale);
        gaussian.quats.push_back(quat);
        gaussian.sh0.push_back(sh0);
        gaussian.shN.push_back(shN);
    }

    std::cout << " Number of Gaussians " << gaussian.positions.size() << std::endl;
    in.close();
    return true;
}

bool writeFile(const std::string& filename, const GS& gs, PlyFormat fmt) {
    if (filename.empty()) {
        std::cerr << "Output filename empty\n";
        return false;
    }

    std::ios::openmode mode = std::ios::out;
    if (fmt == PlyFormat::BinaryLittleEndian) {
        mode |= std::ios::binary;
    }

    std::ofstream out(filename, mode);
    if (!out) {
        throw std::runtime_error("Cannot open file for writing: " + filename);
    }

    const size_t N = gs.getGaussianCount();

    // --- Header ---
    out << "ply\n";
    if (fmt == PlyFormat::Ascii) {
        out << "format ascii 1.0\n";
    } else {
        out << "format binary_little_endian 1.0\n";
    }
    out << "element vertex " << N << "\n";
    out << "property float x\nproperty float y\nproperty float z\n";
    for (size_t i = 0; i < 3; ++i)
        out << "property float f_dc_" << i << "\n";
    for (size_t i = 0; i < 45; ++i)
        out << "property float f_rest_" << i << "\n";
    out << "property float opacity\n";
    out << "property float scale_0\nproperty float scale_1\nproperty float scale_2\n";
    out << "property float rot_0\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n";
    out << "end_header\n";

    if (fmt == PlyFormat::BinaryLittleEndian) {
        // --- Binary payload ---
        for (size_t i = 0; i < N; ++i) {
            out.write(reinterpret_cast<const char*>(gs.positions[i].data()), 3 * sizeof(float));
            out.write(reinterpret_cast<const char*>(gs.sh0[i].data()),       3 * sizeof(float));
            out.write(reinterpret_cast<const char*>(gs.shN[i].data()),      45 * sizeof(float));
            out.write(reinterpret_cast<const char*>(&gs.opacity[i]),         1 * sizeof(float));
            out.write(reinterpret_cast<const char*>(gs.scales[i].data()),    3 * sizeof(float));
            out.write(reinterpret_cast<const char*>(gs.quats[i].data()),     4 * sizeof(float));
        }
    } else {
        // --- ASCII payload ---
        // Ensure decimal point is '.', independent of system locale.
        out.imbue(std::locale::classic());
        out.setf(std::ios::fixed, std::ios::floatfield);
        out << std::setprecision(std::numeric_limits<float>::max_digits10); // typically 9 for float

        for (size_t i = 0; i < N; ++i) {
            // Position
            const auto& p = gs.positions[i];
            out << p[0] << ' ' << p[1] << ' ' << p[2] << ' ';

            // SH0 (3 floats)
            const auto& dc = gs.sh0[i];
            out << dc[0] << ' ' << dc[1] << ' ' << dc[2] << ' ';

            // SHN (45 floats)
            const auto& rest = gs.shN[i];
            for (int k = 0; k < 45; ++k) {
                out << rest[k] << ' ';
            }

            // Opacity
            out << gs.opacity[i] << ' ';

            // Scales (3 floats)
            const auto& s = gs.scales[i];
            out << s[0] << ' ' << s[1] << ' ' << s[2] << ' ';

            // Quaternion (4 floats)
            const auto& q = gs.quats[i];
            out << q[0] << ' ' << q[1] << ' ' << q[2] << ' ' << q[3] << '\n';
        }
    }

    return true;
}