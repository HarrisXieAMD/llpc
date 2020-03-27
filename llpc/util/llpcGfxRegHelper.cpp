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
* @file  llpcGfxRegHelper.cpp
* @brief LLPC source file: Implementation of LLPC utility class GfxRegHelper
***********************************************************************************************************************
*/
#include "llpcGfxRegHelper.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace lgc;
using namespace llvm;

// =====================================================================================================================
GfxRegHelper::GfxRegHelper(
    Builder* pBuilder,  // [in] The builder handle
    Value*   pRegister) // [in] The registered target vec <n x i32>
: GfxRegHandlerBase(pBuilder, pRegister)
{
    m_pInt32One     = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);
    m_pGfxIpVersion = nullptr;
    m_bitsInfo      = nullptr;
    m_bitsState     = nullptr;
}

GfxRegHelper::~GfxRegHelper()
{
    if (m_bitsState != nullptr)
    {
        delete m_bitsState;
        m_bitsState = nullptr;
    }
}

// =====================================================================================================================
// Common function for getting register
// - Cache value when fetching
Value* GfxRegHelper::GetRegCommon(
    uint32_t id) // [in] The Reg ID
{
    // Under two condition, we need to fetch the range of bits
    //  - The register has not being initialized
    //  - The register is being modified
    if ((m_bitsState[id].pValue == nullptr) ||
        (m_bitsState[id].isModified == 1))
    {
        // Fetch bits according to BitsInfo
        m_bitsState[id].pValue = GetBits(m_bitsInfo[id]);
    }

    // Since the specified range of bits is cached, set it unmodified
    m_bitsState[id].isModified = 0;

    // Return the cached value
    return m_bitsState[id].pValue;
}

// =====================================================================================================================
// Get combined data from two seperate DWORDs
// Note: this function only supports 32 bits range
Value* GfxRegHelper::GetRegCombine(
    uint32_t idLo, // Reg ID low
    uint32_t idHi) // Reg ID high
{
    Value* pDataLo = GetRegCommon(idLo);
    Value* pDataHi = GetRegCommon(idHi);
    return m_pBuilder->CreateOr(m_pBuilder->CreateShl(pDataHi, m_bitsInfo[idLo].offset), pDataLo);
}

// =====================================================================================================================
// Set data into two seperate DWORDs
// Note: this function only supports 32 bits range
void GfxRegHelper::SetRegCombine(
    uint32_t idLo, // Reg ID low
    uint32_t idHi, // Reg ID high
    Value*   pReg) // [in] Data used for setting
{
    Value* pDataLo = m_pBuilder->CreateIntrinsic(Intrinsic::amdgcn_ubfe,
                                                 m_pBuilder->getInt32Ty(),
                                                 { pReg,
                                                   m_pBuilder->getInt32(0),
                                                   m_pBuilder->getInt32(m_bitsInfo[idLo].offset) });

    Value* pDataHi = m_pBuilder->CreateLShr(pReg, m_pBuilder->getInt32(m_bitsInfo[idLo].offset));

    SetRegCommon(idLo, pDataLo);
    SetRegCommon(idHi, pDataHi);
}

// =====================================================================================================================
// SqImgSampReg Bits infomation look up table (Gfx9-10)
static constexpr BitsInfo g_sqImgSampRegBitsGfx9[SqSampRegsCount] =
{
    { 0, 30, 2 }, // FilterMode
    { 2, 20, 2 }, // XyMagFilter
    { 2, 22, 2 }, // XyMinFilter
};

// =====================================================================================================================
SqImgSampRegHelper::SqImgSampRegHelper(
    Builder*      pBuilder,      // [in] Bound builder context
    Value*        pRegister,     // [in] Bound register vec <n x i32>
    GfxIpVersion* pGfxIpVersion) // [in] Target gfxIp version
: GfxRegHelper(pBuilder, pRegister)
{
    m_pGfxIpVersion = pGfxIpVersion;

    switch (pGfxIpVersion->major)
    {
    case 9:
    case 10:
        m_bitsInfo = g_sqImgSampRegBitsGfx9;
        break;
    default:
        llvm_unreachable("GfxIp is not supported!");
        break;
    }
    m_bitsState = new BitsState[SqSampRegsCount];
}

// =====================================================================================================================
// Acquire register value
Value* SqImgSampRegHelper::GetReg(
    SqSampRegs id) // Register ID
{
    switch (id)
    {
    case FilterMode:
    case xyMagFilter:
    case xyMinFilter:
        return GetRegCommon(id);
    default:
        llvm_unreachable("Not implemented!");
        break;
    }
    return nullptr;
}

// =====================================================================================================================
// Set register value
void SqImgSampRegHelper::SetReg(
    SqSampRegs id,     // Register ID
    Value*     pParam) // [in] Param to be set
{
    switch (id)
    {
    case FilterMode:
    case xyMagFilter:
    case xyMinFilter:
        SetRegCommon(id, pParam);
        break;
    default:
        llvm_unreachable("Set \"IsTileOpt\" is not allowed!");
        break;
    }
}
