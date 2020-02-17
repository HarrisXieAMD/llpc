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
* @file  llpcGfxRegHandler.cpp
* @brief LLPC source file: Implementation of LLPC utility class llpcGfxRegHandler
***********************************************************************************************************************
*/
#include "llpcGfxRegHandler.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"
#include "llpcTargetInfo.h"

using namespace Llpc;
using namespace llvm;

// =====================================================================================================================
// Set register:
//   - Clear old DWords vector
//   - Fill DWords vector with nullptr
//   - Init dirty mask to all clean
void GfxIpRegHandlerBase::SetRegister(
    Value*    pNewRegister) // [in] A <n x i32> vector
{
    assert(pNewRegister->getType()->isIntOrIntVectorTy());

    if (auto pVectorTy = dyn_cast<VectorType>(pNewRegister->getType()))
    {
        uint32_t count = pVectorTy->getNumElements();

        // Clear previously stored DWords
        m_pDWords.clear();

        // Resize to specific number of DWords
        for (uint32_t i = 0; i < count; i++)
        {
            //m_pDWords.push_back(m_pBuilder->CreateExtractElement(pNewRegister, m_pBuilder->getInt64(i)));
            m_pDWords.push_back(nullptr);
        }
    }
    else
    {
        assert(pNewRegister->getType() == m_pBuilder->getInt32Ty());
        m_pDWords.push_back(nullptr);
    }

    m_pRegister = pNewRegister;
    m_dirtyDWords = 0u;
}

// =====================================================================================================================
// Get register
//   - Overwrite DWords in <n x i32>register if marked as dirty
Value* GfxIpRegHandlerBase::GetRegister()
{
    uint32_t dirtyMask = m_dirtyDWords;

    // Overwrite if the specific DWord is being masked as dirty
    for (uint32_t i = 0; dirtyMask > 0; dirtyMask >>= 1)
    {
        if (dirtyMask & 1)
        {
            m_pRegister = m_pBuilder->CreateInsertElement(m_pRegister,
                                                          m_pDWords[i],
                                                          m_pBuilder->getInt64(i));
        }

        i++;
    }

    // Set mask as all clean since we've update the registered <nxi32>
    m_dirtyDWords = 0u;

    // Just return updated m_pRegister
    return m_pRegister;
}

// =====================================================================================================================
// Get data from a range of bits in indexed DWord according to BitsInfo
Value* GfxIpRegHandlerBase::GetBits(
    const BitsInfo&    bitsInfo)    // [in] The BitsInfo of data
{
    if (bitsInfo.count == 32)
    {
        return GetDWord(bitsInfo.index);
    }

    ExtractDWordIfNecessary(bitsInfo.index);

    return m_pBuilder->CreateIntrinsic(Intrinsic::amdgcn_ubfe,
                                       m_pBuilder->getInt32Ty(),
                                       { m_pDWords[bitsInfo.index],
                                         m_pBuilder->getInt32(bitsInfo.offset),
                                         m_pBuilder->getInt32(bitsInfo.count) });
}

// =====================================================================================================================
// Set data to a range of bits in indexed DWord according to BitsInfo
void GfxIpRegHandlerBase::SetBits(
    const BitsInfo&  bitsInfo,    // [in] The BitsInfo of data's high part
    Value*           pData)       // [in] Data using for set
{
    ExtractDWordIfNecessary(bitsInfo.index);

    if (bitsInfo.count != 32)
    {
        Value* pDWordsNew = ReplaceBits(m_pDWords[bitsInfo.index],
                                        bitsInfo.offset,
                                        bitsInfo.count,
                                        pData);
        SetDWord(bitsInfo.index, pDWordsNew);
    }
    else
    {
        SetDWord(bitsInfo.index, pData);
    }
}

// =====================================================================================================================
// Return new DWord which is replaced [offset, offset + count) with pNewBits
Value* GfxIpRegHandlerBase::ReplaceBits(
    Value*    pDWord,   // [in] Target DWORD
    uint32_t  offset,   // The first bit to be replaced
    uint32_t  count,    // The number of bits should be replaced
    Value*    pNewBits) // [in] The data used to replace specific bits
{
    // mask = ((1 << count) - 1) << offset
    // Result = (pDWord & ~mask)|((pNewBits << offset) & mask)
    uint32_t mask = ((1 << count) - 1) << offset;
    Value* pMask = m_pBuilder->getInt32(mask);
    Value* pInvMask = m_pBuilder->getInt32(~mask);
    Value* pBeginBit = m_pBuilder->getInt32(offset);
    pNewBits = m_pBuilder->CreateAnd(m_pBuilder->CreateShl(pNewBits, pBeginBit), pMask);
    return m_pBuilder->CreateOr(m_pBuilder->CreateAnd(pDWord, pInvMask), pNewBits);
}