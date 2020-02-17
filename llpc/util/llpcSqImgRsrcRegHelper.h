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
  * @file  llpcSqImgRsrcRegHelper.h
  * @brief LLPC header file: contains the definition of LLPC utility class SqImgRsrcRegHelper.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcGfxRegHandler.h"

namespace Llpc
{

using namespace llvm;

struct BitsState
{
    BitsInfo     info;
    Value*       pValue;
    uint32_t     isModified;
};

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

template<typename T>
class SqImgRsrcRegHelper : public GfxIpRegHandlerBase
{
public:
    SqImgRsrcRegHelper(Builder*    pBuilder,
                       Value*      pRegister):GfxIpRegHandlerBase(pBuilder, pRegister)
    {
        m_pInt32One = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);
    }

    inline Value* GetParam(SqRsrcRegs id)
    {
        return Derived()->GetParam(id);
    }

    inline void SetParam(SqRsrcRegs id, Value* pVal)
    {
        Derived()->SetParam(id, pVal);
    }

protected:
    Value* GetRegCommon(BitsState* pBitsState , uint32_t id);
    void SetRegCommon(BitsState* pBitsState, uint32_t id, Value*);
    Value* m_pInt32One;

private:
    inline T* Derived() { return static_cast <T*>(this); }
};

class SqImgRsrcRegHelperGfx9 : public SqImgRsrcRegHelper<SqImgRsrcRegHelperGfx9>
{
public:
    enum SqRsrcRegs9 : uint32_t
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

    SqImgRsrcRegHelperGfx9(Builder*    pBuilder,
                           Value*      pRegister):SqImgRsrcRegHelper(pBuilder, pRegister) {}

    Value* GetParam(SqRsrcRegs id) ;
    void SetParam(SqRsrcRegs id, Value*) ;

protected:
    Value* GetReg(SqRsrcRegs9 id);
    void SetReg(SqRsrcRegs9 id, Value*);

    BitsState m_bitsStateList[SqRsrcRegs9::EnumCount] =
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
};

class SqImgRsrcRegHelperGfx10 : public SqImgRsrcRegHelper<SqImgRsrcRegHelperGfx10>
{
public:
    enum SqRsrcRegs10 : uint32_t
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

    SqImgRsrcRegHelperGfx10(Builder* pBuilder,
        Value* pRegister) :SqImgRsrcRegHelper(pBuilder, pRegister){}

    Value* GetParam(SqRsrcRegs id) ;
    void SetParam(SqRsrcRegs id, Value* pVal);

private:
    Value* GetReg(SqRsrcRegs10 id);
    void SetReg(SqRsrcRegs10 id,
                Value*       pVal);

    // Get combined data from two seperate DWords
    Value* GetRegCombine(SqRsrcRegs10 idLo,
                         SqRsrcRegs10 idHi);

    // Set data into two seperate DWords
    void SetRegCombine(SqRsrcRegs10 idLo,
                       SqRsrcRegs10 idHi,
                       Value*       pData);

    BitsState m_bitsStateList[SqRsrcRegs10::EnumCount] =
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
};

} // Llpc
