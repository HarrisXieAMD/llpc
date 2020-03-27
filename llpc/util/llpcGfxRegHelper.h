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
  * @file  llpcGfxRegHelper.h
  * @brief LLPC header file: contains the definition of LLPC utility class GfxRegHelper.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcGfxRegHandler.h"
#include "llpcTargetInfo.h"

namespace lgc
{

using namespace llvm;

struct BitsState
{
    Value* pValue = nullptr;
    bool isModified = false;
};

// =====================================================================================================================
// Helper class for handling graphics registers
class GfxRegHelper : public GfxRegHandlerBase
{
protected:
    GfxRegHelper(Builder* pBuilder, Value* pRegister);

    virtual ~GfxRegHelper();

    // Get register
    Value* GetRegCommon(uint32_t id);

    // Set register
    inline void SetRegCommon(uint32_t id, Value* pVal)
    {
        SetBits(m_bitsInfo[id], pVal);
        // It is assumed the register is being modified
        m_bitsState[id].isModified = 1;
    }

    // Get combined data from two seperate DWORDs
    Value* GetRegCombine(uint32_t idLo, uint32_t idHi);

    // Set data into two seperate DWORDs
    void SetRegCombine(uint32_t idLo, uint32_t idHi, Value* pReg);

protected:
    BitsState*      m_bitsState;
    const BitsInfo* m_bitsInfo;
    Value*          m_pInt32One;
    GfxIpVersion*   m_pGfxIpVersion;
};

// SqImgSampRegisters Id
enum SqSampRegs
{
    FilterMode = 0,
    xyMagFilter,
    xyMinFilter,

    SqSampRegsCount,
};

// =====================================================================================================================
// Helper class for handling SqImgRsrcRegisters
class SqImgSampRegHelper : public GfxRegHelper
{
public:
    SqImgSampRegHelper(Builder* pBuilder, Value* pRegister, GfxIpVersion* pGfxIpVersion);

    // Acquire register value
    Value* GetReg(SqSampRegs id);

    // Set register value
    void SetReg(SqSampRegs id, Value* pParam);
};

} // lgc
