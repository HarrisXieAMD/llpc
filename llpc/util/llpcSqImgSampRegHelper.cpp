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
* @file  llpcSqImgSampRegHelper.cpp
* @brief LLPC source file: Implementation of LLPC utility class SqImgSampRegHelper
***********************************************************************************************************************
*/
#include "llpcSqImgSampRegHelper.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace Llpc;
using namespace llvm;

Value* Llpc::SqImgSampRegHelperGfx9::GetFilterMode()
{
    return GetBits(m_filterModeBitsInfo);
}

void Llpc::SqImgSampRegHelperGfx9::SetFilterMode(Value* pFilterMode)
{
    SetBits(m_filterModeBitsInfo, pFilterMode);
}

Value* Llpc::SqImgSampRegHelperGfx9::GetXYMagFilter()
{
    return GetBits(m_xyMagFilterBitsInfo);
}

void Llpc::SqImgSampRegHelperGfx9::SetXYMagFilter(Value* pXYMagFilter)
{
    SetBits(m_xyMagFilterBitsInfo, pXYMagFilter);
}

Value* Llpc::SqImgSampRegHelperGfx9::GetXYMinFilter()
{
    return GetBits(m_xyMinFilterBitsInfo);
}

void Llpc::SqImgSampRegHelperGfx9::SetXYMinFilter(Value* pXYMinFilter)
{
    SetBits(m_xyMinFilterBitsInfo, pXYMinFilter);
}
