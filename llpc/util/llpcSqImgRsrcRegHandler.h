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
  * @file  llpcSqImgRsrcRegHandler.h
  * @brief LLPC header file: contains the definition of LLPC utility class SqImgRsrcRegHandler.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcGfxRegHandler.h"

namespace Llpc
{

using namespace llvm;

// Struct used to record bits range info, value and modified or not
struct BitsState
{
    BitsInfo     info;
    Value*       pValue;
    uint32_t     isModified;
};

// SqRsrcParams id
// Note: Generalized params, would not match real registers
enum SqRsrcParams : uint32_t
{
    BaseAddress = 0,
    BaseAddressHi,
    Format,
    Width,
    Height,
    DstSelXYZW,
    IsTileOpt,
    Depth,
    Pitch,
    BcSwizzle,
    EnumCount,
};

// =====================================================================================================================
// Helper class for handling SqImgRsrcRegisters in compiling time polymorphism
class SqImgRsrcRegHandler : public GfxRegHandlerBase
{
    struct BitsStateGfx9
    {
        // SqRsrcRegs9 id
        enum SqRsrcRegs : uint32_t
        {
            BaseAddress = 0,
            BaseAddressHi,
            Format,
            Width,
            Height,
            DstSelXYZW,
            IsTileOpt,
            Depth,
            Pitch,
            BcSwizzle,
            EnumCount,
        };

        // Bits state list
        BitsState m_bitsStateList[SqRsrcRegs::EnumCount] =
        {
            {{ 0,  0, 32 }, nullptr, 0 }, // BaseAddress
            {{ 1,  0,  8 }, nullptr, 0 }, // BaseAddressHi
            {{ 1, 20,  9 }, nullptr, 0 }, // Format
            {{ 2,  0, 14 }, nullptr, 0 }, // Width
            {{ 2, 14, 14 }, nullptr, 0 }, // Height
            {{ 3,  0, 12 }, nullptr, 0 }, // DstSelXYZW
            {{ 3, 20,  5 }, nullptr, 0 }, // IsTileOpt
            {{ 4,  0, 13 }, nullptr, 0 }, // Depth
            {{ 4, 13, 12 }, nullptr, 0 }, // Pitch
            {{ 4, 29,  3 }, nullptr, 0 }, // BcSwizzle
        };
    } bitsStateGfx9;

    // Gfx9 Bits States look up table
    BitsState* pBitsStatesGfx9[SqRsrcParams::EnumCount] =
    {
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::BaseAddress],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::BaseAddressHi],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::Format],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::Width],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::Height],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::DstSelXYZW],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::IsTileOpt],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::Depth],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::Pitch],
        &bitsStateGfx9.m_bitsStateList[BitsStateGfx9::BcSwizzle],
    };

    struct BitsStateGfx10
    {
        // SqRsrcRegs10 id
        enum SqRsrcRegs : uint32_t
        {
            BaseAddress = 0,
            BaseAddressHi,
            Format,
            WidthLo,
            WidthHi,
            Height,
            DstSelXYZW,
            IsTileOpt,
            BcSwizzle,
            Depth,
            EnumCount
        };

        // Bits state list
        BitsState m_bitsStateList[SqRsrcRegs::EnumCount] =
        {
            {{ 0,  0, 32 }, nullptr, 0 }, // BaseAddress
            {{ 1,  0,  8 }, nullptr, 0 }, // BaseAddressHi
            {{ 1, 20,  9 }, nullptr, 0 }, // Format
            {{ 1, 30,  2 }, nullptr, 0 }, // WidthLo
            {{ 2,  0, 14 }, nullptr, 0 }, // WidthHi
            {{ 2, 14, 16 }, nullptr, 0 }, // Height
            {{ 3,  0, 12 }, nullptr, 0 }, // DstSelXYZW
            {{ 3, 20,  5 }, nullptr, 0 }, // IsTileOpt
            {{ 3, 25,  3 }, nullptr, 0 }, // BcSwizzle
            {{ 4,  0, 16 }, nullptr, 0 }, // Depth
        };
    } bitsStateGfx10;

    // Gfx10 Bits States look up table
    BitsState* pBitsStatesGfx10[SqRsrcParams::EnumCount] =
    {
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::BaseAddress],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::BaseAddressHi],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::Format],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::WidthLo],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::Height],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::DstSelXYZW],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::IsTileOpt],
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::Depth],
        nullptr,
        &bitsStateGfx10.m_bitsStateList[BitsStateGfx10::BcSwizzle],
    };

public:
    SqImgRsrcRegHandler(Builder* pBuilder, Value* pRegister, GfxIpVersion* pGfxIpVersion);

    // Get specific param value according to SqRsrcRegs id
    Value* GetParam(SqRsrcParams id);

    // Set specific param value according to SqRsrcRegs id
    void SetParam(SqRsrcParams id, Value* pVal);

protected:
    // Get register
    Value* GetReg(BitsState* pBitsState);

    // Set register
    void SetReg(BitsState* pBitsState, Value*);

    // Set GfxIpVersion
    void SetGfxIpVersion(GfxIpVersion* pGfxIpVersion) { m_pGfxIpVersion = pGfxIpVersion; }

protected:
    GfxIpVersion* m_pGfxIpVersion;
    BitsState**   m_pBitsSate;
    Value*        m_pInt32One;

private:
    // Get combined data from two seperate DWORDs
    Value* GetRegCombine( BitsState* pBitsStateLo, BitsState* pBitsStateHi);

    // Set data into two seperate DWORDs
    void SetRegCombine( BitsState* pBitsStateLo, BitsState* pBitsStateHi, Value* pReg);
};

} // Llpc
