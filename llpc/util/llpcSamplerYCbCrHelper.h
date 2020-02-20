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
#include "llpcBuilderImpl.h"

namespace Llpc
{

using namespace llvm;

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
        m_channel = (m_value - Value::R);
        return *this;
    }
    constexpr bool operator!=(ComponentSwizzle::Value op) const { return m_value != op; }
    constexpr bool operator==(ComponentSwizzle::Value op) const { return m_value == op; }

    const uint32_t GetChannel() const { return m_channel; }

private:
    Value m_value;
    uint32_t m_channel;
};

// General YCbCr sample info
struct YCbCrSampleInfo
{
    Type*              pResultTy;
    uint32_t           dim;
    uint32_t           flags;
    Value*             pImageDesc;
    Value*             pSamplerDesc;
    ArrayRef<Value*>   address;
    const std::string& instNameStr;
    bool               isSample;
};

// YCbCr sample info for downsampled chroma channels in the X dimension
struct XChromaSampleInfo
{
    YCbCrSampleInfo*   pYCbCrInfo;
    Value*             pImageDesc1;
    Value*             pI;
    Value*             pJ;
    Value*             pChromaWidth;
    Value*             pChromaHeight;
    ChromaLocation     xChromaOffset;
};

// YCbCr sample info for downsampled chroma channels in both X and Y dimension
struct XYChromaSampleInfo : XChromaSampleInfo
{
    Value*             pImageDesc2;
    uint32_t           planeNum;
    ChromaLocation     yChromaOffset;
};

// YCbCr wrapped sample info
struct YCbCrWrappedSampleInfo : XYChromaSampleInfo
{
    Value*             pImageDesc3;
    bool               subsampledX;
    bool               subsampledY;
};

// =====================================================================================================================
// Represents LLPC sampler YCbCr conversion helper class
class SamplerYCbCrHelper
{
public:
    // Register Builder
    inline void Register(BuilderImplImage* pBuilder) { m_pBuilder = pBuilder; }

    // Unregister Builder
    inline void UnRegister() { m_pBuilder = nullptr; }

    SamplerYCbCrHelper() = default;

    void SetYCbCrConversionMetaData(SamplerYCbCrConversionMetaData& yCbCrMetaData);

    void SetSamplerDescLuma(Value*  pSamplerDescLuma);
    
    void SetSamplerDescChroma(Value* pSamplerDescChroma);

    void SetImageDescLuma(Value*  pImgDescLuma);

    void SetImageDescChroma(ArrayRef<Value*> pImgDescChromas);
    
    void GenSamplerDescChroma();

    void GenImgDescChroma();
    
    Value* ConvertColorSpace();

    void SampleYCbCrData(YCbCrSampleInfo& yCbCrSampleInfo);

    void SetCoord(Value* pS,
                  Value* pT);

    inline void SetSampleInfoLuma(Type* pResultType) { m_pResultType = pResultType; }

    // Replace [beginBit, beginBit + adjustBits) bits with pData in specific pWord
    Value* ReplaceBitsInWord(Value*   pWord,
                             uint32_t beginBit,
                             uint32_t adjustBits,
                             Value*   pData);

    // Implement transfer from ST coordinates to UV coordiantes operation
    Value* TransferSTtoUVCoords(Value* pST,
                                Value* pSize);

    // Implement the adjustment of UV coordiantes when the sample location associated with
    // downsampled chroma channels in the X/XY dimension occurs
    Value* YCbCrCalculateImplicitChromaUV(ChromaLocation offset,
                                          Value*         pUV);

    // Transfer IJ coordinates from UV coordinates
    Value* TransferUVtoIJCoords(SamplerFilter filter,
                                Value*        pUV);

    // Calculate UV offset to top-left pixel
    Value* CalculateUVoffset(Value* pUV);

    // Implement bilinear blending
    Value* BilinearBlend(Value* pAlpha,
                         Value* pBeta,
                         Value* pTL,
                         Value* pTR,
                         Value* pBL,
                         Value* pBR);

    // Implement wrapped YCbCr sample
    Value* YCbCrWrappedSample(YCbCrWrappedSampleInfo& wrapInfo);

    // Implement reconstructed YCbCr sample operation for downsampled chroma channels in the X dimension
    Value* YCbCrReconstructLinearXChromaSample(XChromaSampleInfo& xChromaInfo);

    // Implement reconstructed YCbCr sample operation for downsampled chroma channels in both X and Y dimension
    Value* YCbCrReconstructLinearXYChromaSample(XYChromaSampleInfo& xyChromaInfo);

    // Implement interanl image sample for YCbCr conversion
    Value* YCbCrCreateImageSampleInternal(SmallVectorImpl<Value*>& coords,
                                          YCbCrSampleInfo*         pYCbCrInfo);

    // Generate sampler descriptor for YCbCr conversion
    Value* YCbCrGenerateSamplerDesc(Value*        pSamplerDesc,
                                    SamplerFilter filter,
                                    bool          forceExplicitReconstruction);

    // Implement range expanding operation on checking whether the encoding uses full numerical range on chroma channel
    Value* YCbCrRangeExpand(SamplerYCbCrRange  range,
                            const uint32_t*    pBits,
                            Value*             pSample);

    // Implement the color transfer operation for conversion from YCbCr to RGB color model
    Value* YCbCrConvertColor(Type*                       pResultTy,
                             SamplerYCbCrModelConversion colorModel,
                             SamplerYCbCrRange           range,
                             uint32_t*                   pBits,
                             Value*                      pImageOp);

private:
    SamplerYCbCrConversionMetaData* m_pMetaData;
    BuilderImplImage*           m_pBuilder                ;
    int                         m_planeNum                ;
    bool                        m_subsampledX             ;
    bool                        m_subsampledY             ;
    bool                        m_forceExplicitReconstruct;
    bool                        m_tileOptimal;
    uint32_t                    m_xBitCount               ;
    ComponentSwizzle            m_swizzleR                ;
    ComponentSwizzle            m_swizzleG                ;
    ComponentSwizzle            m_swizzleB                ;
    ComponentSwizzle            m_swizzleA                ;
    SamplerFilter               m_lumaFilter              ;
    SamplerFilter               m_chromaFilter            ;
    ChromaLocation              m_xChromaOffset           ;
    ChromaLocation              m_yChromaOffset           ;
    SamplerYCbCrRange           m_yCbCrRange              ;
    SamplerYCbCrModelConversion m_yCbCrModel              ;
    uint32_t m_bits[3];

    Value* m_pWidth;
    Value* m_pHeight;

    Value* m_pS;
    Value* m_pT;

    Value* m_pU;
    Value* m_pV;
    Value* m_pI;
    Value* m_pJ;

    Value* m_pSamplerDescLuma;
    Value* m_pImgDescLuma;
    Value* m_pSamplerDescChroma;
    std::vector<Value*> m_pImgDescChromas;

    Value* m_pYCbCrData;
    Value* m_pRGBAData;

    Type* m_pResultType;
};

} // Llpc
