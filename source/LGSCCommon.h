/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: Apache-2.0 */

#ifndef LGSCCOMMON_H
#define LGSCCOMMON_H

#include "attributes.h"
#include "LGSCMisc.h"

// ******************************
// ** Read and Write Functions **
// ******************************

struct PropertyInfo {
    std::string name;
    std::string type;
};

enum class PlyFormat { BinaryLittleEndian, Ascii };

bool readFile(const std::string& srcName, GS& gaussian);

bool writeFile(const std::string& filename, const GS& gs, PlyFormat fmt = PlyFormat::BinaryLittleEndian);

// ************************
// *** Helper Functions ***
// ************************


void initializeQuantizers(std::vector<Quantizer>& posQuantizer, std::vector<Quantizer>& attrQuantizer, const CoderParams& coderParams, const GS& gs);

void zeroOutSphericalHarmonics(GS& gs, const int sphericalOrder);

void rearrangeSphericalHarmonics(GS& gs, const bool forward);

void applySigmoid(GS& gs, const bool forward);

void skipRotationDimension(GS& gs, const bool forward);

void processGaussian(GS& gs, const CoderParams& coderParams, bool forward);


#endif