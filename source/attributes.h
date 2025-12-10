/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#pragma once
#ifndef ATTRIBUTES_H
#define ATTRIBUTES_H

#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <iostream>
#include <iomanip>
#include <cfloat>
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <stdexcept>

enum class AttrType { POS, SPH_DC, SPH_REST, OPACITY, SCALE, ROT4 };

using Vec3f = std::array<float, 3>;
using Vec4f = std::array<float, 4>;

struct Attribute {
    AttrType name;                             // Attribute group name (e.g., SPH_DC)
    std::vector<std::string> tags;             // Individual dimension tags (e.g., f_dc_0)
    int numDims;                               // Number of dimensions
};

struct AttributeStats {
    int attrCnt;
    std::vector<Attribute> attributes;        // All attributes (INRIA format)
    std::map<std::string, double> minVals, maxVals;
    std::unordered_map<AttrType, std::size_t> nameToIdx;
    
    bool printPerDimStats = true;
    bool printPerAttrStats = false;

    AttributeStats() : attrCnt(0) {}

    // Initialize strictly in INRIA format
    void initINRIA() {
        attributes.clear();
        attrCnt = 0;

        // Define INRIA attributes
        addAttribute(AttrType::POS, {"x", "y", "z"});
        addAttribute(AttrType::SPH_DC, {"f_dc_0", "f_dc_1", "f_dc_2"});
        addAttribute(AttrType::SPH_REST, generateTags("f_rest_", 45));
        addAttribute(AttrType::OPACITY, {"opacity"});
        addAttribute(AttrType::SCALE, generateTags("scale_", 3));
        addAttribute(AttrType::ROT4, generateTags("rot_", 4));

        attrCnt = static_cast<int>(attributes.size());

        // Initialize min/max for all tags
        for (const auto& attr : attributes) {
            for (const auto& tag : attr.tags) {
                minVals[tag] = DBL_MAX;
                maxVals[tag] = -DBL_MAX;
            }
        }
    }

    void update(const std::string& tag, double val) {
        if (minVals.find(tag) != minVals.end()) {
            minVals[tag] = std::min(minVals[tag], val);
            maxVals[tag] = std::max(maxVals[tag], val);
        }
    }

    static std::string attrTypeToString(AttrType type) {
        switch (type) {
            case AttrType::POS: return "POS";
            case AttrType::SPH_DC: return "SPH_DC";
            case AttrType::SPH_REST: return "SPH_REST";
            case AttrType::OPACITY: return "OPACITY";
            case AttrType::SCALE: return "SCALE";
            case AttrType::ROT4: return "ROT4";
            default: return "UNKNOWN";
        }
    }

    void print() const {
        if (printPerDimStats) {
            std::cout << "---- Per-Dimension Stats ----\n";
            for (const auto& attr : attributes) {
                for (const auto& tag : attr.tags) {
                    std::cout << std::setw(12) << tag
                              << std::setw(20) << minVals.at(tag)
                              << std::setw(20) << maxVals.at(tag) << "\n";
                }
            }
        }

        if (printPerAttrStats) {
            std::cout << "\n---- Per-Attribute Summary ----\n";
            for (const auto& attr : attributes) {
                double minVal = DBL_MAX, maxVal = -DBL_MAX;
                for (const auto& tag : attr.tags) {
                    minVal = std::min(minVal, minVals.at(tag));
                    maxVal = std::max(maxVal, maxVals.at(tag));
                }
                std::cout << std::setw(12) << attrTypeToString(attr.name)
                          << std::setw(20) << minVal
                          << std::setw(20) << maxVal << "\n";
            }
        }
    }
    
    int getAttrIdx(AttrType attrName) const {
        auto it = nameToIdx.find(attrName);
        return (it != nameToIdx.end()) ? static_cast<int>(it->second) : -1;
    }

private:
    void addAttribute(AttrType name, const std::vector<std::string>& tags) {
        std::size_t idx = attributes.size();
        attributes.push_back({name, tags, static_cast<int>(tags.size())});
        nameToIdx.emplace(name, idx);
    }


    std::vector<std::string> generateTags(const std::string& prefix, int count) {
        std::vector<std::string> tags;
        for (int i = 0; i < count; ++i) {
            tags.push_back(prefix + std::to_string(i));
        }
        return tags;
    }
};


struct GS {
    std::vector<Vec3f> positions;         // x, y, z
    std::vector<Vec4f> quats;             // x, y, z, w
    std::vector<Vec3f> scales;
    std::vector<float> opacity;
    std::vector<Vec3f> sh0;
    std::vector<std::array<float, 45>> shN;
    
    std::size_t getGaussianCount() const noexcept { return positions.size(); }
    
    void clear() noexcept {
        positions.clear();
        opacity.clear();
        scales.clear();
        quats.clear();
        sh0.clear();
        shN.clear();
    }
    
    void resize(std::size_t n) {
        positions.resize(n);
        quats.resize(n);
        scales.resize(n);
        opacity.resize(n);
        sh0.resize(n);
        shN.resize(n);
    }

    static int getAttrDims(AttrType type) noexcept {
        switch (type) {
            case AttrType::POS:       return 3;
            case AttrType::SPH_DC:    return 3;
            case AttrType::SPH_REST:  return 45;
            case AttrType::OPACITY:   return 1;
            case AttrType::SCALE:     return 3;
            case AttrType::ROT4:      return 4;
            default:                  return 0; // unknown
        }
    }

    float& getAttr(AttrType type, std::size_t idx, int dim) {
        switch (type) {
            case AttrType::POS:
                return positions[idx][dim];
            case AttrType::SPH_DC:
                return sh0[idx][dim];
            case AttrType::SPH_REST:
                return shN[idx][dim];
            case AttrType::OPACITY:
                return opacity[idx];
            case AttrType::SCALE:
                return scales[idx][dim];
            case AttrType::ROT4:
                return quats[idx][dim];
            default:
                throw std::invalid_argument("Unknown attribute type");
        }
    }
    float getAttr(AttrType type, std::size_t idx, int dim) const {
        return const_cast<GS*>(this)->getAttr(type, idx, dim);
    }

    
    void setAttributeStats(const AttributeStats& stats) { attrStats = stats; }
    AttributeStats& getAttributeStats() { return attrStats; }
    const AttributeStats& getAttributeStats() const { return attrStats; }
    
    void deriveStats() {
        attrStats.initINRIA();
        
        const int N = static_cast<int>(getGaussianCount());
        for (const auto& attr : attrStats.attributes) {
            for (int dim = 0; dim < attr.numDims; ++dim) {
                const std::string& tag = attr.tags[dim];
                double minVal = DBL_MAX, maxVal = -DBL_MAX;
                for (int n = 0; n < N; ++n) {
                    double v = 0.0;
                    switch (attr.name) {
                        case AttrType::POS:
                            v = static_cast<double>(positions[n][dim]);
                            break;
                        case AttrType::SPH_DC:
                            v = static_cast<double>(sh0[n][dim]);
                            break;
                        case AttrType::SPH_REST:
                            v = static_cast<double>(shN[n][dim]);
                            break;
                        case AttrType::OPACITY:
                            v = static_cast<double>(opacity[n]);
                            break;
                        case AttrType::SCALE:
                            v = static_cast<double>(scales[n][dim]);
                            break;
                        case AttrType::ROT4:
                            v = static_cast<double>(quats[n][dim]);
                            break;
                        default:
                            continue;
                    }
                    if (v < minVal) minVal = v;
                    if (v > maxVal) maxVal = v;
                }
                attrStats.minVals[tag] = minVal;
                attrStats.maxVals[tag] = maxVal;
            }
        }
    }

    void populateGSAttributes(
        int& numGaussians,
        std::vector<float>& pos,
        std::vector<float>& rotations,
        std::vector<float>& scalesOut,
        std::vector<float>& opacities,
        std::vector<float>& features_diffuse,
        std::vector<float>& features_specular,
        int shDegree = 3)
    {
        const std::size_t n = getGaussianCount();
        numGaussians = static_cast<int>(n);

        const int posDims  = getAttrDims(AttrType::POS);
        const int rotDims  = getAttrDims(AttrType::ROT4);
        const int sclDims  = getAttrDims(AttrType::SCALE);
        const int opaDims  = getAttrDims(AttrType::OPACITY);   // usually 1
        const int dcDims   = getAttrDims(AttrType::SPH_DC);    // also equals channel count
        const int numShDims = ((shDegree + 1) * (shDegree + 1) - 1);

        // Resize outputs according to queried dims
        pos.resize(n * static_cast<std::size_t>(posDims));
        rotations.resize(n * static_cast<std::size_t>(rotDims));
        scalesOut.resize(n * static_cast<std::size_t>(sclDims));
        opacities.resize(n * static_cast<std::size_t>(opaDims));
        features_diffuse.resize(n * static_cast<std::size_t>(dcDims));
        features_specular.resize(n * static_cast<std::size_t>(numShDims * 3));

        // --- Copy each attribute using getAttrDims/getAttr (dimension-agnostic) ---

        // POS
        {
            float* fPtr = pos.data();
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < posDims; ++d)
                    *fPtr++ = getAttr(AttrType::POS, i, d);
        }
        // ROT4
        {
            float* fPtr = rotations.data();
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < rotDims; ++d)
                    *fPtr++ = getAttr(AttrType::ROT4, i, d);
        }
        // SCALE
        {
            float* fPtr = scalesOut.data();
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < sclDims; ++d)
                    *fPtr++ = getAttr(AttrType::SCALE, i, d);
        }
        // OPACITY
        {
            float* fPtr = opacities.data();
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < opaDims; ++d)
                    *fPtr++ = getAttr(AttrType::OPACITY, i, d);
        }
        // SPH_DC (diffuse)
        {
            float* fPtr = features_diffuse.data();
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < dcDims; ++d)
                    *fPtr++ = getAttr(AttrType::SPH_DC, i, d);
        }
        // SPH_REST (specular): per-channel truncation to shDegree
        {
            float* fPtr = features_specular.data();
            for (std::size_t i = 0; i < n; ++i) {
                for (auto ch = 0; ch < 3; ch++){
                    for (auto dim = 0, s = 15 * ch; dim < numShDims; dim++, s++){
                        *fPtr++ = getAttr(AttrType::SPH_REST, i, s);
                    }
                }
            }
        }
    }

    void populateGSAttributes(
        int& numGaussians,
        float** pos,
        float** rotations,
        float** scalesOut,
        float** opacities,
        float** features_diffuse,
        float** features_specular,
        int shDegree = 3)
    {
        const std::size_t n = getGaussianCount();
        numGaussians = static_cast<int>(n);

        const int posDims   = getAttrDims(AttrType::POS);
        const int rotDims   = getAttrDims(AttrType::ROT4);
        const int sclDims   = getAttrDims(AttrType::SCALE);
        const int opaDims   = getAttrDims(AttrType::OPACITY);
        const int dcDims    = getAttrDims(AttrType::SPH_DC);
        const int numShDims = ((shDegree + 1) * (shDegree + 1) - 1);

        // Allocate outputs
        *pos              = new float[n * static_cast<std::size_t>(posDims)];
        *rotations        = new float[n * static_cast<std::size_t>(rotDims)];
        *scalesOut        = new float[n * static_cast<std::size_t>(sclDims)];
        *opacities        = new float[n * static_cast<std::size_t>(opaDims)];
        *features_diffuse = new float[n * static_cast<std::size_t>(dcDims)];
        *features_specular= new float[n * static_cast<std::size_t>(numShDims * 3)];

        // POS
        {
            float* fPtr = *pos;
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < posDims; ++d)
                    *fPtr++ = getAttr(AttrType::POS, i, d);
        }

        // ROT4
        {
            float* fPtr = *rotations;
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < rotDims; ++d)
                    *fPtr++ = getAttr(AttrType::ROT4, i, d);
        }

        // SCALE
        {
            float* fPtr = *scalesOut;
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < sclDims; ++d)
                    *fPtr++ = getAttr(AttrType::SCALE, i, d);
        }

        // OPACITY
        {
            float* fPtr = *opacities;
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < opaDims; ++d)
                    *fPtr++ = getAttr(AttrType::OPACITY, i, d);
        }

        // SPH_DC (diffuse)
        {
            float* fPtr = *features_diffuse;
            for (std::size_t i = 0; i < n; ++i)
                for (int d = 0; d < dcDims; ++d)
                    *fPtr++ = getAttr(AttrType::SPH_DC, i, d);
        }

        // SPH_REST (specular): 3 channels, up to numShDims per channel, stored with 15 per channel.
        {
            float* fPtr = *features_specular;
            for (std::size_t i = 0; i < n; ++i) {
                for (int ch = 0; ch < 3; ++ch) {
                    for (int dim = 0, s = 15 * ch; dim < numShDims; ++dim, ++s) {
                        *fPtr++ = getAttr(AttrType::SPH_REST, i, s);
                    }
                }
            }
        }
    }

    void parseAttributes(int numGaussians, 
        std::vector<float>& pos,
        std::vector<float>& rotations,
        std::vector<float>& scalesOut,
        std::vector<float>& opacties,
        std::vector<float>& features_diffuse,
        std::vector<float>& features_specular, 
        int shDegree)
    {
        resize(numGaussians);

        const int posDims  = getAttrDims(AttrType::POS);
        const int rotDims  = getAttrDims(AttrType::ROT4);
        const int sclDims  = getAttrDims(AttrType::SCALE);
        const int opaDims  = getAttrDims(AttrType::OPACITY);   // usually 1
        const int dcDims   = getAttrDims(AttrType::SPH_DC);    // also equals channel count
        const int numShDims = ((shDegree + 1) * (shDegree + 1) - 1);  // per channel

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (int d = 0; d < posDims; ++d)
                getAttr(AttrType::POS, i, d) = pos[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < rotDims; dim++)
                getAttr(AttrType::ROT4, i, dim) = rotations[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < sclDims; dim++)
                getAttr(AttrType::SCALE, i, dim) = scalesOut[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < opaDims; dim++)
                getAttr(AttrType::OPACITY, i, dim) = opacties[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < dcDims; dim++)
                getAttr(AttrType::SPH_DC, i, dim) = features_diffuse[ctr++];

        if (numShDims)
            for (auto i = 0, ctr = 0; i < numGaussians; i++)
                for (auto c = 0; c < 3; c++)
                    for (auto n = 0, s = 15 * c; n < numShDims; n++, s++)
                        getAttr(AttrType::SPH_REST, i, s) = features_specular[ctr++];
    }

    void parseAttributes(int numGaussians, 
            const float* pos,
            const float* rotations,
            const float* scalesOut,
            const float* opacties,
            const float* features_diffuse,
            const float* features_specular, 
            int shDegree)
    {
        
        resize(numGaussians);

        const int posDims  = getAttrDims(AttrType::POS);
        const int rotDims  = getAttrDims(AttrType::ROT4);
        const int sclDims  = getAttrDims(AttrType::SCALE);
        const int opaDims  = getAttrDims(AttrType::OPACITY);
        const int dcDims   = getAttrDims(AttrType::SPH_DC);
        const int numShDims = ((shDegree + 1) * (shDegree + 1) - 1);  // per channel

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (int d = 0; d < posDims; ++d)
                getAttr(AttrType::POS, i, d) = pos[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < rotDims; dim++)
                getAttr(AttrType::ROT4, i, dim) = rotations[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < sclDims; dim++)
                getAttr(AttrType::SCALE, i, dim) = scalesOut[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < opaDims; dim++)
                getAttr(AttrType::OPACITY, i, dim) = opacties[ctr++];

        for (auto i = 0, ctr = 0; i < numGaussians; i++)
            for (auto dim = 0; dim < dcDims; dim++)
                getAttr(AttrType::SPH_DC, i, dim) = features_diffuse[ctr++];

        if (numShDims)
            for (auto i = 0, ctr = 0; i < numGaussians; i++)
                for (auto c = 0; c < 3; c++)
                    for (auto n = 0, s = 15 * c; n < numShDims; n++, s++)
                        getAttr(AttrType::SPH_REST, i, s) = features_specular[ctr++];
    }

   
    private:
        AttributeStats attrStats;
};

#endif