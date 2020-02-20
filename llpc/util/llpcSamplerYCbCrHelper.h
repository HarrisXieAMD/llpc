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
  * @file  llpcSamplerYCbCrHelper.h
  * @brief LLPC header file: contains the definition of LLPC utility class SamplerYCbCrHelper.
  ***********************************************************************************************************************
  */

#pragma once
#include "llpc.h"
#include "llpcDebug.h"

namespace Llpc
{
// Represents the type of sampler filter.
enum class SamplerFilter
{
    Nearest = 0,
    Linear,
};

// Represents the YCbCr conversion model.
enum class SamplerYCbCrModelConversion : uint32_t
{
    RgbIdentity = 0,
    YCbCrIdentity,
    YCbCr709,
    YCbCr601,
    YCbCr2020,
};

// Represents whether color channels are encoded using the full range of numerical
// values or whether values are reserved for headroom and foot room.
enum class SamplerYCbCrRange
{
    ItuFull = 0,
    ItuNarrow,
};

// Represents the location of downsampled chroma channel samples relative to the luma samples.
enum class ChromaLocation
{
    CositedEven = 0,
    Midpoint,
};

// Represents the component values placed in each component of the output vector.
class ComponentSwizzle
{
public:
    enum Value: uint32_t
    {
        Zero = 0,
        One  = 1,
        R    = 4,
        G    = 5,
        B    = 6,
        A    = 7,
    };

    ComponentSwizzle() = default;
    ComponentSwizzle(const Value swizzle) : m_value(swizzle), m_channel(swizzle - Value::R) {};
    ComponentSwizzle(const uint32_t swizzle)
    {
        LLPC_ASSERT((swizzle >= Value::Zero) && (swizzle <= Value::A));
        m_value = static_cast<Value>(swizzle);
        m_channel = (m_value - Value::R);
    }

    operator Value() const { return m_value; }

    ComponentSwizzle& operator=(uint32_t op)
    {
        LLPC_ASSERT((op >= Value::Zero) && (op <= Value::A));
        m_value = static_cast<Value>(op);
        return *this;
    }
    constexpr bool operator!=(ComponentSwizzle::Value op) const { return m_value != op; }
    constexpr bool operator==(ComponentSwizzle::Value op) const { return m_value == op; }

    const uint32_t GetChannel() const { return m_channel; }

private:
    Value m_value;
    uint32_t m_channel;
};

// =====================================================================================================================
// Represents LLPC sampler yCbCr conversion helper class
class SamplerYCbCrHelper
{
public:

};

} // Llpc
