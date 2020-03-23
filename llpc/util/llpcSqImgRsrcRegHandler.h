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

enum class GfxRegType
{
    SqImgRsrc,
    SqImgSamp,
};

// Struct used to record bits range info
enum SqRsrcRegs
{
    BaseAddress = 0,
    BaseAddressHi,
    Format,
    Width, // only gfx9 and before
    Height,
    DstSelXYZW,
    IsTileOpt,
    Depth,
    Pitch,
    BcSwizzle,

    // The following are introduced in gfx10.
    WidthLo,
    WidthHi,

    SqRsrcRegsCount,
};

enum SqSampRegs
{
    FilterMode = 0,
    xyMagFilter,
    xyMinFilter,

    SqSampRegsCount,
};

struct BitsState
{
    Value* pValue = nullptr;
    bool isModified = false;
};

// =====================================================================================================================
// Helper class for handling SqImgRsrcRegisters in compiling time polymorphism
class GfxRegHandler : public GfxRegHandlerBase
{
public:
    GfxRegHandler(Builder* pBuilder, Value* pRegister, GfxRegType regType, GfxIpVersion* pGfxIpVersion);

    virtual ~GfxRegHandler();

    // Get register
    Value* GetReg(uint32_t id);

    // Get bitsInfo
    const BitsInfo& GetBitsInfo(uint32_t id) { return m_bitsInfo[id]; }

    // Set register
    inline void SetReg(uint32_t id, Value* pVal)
    {
        // Set register
        SetBits(m_bitsInfo[id], pVal);
        // It is assumed the register is being modified
        m_bitsState[id].isModified = 1;
    }

protected:
    BitsState*      m_bitsState;
    const BitsInfo* m_bitsInfo;
};

} // Llpc
