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
  * @file  llpcSqImgSampRegHelper.h
  * @brief LLPC header file: contains the definition of LLPC utility class SqImgSampRegHelper.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcGfxRegHandler.h"

namespace Llpc
{

using namespace llvm;

class SqImgSampRegHelper : public GfxIpRegHandlerBase
{
public:
    SqImgSampRegHelper(Builder* pBuilder,
        Value* pRegister) :GfxIpRegHandlerBase(pBuilder, pRegister) {}
    virtual Value* GetFilterMode() = 0;
    virtual void SetFilterMode(Value*) = 0;

    virtual Value* GetXYMagFilter() = 0;
    virtual void SetXYMagFilter(Value*) = 0;

    virtual Value* GetXYMinFilter() = 0;
    virtual void SetXYMinFilter(Value*) = 0;
};

class SqImgSampRegHelperGfx9 : public SqImgSampRegHelper
{
public:
    SqImgSampRegHelperGfx9(Builder* pBuilder,
        Value* pRegister) :SqImgSampRegHelper(pBuilder, pRegister) {}

    Value* GetFilterMode();
    void SetFilterMode(Value*);

    Value* GetXYMagFilter();
    void SetXYMagFilter(Value*);

    Value* GetXYMinFilter();
    void SetXYMinFilter(Value*);

private:
    BitsInfo m_filterModeBitsInfo  = { 0, 30, 2 };
    BitsInfo m_xyMagFilterBitsInfo = { 2, 20, 2 };
    BitsInfo m_xyMinFilterBitsInfo = { 2, 22, 2 };
};

} // Llpc
