/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
  * @file  llpcYCbCrAddressHandler.h
  * @brief LLPC header file: contains the definition of LLPC class YCbCrAddressHandler.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcGfxRegHandler.h"
#include "llpcBuilderImpl.h"

namespace lgc
{

// =====================================================================================================================
// The Address helper class for YCbCr Conversion
class YCbCrAddressHandler
{
public:
    YCbCrAddressHandler(Builder* pBuilder, SqImgRsrcRegHandler* pSqImgRsrcRegHelper, GfxIpVersion* pGfxIpVersion)
    {
        m_pBuilder = pBuilder;
        m_pRegHelper = pSqImgRsrcRegHelper;
        m_pGfxIpVersion = pGfxIpVersion;
        m_PlaneBaseAddresses.clear();
        m_pOne = pBuilder->getInt32(1);
    }

    // Generate base address of each image plane
    void GenBaseAddress(uint32_t planeNum);

    // Generate height and pitch
    void GenHeightAndPitch(uint32_t bits, uint32_t bpp, uint32_t xBitCount, bool isTileOptimal, uint32_t planeNum);

    // Power2align operation
    llvm::Value* Power2Align(llvm::Value* x, uint32_t align);

    // Get specific plane
    inline llvm::Value* GetPlane(uint32_t idx) { return m_PlaneBaseAddresses[idx]; }

    // Get pitch for Y plane
    inline llvm::Value* GetPitchY() { return m_pPitchY; }

    // Get pitch for Cb plane
    inline llvm::Value* GetPitchCb() { return m_pPitchCb; }

    // Get height for Y plane
    inline llvm::Value* GetHeightY() { return m_pHeightY; }

    // Get Height for Cb plane
    inline llvm::Value* GetHeightCb() { return m_pHeightCb; }

    SqImgRsrcRegHandler*               m_pRegHelper;
    Builder*                           m_pBuilder;
    llvm::SmallVector<llvm::Value*, 3> m_PlaneBaseAddresses;
    llvm::Value*                       m_pPitchY;
    llvm::Value*                       m_pHeightY;
    llvm::Value*                       m_pPitchCb;
    llvm::Value*                       m_pHeightCb;
    llvm::Value*                       m_pOne;
    GfxIpVersion*                      m_pGfxIpVersion;
};

} // lgc
