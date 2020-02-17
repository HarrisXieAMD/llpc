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
* @file  llpcSqImgRsrcRegHelper.cpp
* @brief LLPC source file: Implementation of LLPC utility class SqImgRsrcRegHelper
***********************************************************************************************************************
*/
#include "llpcSqImgRsrcRegHelper.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace Llpc;
using namespace llvm;

template<typename T>
Value* SqImgRsrcRegHelper<T>::GetRegCommon(BitsState* pBitsState, uint32_t id)
{
    if ((pBitsState[id].pValue == nullptr) ||
        (pBitsState[id].isModified == 1))
    {
        pBitsState[id].pValue = GetBits(pBitsState[id].info);
    }
    
    pBitsState[id].isModified = 0;
    
    return pBitsState[id].pValue;
}

template<typename T>
void SqImgRsrcRegHelper<T>::SetRegCommon(BitsState* pBitsState, uint32_t id, Value* pVal)
{
    Derived()->SetBits(pBitsState[id].info, pVal);
    pBitsState[id].isModified = 1;
}

Value* SqImgRsrcRegHelperGfx9::GetReg(SqRsrcRegs9 id)
{
    return GetRegCommon(m_bitsStateList, id);
}

void SqImgRsrcRegHelperGfx9::SetReg(SqRsrcRegs9 id, Value* pVal)
{
    SetRegCommon(m_bitsStateList, id, pVal);
}

Value* SqImgRsrcRegHelperGfx9::GetParam(SqRsrcRegs id)
{
    switch (id)
    {
    case Llpc::BaseAddress:
        return GetReg(SqRsrcRegs9::BaseAddress);
    case Llpc::BaseAddressHi:
        return GetReg(SqRsrcRegs9::BaseAddressHi);
    case Llpc::Format:
        return GetReg(SqRsrcRegs9::Format);
    case Llpc::Width:
        return m_pBuilder->CreateAdd(GetReg(SqRsrcRegs9::Width), m_pInt32One);
    case Llpc::Height:
        return m_pBuilder->CreateAdd(GetReg(SqRsrcRegs9::Height), m_pInt32One);
    case Llpc::DstSelXYZW:
        return GetReg(SqRsrcRegs9::DstSelXYZW);
    case Llpc::IsTileOpt:
        return m_pBuilder->CreateICmpNE(GetReg(SqRsrcRegs9::IsTileOpt), m_pBuilder->getInt32(0));
    case Llpc::Depth:
        return GetReg(SqRsrcRegs9::Depth);
    case Llpc::Pitch:
         return m_pBuilder->CreateAdd(GetReg(SqRsrcRegs9::Pitch), m_pInt32One);
    case Llpc::BcSwizzle:
        return GetReg(SqRsrcRegs9::BcSwizzle);
    default:
        break;
    }

    return nullptr;
}

void SqImgRsrcRegHelperGfx9::SetParam(SqRsrcRegs id, Value* pVal)
{
    switch (id)
    {
    case Llpc::BaseAddress:
        SetReg(SqRsrcRegs9::BaseAddress, pVal);
        break;
    case Llpc::BaseAddressHi:
        SetReg(SqRsrcRegs9::BaseAddressHi, pVal);
        break;
    case Llpc::Format:
        SetReg(SqRsrcRegs9::Format, pVal);
        break;
    case Llpc::Width:
        SetReg(SqRsrcRegs9::Width, m_pBuilder->CreateSub(pVal, m_pInt32One));
        break;
    case Llpc::Height:
        SetReg(SqRsrcRegs9::Height, m_pBuilder->CreateSub(pVal, m_pInt32One));
        break;
    case Llpc::DstSelXYZW:
        SetReg(SqRsrcRegs9::DstSelXYZW, pVal);
        break;
    case Llpc::IsTileOpt:
        llvm_unreachable("Set \"IsTileOpt\" is not allowed!");
        break;
    case Llpc::Depth:
        SetReg(SqRsrcRegs9::Depth, pVal);
        break;
    case Llpc::Pitch:
        SetReg(SqRsrcRegs9::Pitch, m_pBuilder->CreateSub(pVal, m_pInt32One));
        break;
    case Llpc::BcSwizzle:
        SetReg(SqRsrcRegs9::BcSwizzle, pVal);
        break;
    default:
        break;
    }
}

Value* Llpc::SqImgRsrcRegHelperGfx10::GetParam(SqRsrcRegs id)
{
    switch (id)
    {
    case Llpc::BaseAddress:
        return GetReg(SqRsrcRegs10::BaseAddress);
    case Llpc::BaseAddressHi:
        return GetReg(SqRsrcRegs10::BaseAddressHi);
    case Llpc::Format:
        return GetReg(SqRsrcRegs10::Format);
    case Llpc::Width:
        return m_pBuilder->CreateAdd(GetRegCombine(SqRsrcRegs10::WidthLo, SqRsrcRegs10::WidthHi), m_pInt32One);
    case Llpc::Height:
        return m_pBuilder->CreateAdd(GetReg(SqRsrcRegs10::Height), m_pInt32One);
    case Llpc::DstSelXYZW:
        return GetReg(SqRsrcRegs10::DstSelXYZW);
    case Llpc::IsTileOpt:
        return m_pBuilder->CreateICmpNE(GetReg(SqRsrcRegs10::IsTileOpt), m_pBuilder->getInt32(0));
    case Llpc::Depth:
        return GetReg(SqRsrcRegs10::Depth);
    case Llpc::Pitch:
        break;
    case Llpc::BcSwizzle:
        return GetReg(SqRsrcRegs10::BcSwizzle);
    default:
        break;
    }
 
    return nullptr;
}

void Llpc::SqImgRsrcRegHelperGfx10::SetParam(SqRsrcRegs id, Value* pVal)
{
    switch (id)
    {
    case Llpc::BaseAddress:
        SetReg(SqRsrcRegs10::BaseAddress, pVal);
        break;
    case Llpc::BaseAddressHi:
        SetReg(SqRsrcRegs10::BaseAddressHi, pVal);
        break;
    case Llpc::Format:
        SetReg(SqRsrcRegs10::Format, pVal);
        break;
    case Llpc::Width:
        SetRegCombine(SqRsrcRegs10::WidthLo, SqRsrcRegs10::WidthHi, m_pBuilder->CreateSub(pVal, m_pInt32One));
        break;
    case Llpc::Height:
        SetReg(SqRsrcRegs10::Height, m_pBuilder->CreateSub(pVal, m_pInt32One));
        break;
    case Llpc::DstSelXYZW:
        SetReg(SqRsrcRegs10::DstSelXYZW, pVal);
        break;
    case Llpc::IsTileOpt:
        llvm_unreachable("Set \"IsTileOpt\" is not allowed!");
        break;
    case Llpc::Depth:
        SetReg(SqRsrcRegs10::Depth, pVal);
        break;
    case Llpc::Pitch:
        break;
    case Llpc::BcSwizzle:
        SetReg(SqRsrcRegs10::BcSwizzle, pVal);
        break;
    default:
        break;
    }
}

Value* Llpc::SqImgRsrcRegHelperGfx10::GetReg(SqRsrcRegs10 id)
{
    return GetRegCommon(m_bitsStateList, id);
}

void Llpc::SqImgRsrcRegHelperGfx10::SetReg(SqRsrcRegs10 id, Value* pVal)
{
    SetRegCommon(m_bitsStateList, id, pVal);
}

// =====================================================================================================================
// Get combined data from two seperate DWords
// Note: this function only supports 32 bits range
Value* Llpc::SqImgRsrcRegHelperGfx10::GetRegCombine(SqRsrcRegs10 idLo, SqRsrcRegs10 idHi)
{
    Value* pDataLo = GetReg(SqRsrcRegs10::WidthLo);
    Value* pDataHi = GetReg(SqRsrcRegs10::WidthHi);
    return m_pBuilder->CreateOr(m_pBuilder->CreateShl(pDataHi, m_bitsStateList[idLo].info.offset), pDataLo);
}

// =====================================================================================================================
// Set data into two seperate DWords
// Note: this function only supports 32 bits range
void Llpc::SqImgRsrcRegHelperGfx10::SetRegCombine(SqRsrcRegs10 idLo, SqRsrcRegs10 idHi, Value* pData)
{
    Value* pDataLo = m_pBuilder->CreateIntrinsic(Intrinsic::amdgcn_ubfe,
                                                m_pBuilder->getInt32Ty(),
                                                { pData,
                                                  m_pBuilder->getInt32(0),
                                                  m_pBuilder->getInt32(m_bitsStateList[idLo].info.offset) });

    Value* pDataHi = m_pBuilder->CreateLShr(pData, m_pBuilder->getInt32(m_bitsStateList[idLo].info.offset));

    SetReg(idLo, pDataLo);
    SetReg(idHi, pDataHi);
}
