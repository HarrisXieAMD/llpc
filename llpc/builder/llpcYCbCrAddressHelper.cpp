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
#include "llpcSqImgRsrcRegHandler.h"
#include "llpcInternal.h"
#include "llpcTargetInfo.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace Llpc;
using namespace llvm;

// =====================================================================================================================
Value* YCbCrAddressHelper::GetParam(
    ImgRsrcParams id) // Pram ID
{
    switch (id)
    {
    case BaseAddress:
        return m_pRegHelper->GetReg(SqRsrcRegs::BaseAddress);
    case Format:
        return m_pRegHelper->GetReg(SqRsrcRegs::Format);
    case DstSelXYZW:
        return m_pRegHelper->GetReg(SqRsrcRegs::DstSelXYZW);
    case Depth:
        return m_pRegHelper->GetReg(SqRsrcRegs::Depth);
    case BcSwizzle:
        return m_pRegHelper->GetReg(SqRsrcRegs::BcSwizzle);
    case Height:
        return m_pRegHelper->GetReg(SqRsrcRegs::Height);
    case Pitch:
        return m_pBuilder->CreateAdd(m_pRegHelper->GetReg(SqRsrcRegs::Pitch), m_pInt32One);
    case Width:
        switch (m_pGfxIpVersion->major)
        {
        case 9:
            return m_pBuilder->CreateAdd(m_pRegHelper->GetReg(SqRsrcRegs::Width), m_pInt32One);
        case 10:
            return m_pBuilder->CreateAdd(GetRegCombine(SqRsrcRegs::WidthLo, SqRsrcRegs::WidthHi), m_pInt32One);
        default:
            llvm_unreachable("The current gfx is not supported!");
            break;
        }
    case IsTileOpt:
        return m_pBuilder->CreateICmpNE(m_pRegHelper->GetReg(SqRsrcRegs::IsTileOpt), m_pBuilder->getInt32(0));
    default:
        llvm_unreachable("Not implemented!");
    }
    return nullptr;
}

// =====================================================================================================================
void YCbCrAddressHelper::SetParam(
    ImgRsrcParams id,     // Param ID
    Value*        pParam) // [in] Param to be set
{
    switch (id)
    {
    case BaseAddress:
        m_pRegHelper->SetReg(SqRsrcRegs::BaseAddress, pParam);
        break;
    case Format:
        m_pRegHelper->SetReg(SqRsrcRegs::Format, pParam);
        break;
    case DstSelXYZW:
        m_pRegHelper->SetReg(SqRsrcRegs::DstSelXYZW, pParam);
        break;
    case Depth:
        m_pRegHelper->SetReg(SqRsrcRegs::Depth, pParam);
        break;
    case BcSwizzle:
        m_pRegHelper->SetReg(SqRsrcRegs::BcSwizzle, pParam);
        break;
    case Height:
        m_pRegHelper->SetReg(SqRsrcRegs::Height, pParam);
        break;
    case Pitch:
        m_pRegHelper->SetReg(SqRsrcRegs::Pitch, m_pBuilder->CreateSub(pParam, m_pInt32One));
        break;
    case Width:
        switch (m_pGfxIpVersion->major)
        {
        case 9:
            m_pRegHelper->SetReg(SqRsrcRegs::Width, m_pBuilder->CreateSub(pParam, m_pInt32One));
            break;
        case 10:
            SetRegCombine(SqRsrcRegs::WidthLo, SqRsrcRegs::WidthHi, m_pBuilder->CreateSub(pParam, m_pInt32One));
            break;
        default:
            llvm_unreachable("The current gfx is not supported!");
            break;
        }
        break;
    case IsTileOpt:
        llvm_unreachable("Set \"IsTileOpt\" is not allowed!");
        break;
    }
}

// =====================================================================================================================
// Get combined data from two seperate DWORDs
// Note: this function only supports 32 bits range
Value* YCbCrAddressHelper::GetRegCombine(
    SqRsrcRegs idLo, // [in] The state of spefied range of bits
    SqRsrcRegs idHi) // [in] The state of spefied range of bits
{
    Value* pDataLo = m_pRegHelper->GetReg(idLo);
    Value* pDataHi = m_pRegHelper->GetReg(idHi);
    return m_pBuilder->CreateOr(m_pBuilder->CreateShl(pDataHi, m_pRegHelper->GetBitsInfo(idLo).offset), pDataLo);
}

// =====================================================================================================================
// Set data into two seperate DWORDs
// Note: this function only supports 32 bits range
void YCbCrAddressHelper::SetRegCombine(
    SqRsrcRegs idLo, // [in] The state of spefied range of bits
    SqRsrcRegs idHi, // [in] The state of spefied range of bits
    Value*     pReg) // [in] Data used for setting
{
    Value* pDataLo = m_pBuilder->CreateIntrinsic(Intrinsic::amdgcn_ubfe,
                                                 m_pBuilder->getInt32Ty(),
                                                 { pReg,
                                                   m_pBuilder->getInt32(0),
                                                   m_pBuilder->getInt32(m_pRegHelper->GetBitsInfo(idLo).offset) });

    Value* pDataHi = m_pBuilder->CreateLShr(pReg, m_pBuilder->getInt32(m_pRegHelper->GetBitsInfo(idLo).offset));

    m_pRegHelper->SetReg(idLo, pDataLo);
    m_pRegHelper->SetReg(idHi, pDataHi);
}

// =====================================================================================================================
void YCbCrAddressHelper::GenBaseAddress(uint32_t planeNum)
{
    if (planeNum > 0)
    {
        m_PlaneBaseAddresses.push_back(GetParam(BaseAddress));
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
Value* YCbCrAddressHelper::Power2Align(Value* x, uint32_t align)
{
    uint32_t alignM1 = align - 1;
    uint32_t alignM1Inv = ~alignM1;
    Value* pResult = m_pBuilder->CreateAdd(x, m_pBuilder->getInt32(alignM1));
    return m_pBuilder->CreateAnd(pResult, m_pBuilder->getInt32(alignM1Inv));
}

// =====================================================================================================================
void YCbCrAddressHelper::GenHeightAndPitch(
    uint32_t bits,
    uint32_t bpp,
    uint32_t xBitCount,
    bool     isTileOptimal,
    uint32_t planeNum)
{
    switch (m_pGfxIpVersion->major)
    {
    case 9:
    {
        Value* pHeight = GetParam(Height);
        Value* pHeightR1 = m_pBuilder->CreateLShr(pHeight, m_pInt32One);
        m_pHeightY = pHeight;
        m_pHeightCb = pHeightR1;

        Value* pPitch = GetParam(Pitch);
        Value* pPitchR1 = m_pBuilder->CreateLShr(pPitch, m_pInt32One);
        Value* pPitchR1M1 = m_pBuilder->CreateSub(pPitchR1, m_pInt32One);

        // pPitchY * (m_xBitCount >> 3)
        m_pPitchY = m_pBuilder->CreateMul(pPitch, m_pBuilder->CreateLShr(m_pBuilder->getInt32(xBitCount), 3));

        // pPitchCb = pPitchCb * (m_xBitCount >> 3)
        m_pPitchCb = m_pBuilder->CreateMul(pPitchR1, m_pBuilder->CreateLShr(m_pBuilder->getInt32(xBitCount), 3));

        if (isTileOptimal)
        {
            Value* pIsTileOpt = GetParam(IsTileOpt);

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

        Value* pHeight = GetParam(Height);
        m_pHeightY = pHeight;

        Value* pWidth = GetParam(Width);
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

            Value* pIsTileOpt = GetParam(IsTileOpt);
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
