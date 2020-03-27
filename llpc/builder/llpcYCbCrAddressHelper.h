/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
 /**
  ***********************************************************************************************************************
  * @file  llpcYCbCrAddressHelper.h
  * @brief LLPC header file: contains the definition of LLPC utility class YCbCrAddressHelper.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcGfxRegHelper.h"
#include "llpcBuilderImpl.h"

namespace lgc
{

using namespace llvm;

// =====================================================================================================================
// The Address helper class for YCbCr Conversion
class YCbCrAddressHelper
{
public:
    YCbCrAddressHelper(Builder* pBuilder, SqImgRsrcRegHelper* pSqImgRsrcRegHelper, GfxIpVersion* pGfxIpVersion)
    {
        m_pBuilder = pBuilder;
        m_pRegHelper = pSqImgRsrcRegHelper;
        m_PlaneBaseAddresses.clear();
        m_pGfxIpVersion = pGfxIpVersion;
        m_pInt32One = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);
    }

    // Generate base address of each image plane
    void GenBaseAddress(uint32_t planeNum);

    // Generate height and pitch
    // Note: Need pRegHelper to be bounded
    void GenHeightAndPitch(uint32_t bits, uint32_t bpp, uint32_t xBitCount, bool isTileOptimal, uint32_t planeNum);

    // Power2align operation
    Value* Power2Align(Value* x, uint32_t align);

    // Get specific plane
    inline Value* GetPlane(uint32_t idx) { return m_PlaneBaseAddresses[idx]; }

    // Get pitch for Y plane
    inline Value* GetPitchY() { return m_pPitchY; }

    // Get pitch for Cb plane
    inline Value* GetPitchCb() { return m_pPitchCb; }

    // Get height for Y plane
    inline Value* GetHeightY() { return m_pHeightY; }

    // Get Height for Cb plane
    inline Value* GetHeightCb() { return m_pHeightCb; }

    SqImgRsrcRegHelper*    m_pRegHelper;
    Builder*               m_pBuilder;
    SmallVector<Value*, 3> m_PlaneBaseAddresses;
    Value*                 m_pPitchY;
    Value*                 m_pHeightY;
    Value*                 m_pPitchCb;
    Value*                 m_pHeightCb;
    Value*                 m_pInt32One;
    GfxIpVersion*          m_pGfxIpVersion;
};

} // lgc
