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
#include "llpcSqImgRsrcRegHandler.h"
#include "llpcBuilderImpl.h"

namespace Llpc
{

using namespace llvm;

enum ImgRsrcParams
{
    BaseAddress = 0,
    Format,
    Width,
    Height,
    DstSelXYZW,
    IsTileOpt,
    Depth,
    Pitch,
    BcSwizzle,
    Count,
};

// =====================================================================================================================
class YCbCrAddressHelper
{
public:
    YCbCrAddressHelper(Builder* pBuilder, GfxRegHandler* pSqImgRsrcRegHelper, GfxIpVersion* pGfxIpVersion)
    {
        m_pBuilder = pBuilder;
        m_pRegHelper = pSqImgRsrcRegHelper;
        m_PlaneBaseAddresses.clear();
        m_pGfxIpVersion = pGfxIpVersion;
        m_pInt32One = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);
    }
    void GenBaseAddress(uint32_t planeNum);
    void GenHeightAndPitch(uint32_t bits, uint32_t bpp, uint32_t xBitCount, bool isTileOptimal, uint32_t planeNum);

    Value* Power2Align(Value* x, uint32_t align);

    inline Value* GetPlane(uint32_t idx) { return m_PlaneBaseAddresses[idx]; }
    inline Value* GetPitchY() { return m_pPitchY; }
    inline Value* GetPitchCb() { return m_pPitchCb; }
    inline Value* GetHeightY() { return m_pHeightY; }
    inline Value* GetHeightCb() { return m_pHeightCb; }

    // Get param value
    Value* GetParam(ImgRsrcParams id);

    // Set param value
    void SetParam(ImgRsrcParams id, Value* pVal);

    Value* GetDesc() { return m_pRegHelper->GetRegister(); }

private:
    // Get combined data from two seperate DWORDs
    Value* GetRegCombine(SqRsrcRegs idLo, SqRsrcRegs idHi);

    // Set data into two seperate DWORDs
    void SetRegCombine(SqRsrcRegs idLo, SqRsrcRegs idHi, Value* pReg);

private:
    GfxRegHandler* m_pRegHelper;
    Builder* m_pBuilder;
    SmallVector<Value*, 3> m_PlaneBaseAddresses;
    Value* m_pPitchY;
    Value* m_pHeightY;
    Value* m_pPitchCb;
    Value* m_pHeightCb;
    Value* m_pInt32One;
    GfxIpVersion* m_pGfxIpVersion;
};

} // Llpc
