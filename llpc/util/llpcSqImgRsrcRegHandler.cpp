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
* @file  llpcSqImgRsrcRegHandler.cpp
* @brief LLPC source file: Implementation of LLPC utility class SqImgRsrcRegHandler
***********************************************************************************************************************
*/
#include "llpcSqImgRsrcRegHandler.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace Llpc;
using namespace llvm;

SqImgRsrcRegHandler::SqImgRsrcRegHandler(
    Builder*      pBuilder,      // [in] The builder handle
    Value*        pRegister,     // [in] The registered target vec <n x i32>
    GfxIpVersion* pGfxIpVersion) // [in] The gfxip version
    : GfxRegHandlerBase(pBuilder, pRegister)
{
    m_pGfxIpVersion = pGfxIpVersion;
    m_pInt32One = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);

    switch (pGfxIpVersion->major)
    {
    case 9:
        m_pBitsSate = pBitsStatesGfx9;
        break;
    case 10:
        m_pBitsSate = pBitsStatesGfx10;
        break;
    default:
        llvm_unreachable("The current gfx is not supported!");
        break;
    }
}

Value* SqImgRsrcRegHandler::GetParam(
    SqRsrcParams id) // Pram ID
{
    switch (id)
    {
    case BaseAddress:
    case BaseAddressHi:
    case Format:
    case DstSelXYZW:
    case Depth:
    case BcSwizzle:
        return GetReg(m_pBitsSate[id]);
    case Height:
    case Pitch:
        return m_pBuilder->CreateAdd(GetReg(m_pBitsSate[id]), m_pInt32One);
    case Width:
        switch (m_pGfxIpVersion->major)
        {
        case 9:
            return m_pBuilder->CreateAdd(GetReg(m_pBitsSate[id]), m_pInt32One);
        case 10:
            return m_pBuilder->CreateAdd(GetRegCombine(m_pBitsSate[id], m_pBitsSate[id]+1), m_pInt32One);
        default:
            llvm_unreachable("The current gfx is not supported!");
            break;
        }
    case IsTileOpt:
        return m_pBuilder->CreateICmpNE(GetReg(m_pBitsSate[id]), m_pBuilder->getInt32(0));
    default:
        llvm_unreachable("Not implemented!");
    }
    return nullptr;
}

void SqImgRsrcRegHandler::SetParam(
    SqRsrcParams id,     // Param ID
    Value*       pParam) // [in] Param to be set
{
    switch (id)
    {
    case BaseAddress:
    case BaseAddressHi:
    case Format:
    case DstSelXYZW:
    case Depth:
    case BcSwizzle:
        SetReg(m_pBitsSate[id], pParam);
        break;
    case Height:
    case Pitch:
        SetReg(m_pBitsSate[id], m_pBuilder->CreateSub(pParam, m_pInt32One));
        break;
    case Width:
        switch (m_pGfxIpVersion->major)
        {
        case 9:
            SetReg(m_pBitsSate[id], m_pBuilder->CreateSub(pParam, m_pInt32One));
            break;
        case 10:
            SetRegCombine(m_pBitsSate[id], m_pBitsSate[id]+1, m_pBuilder->CreateSub(pParam, m_pInt32One));
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
// Common function for getting register
// - Record whether the register is being modified or not
// - Record value to bit state
Value* SqImgRsrcRegHandler::GetReg(
    BitsState* pBitsState) // [in] The state of spefied range of bits
{
    // Under two condition, we need to fetch the range of bits
    //  - The register has not being initialized
    //  - The register is being modified
    if ((pBitsState->pValue == nullptr) ||
        (pBitsState->isModified == 1))
    {
        // Fetch bits according to BitsInfo
        pBitsState->pValue = GetBits(pBitsState->info);
    }

    // Since the specified range of bits is cached, set it unmodified
    pBitsState->isModified = 0;

    // Return the cached value
    return pBitsState->pValue;
}

// =====================================================================================================================
// Common function for set register
// - Mark if being modified
void SqImgRsrcRegHandler::SetReg(
    BitsState* pBitsState, // [in] The state of spefied range of bits
    Value*     pVal)       // [in] Data used for setting
{
    // Set register
    SetBits(pBitsState->info, pVal);

    // It is assumed the register is being modified
    pBitsState->isModified = 1;
}

// =====================================================================================================================
// Get combined data from two seperate DWORDs
// Note: this function only supports 32 bits range
Value* SqImgRsrcRegHandler::GetRegCombine(
    BitsState* pBitsStateLo, // [in] The state of spefied range of bits
    BitsState* pBitsStateHi) // [in] The state of spefied range of bits
{
    Value* pDataLo = GetReg(pBitsStateLo);
    Value* pDataHi = GetReg(pBitsStateHi);
    return m_pBuilder->CreateOr(m_pBuilder->CreateShl(pDataHi, pBitsStateLo->info.offset), pDataLo);
}

// =====================================================================================================================
// Set data into two seperate DWORDs
// Note: this function only supports 32 bits range
void SqImgRsrcRegHandler::SetRegCombine(
    BitsState* pBitsStateLo, // [in] The state of spefied range of bits
    BitsState* pBitsStateHi, // [in] The state of spefied range of bits
    Value*             pReg) // [in] Data used for setting
{
    Value* pDataLo = m_pBuilder->CreateIntrinsic(Intrinsic::amdgcn_ubfe,
                                                 m_pBuilder->getInt32Ty(),
                                                 { pReg,
                                                   m_pBuilder->getInt32(0),
                                                   m_pBuilder->getInt32(pBitsStateLo->info.offset) });

    Value* pDataHi = m_pBuilder->CreateLShr(pReg, m_pBuilder->getInt32(pBitsStateLo->info.offset));

    SetReg(pBitsStateLo, pDataLo);
    SetReg(pBitsStateHi, pDataHi);
}
