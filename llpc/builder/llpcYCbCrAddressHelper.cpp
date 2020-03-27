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
* @file  llpcYCbCrAddressHelper.cpp
* @brief LLPC source file: Implementation of LLPC utility class YCbCrAddressHelper
***********************************************************************************************************************
*/
#include "llpcYCbCrAddressHelper.h"
#include "llpcGfxRegHelper.h"
#include "llpcInternal.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace lgc;
using namespace llvm;

// =====================================================================================================================
// Generate base address of each image plane
void YCbCrAddressHelper::GenBaseAddress(
    uint32_t planeNum) // The count of planes
{
    if (planeNum > 0)
    {
        m_PlaneBaseAddresses.push_back(m_pRegHelper->GetReg(BaseAddress));
        if (planeNum > 1)
        {
            m_PlaneBaseAddresses.push_back(m_pBuilder->CreateAdd(m_PlaneBaseAddresses[0],
                                           m_pBuilder->CreateLShr(m_pBuilder->CreateMul(m_pPitchY, m_pHeightY),
                                           m_pBuilder->getInt32(8))));
            if (planeNum > 2)
            {
                m_PlaneBaseAddresses.push_back(m_pBuilder->CreateAdd(m_PlaneBaseAddresses[1],
                                               m_pBuilder->CreateLShr(m_pBuilder->CreateMul(m_pPitchCb, m_pHeightCb),
                                               m_pBuilder->getInt32(8))));
            }
        }
    }
}

// =====================================================================================================================
// Power2align operation
Value* YCbCrAddressHelper::Power2Align(
    Value*   pX,    // [in] Input value to be aligned
    uint32_t align) // Align base
{
    uint32_t alignM1 = align - 1;
    uint32_t alignM1Inv = ~alignM1;
    Value* pResult = m_pBuilder->CreateAdd(pX, m_pBuilder->getInt32(alignM1));
    return m_pBuilder->CreateAnd(pResult, m_pBuilder->getInt32(alignM1Inv));
}

// =====================================================================================================================
// Calculate height and pitch
// Note: Need pRegHelper to be bounded
void YCbCrAddressHelper::GenHeightAndPitch(
    uint32_t bits,           // Channel bits
    uint32_t bpp,            // Bits per pixel
    uint32_t xBitCount,      // Effective channel bits
    bool     isTileOptimal,  // Is tiling optimal
    uint32_t planeNum)       // The number of planes
{
    switch (m_pGfxIpVersion->major)
    {
    case 9:
    {
        Value* pHeight = m_pRegHelper->GetReg(Height);
        Value* pHeightR1 = m_pBuilder->CreateLShr(pHeight, m_pInt32One);
        m_pHeightY = pHeight;
        m_pHeightCb = pHeightR1;

        Value* pPitch = m_pRegHelper->GetReg(Pitch);
        Value* pPitchR1 = m_pBuilder->CreateLShr(pPitch, m_pInt32One);

        // pPitchY * (m_xBitCount >> 3)
        m_pPitchY = m_pBuilder->CreateMul(pPitch, m_pBuilder->CreateLShr(m_pBuilder->getInt32(xBitCount), 3));

        // pPitchCb = pPitchCb * (m_xBitCount >> 3)
        m_pPitchCb = m_pBuilder->CreateMul(pPitchR1, m_pBuilder->CreateLShr(m_pBuilder->getInt32(xBitCount), 3));

        if (isTileOptimal)
        {
            Value* pIsTileOpt = m_pRegHelper->GetReg(IsTileOpt);

            Value* pPtchYOpt = m_pBuilder->CreateMul(pPitch, m_pBuilder->CreateLShr(m_pBuilder->getInt32(bits), 3));
            // pPitchY = pIsTileOpt ? (pPtchYOpt << 5) : pPitchY
            m_pPitchY = m_pBuilder->CreateSelect(pIsTileOpt, m_pBuilder->CreateShl(pPtchYOpt, m_pBuilder->getInt32(5)), m_pPitchY);

            // pPitchCbOpt = pPitchCb * (m_bits[0] >> 3)
            Value* pPitchCbOpt = m_pBuilder->CreateMul(pPitchR1, m_pBuilder->CreateLShr(m_pBuilder->getInt32(bits), 3));
            // pPitchCb = pIsTileOpt ? (pPitchCbOpt << 5) : pPitchCb
            m_pPitchCb = m_pBuilder->CreateSelect(pIsTileOpt, m_pBuilder->CreateShl(pPitchCbOpt, m_pBuilder->getInt32(5)), m_pPitchCb);
        }
        break;
    }
    case 10:
    {
        const uint32_t elementBytes = bpp >> 3;
        const uint32_t pitchAlign = (256 / elementBytes);

        Value* pHeight = m_pRegHelper->GetReg(Height);
        m_pHeightY = pHeight;

        Value* pWidth = m_pRegHelper->GetReg(Width);
        m_pPitchY = Power2Align(pWidth, pitchAlign);
        // pPitchY = pPitchY * pElementBytes
        m_pPitchY = m_pBuilder->CreateMul(m_pPitchY, m_pBuilder->getInt32(elementBytes));

        Value* pHeightR1 = m_pBuilder->CreateLShr(pHeight, m_pInt32One);
        m_pHeightCb = pHeightR1;

        Value* pWidthR1 = m_pBuilder->CreateLShr(pWidth, m_pInt32One);
        m_pPitchCb = Power2Align(pWidthR1, pitchAlign);
        // pPitchCb = pPitchCb * pElementBytes
        m_pPitchCb = m_pBuilder->CreateMul(m_pPitchCb, m_pBuilder->getInt32(elementBytes));

        if (isTileOptimal)
        {
            const uint32_t log2BlkSize = 16;
            const uint32_t log2EleBytes = log2(bpp >> 3);
            const uint32_t log2NumEle = log2BlkSize - log2EleBytes;
            const bool widthPrecedent = 1;
            const uint32_t log2Width = (log2NumEle + (widthPrecedent ? 1 : 0)) / 2;
            const uint32_t pitchAlignOpt = 1u << log2Width;
            const uint32_t heightAlignOpt = 1u << (log2NumEle - log2Width);

            // pPitchY = pPitchY * pElementBytes
            Value* pPtchYOpt = m_pBuilder->CreateMul(Power2Align(pWidth, pitchAlignOpt), m_pBuilder->getInt32(elementBytes));

            // pPitchCb = pPitchCb * pElementBytes
            Value* pPitchCbOpt = m_pBuilder->CreateMul(Power2Align(pWidthR1, pitchAlignOpt), m_pBuilder->getInt32(elementBytes));

            Value* pIsTileOpt = m_pRegHelper->GetReg(IsTileOpt);
            m_pPitchY = m_pBuilder->CreateSelect(pIsTileOpt, pPtchYOpt, m_pPitchY);
            m_pHeightY = m_pBuilder->CreateSelect(pIsTileOpt, Power2Align(pHeight, heightAlignOpt), pHeight);

            m_pPitchCb = m_pBuilder->CreateSelect(pIsTileOpt, pPitchCbOpt, m_pPitchCb);
            m_pHeightCb = m_pBuilder->CreateSelect(pIsTileOpt, Power2Align(pHeightR1, heightAlignOpt), pHeightR1);
        }
        break;
    }
    default:
        llvm_unreachable("GfxIp not supported!");
        break;
    }
}
