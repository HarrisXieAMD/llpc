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

static constexpr BitsInfo g_sqImgRsrcRegBitsGfx9[SqRsrcRegsCount] =
{
    { 0,  0, 32 }, // BaseAddress
    { 1,  0,  8 }, // BaseAddressHi
    { 1, 20,  9 }, // Format
    { 2,  0, 14 }, // Width
    { 2, 14, 14 }, // Height
    { 3,  0, 12 }, // DstSelXYZW
    { 3, 20,  5 }, // IsTileOpt
    { 4,  0, 13 }, // Depth
    { 4, 13, 12 }, // Pitch
    { 4, 29,  3 }, // BcSwizzle
    {},            // WidthLo
    {},            // WidthHi
};

static constexpr BitsInfo g_sqImgRsrcRegBitsGfx10[SqRsrcRegsCount] =
{
    { 0,  0, 32 }, // BaseAddress
    { 1,  0,  8 }, // BaseAddressHi
    { 1, 20,  9 }, // Format
    {},            // Width
    { 2, 14, 16 }, // Height
    { 3,  0, 12 }, // DstSelXYZW
    { 3, 20,  5 }, // IsTileOpt
    { 4,  0, 16 }, // Depth
    {},            // Pitch
    { 3, 25,  3 }, // BcSwizzle
    { 1, 30,  2 }, // WidthLo
    { 2,  0, 14 }, // WidthHi
};

static constexpr BitsInfo g_sqImgSampRegBitsGfx9[SqSampRegsCount] =
{
    { 0, 30, 2 }, // FilterMode
    { 2, 20, 2 }, // XyMagFilter
    { 2, 22, 2 }, // XyMinFilter
};

// =====================================================================================================================
GfxRegHandler::GfxRegHandler(
    Builder*      pBuilder,      // [in] The builder handle
    Value*        pRegister,     // [in] The registered target vec <n x i32>
    GfxRegType    regType,       // The register type
    GfxIpVersion* pGfxIpVersion) // [in] The gfxip version
: GfxRegHandlerBase(pBuilder, pRegister)
{
    switch (regType)
    {
    case GfxRegType::SqImgRsrc:
        switch (pGfxIpVersion->major)
        {
        case 9:
            m_bitsInfo = g_sqImgRsrcRegBitsGfx9;
            break;
        case 10:
            m_bitsInfo = g_sqImgRsrcRegBitsGfx10;
            break;
        default:
            llvm_unreachable("GfxIp is not supported!");
            break;
        }
        m_bitsState = new BitsState[SqRsrcRegsCount];
        break;
    case GfxRegType::SqImgSamp:
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
        break;
    default:
        llvm_unreachable("GfxRegType is not supported!");
        break;
    }
}

GfxRegHandler::~GfxRegHandler()
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
Value* GfxRegHandler::GetReg(
    uint32_t id) // [in] The state of spefied range of bits
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