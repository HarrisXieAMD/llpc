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
  * @file  llpcGfxRegHandler.h
  * @brief LLPC header file: contains the definition of LLPC utility class GfxIpRegHandlerBase.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpcBuilderImpl.h"
#include "llpcPipelineState.h"

namespace Llpc
{

using namespace llvm;

// General bits info for indexed DWORD
struct BitsInfo
{
    uint32_t index;
    uint32_t offset;
    uint32_t count;
};

// =====================================================================================================================
// Base helper class for handling GfxIp registers
class GfxIpRegHandlerBase
{
public:
    // Set register
    void SetRegister(Value* pNewRegister);

    // Get register
    Value* GetRegister();

protected:
    // Constructor
    GfxIpRegHandlerBase(Builder* pBuilder, Value* pRegister)
    {
        m_pBuilder = pBuilder;
        SetRegister(pRegister);
    }

    // Return new DWord with replacing the specific range of data in old DWord
    Value* ReplaceBits(Value* pDWord, uint32_t offset, uint32_t count, Value* pNewBits);

    // Return the size of registered DWords
    inline uint32_t GetDWordsCount() { return m_pDWords.size(); }
    
    // Get indexed DWORD
    inline Value* GetDWord(uint32_t index)
    {
        ExtractDWordIfNecessary(index);
        return m_pDWords[index];
    }
 
    // Set indexed DWORD
    inline void SetDWord(uint32_t index, Value* pDWord)
    {
        // Set the whole 32bits data
        m_pDWords[index] = pDWord;
        // If assign successfully, set corresponding mask bit to 1
        m_dirtyDWords |= 1 << index;
    }

    // Return if the specific DWord is modified or not
    inline bool IsDWordModified(uint32_t index)
    {
        return (m_dirtyDWords & (1 << index));
    }
 
    // Get data from a range of bits in indexed DWORD according to BitsInfo
    Value* GetBits(const BitsInfo& bitsInfo);

    // Set data to a range of bits in indexed DWORD according to BitsInfo
    void SetBits(const BitsInfo& bitsInfo, Value* pData);
    
protected:
    Builder*    m_pBuilder;

private:
    // Load indexed DWORD from <n x i32> vector is pDWord is nullptr
    inline void ExtractDWordIfNecessary(uint32_t index)
    {
        if (m_pDWords[index] == nullptr)
        {
            m_pDWords[index] = m_pBuilder->CreateExtractElement(m_pRegister, m_pBuilder->getInt64(index));
        }
    }

    // Contains (possibly updated) dwords for the register value. Each element will be a nullptr until it
    // is requested or updated for the first time.
    SmallVector<Value*, 8> m_pDWords;
 
    // Combined <n x i32> vector containing the register value, which does not yet reflect the dwords
    // that are marked as dirty.
    Value* m_pRegister;
    
    // Bit-mask of dwords whose value was changed but is not yet reflected in m_pRegister.
    uint32_t m_dirtyDWords;
};

} // Llpc
