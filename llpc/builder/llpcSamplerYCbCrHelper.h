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
#include "llpcSqImgRsrcRegHelper.h"
#include "llpc.h"

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
        assert((swizzle >= Value::Zero) && (swizzle <= Value::A));
        m_value = static_cast<Value>(swizzle);

        m_channel = (swizzle >= Value::R) ? (m_value - Value::R) : (m_value + Value::R);
    }

    operator Value() const { return m_value; }

    ComponentSwizzle& operator=(uint32_t op)
    {
        assert((op >= Value::Zero) && (op <= Value::A));
        m_value = static_cast<Value>(op);
        m_channel = (op >= Value::R) ? (m_value - Value::R) : (m_value + Value::R);
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

template<typename T>
class YCbCrAddressHelper
{
public:
    YCbCrAddressHelper(Builder* pBuilder, T* pSqImgRsrcRegHelper)
    {
        m_pBuilder = pBuilder;
        m_pSqImgRsrcRegHelper = pSqImgRsrcRegHelper;
        m_PlaneBaseAddresses.clear();
        m_pInt32One = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);
    }
    void GenBaseAddress(uint32_t planeNum);
    void GenHeightAndPitch(uint32_t bits, uint32_t bpp, uint32_t xBitCount, bool isTileOptimal, uint32_t planeNum);

    Value* Power2Align(Value* x, uint32_t align);

    Value* GetPlane(uint32_t idx) { return m_PlaneBaseAddresses[idx]; }
    Value* GetPitchY() { return m_pPitchY; }
    Value* GetPitchCb() { return m_pPitchCb; }
    Value* GetHeightY() { return m_pHeightY; }
    Value* GetHeightCb() { return m_pHeightCb; }

private:
    T* m_pSqImgRsrcRegHelper;
    Builder* m_pBuilder;
    SmallVector<Value*, 3> m_PlaneBaseAddresses;
    Value* m_pPitchY;
    Value* m_pHeightY;
    Value* m_pPitchCb;
    Value* m_pHeightCb;
    Value* m_pInt32One;

};

template<typename T>
inline void YCbCrAddressHelper<T>::GenBaseAddress(uint32_t planeNum)
{
    switch (planeNum)
    {
    case 1:
        m_PlaneBaseAddresses.push_back(m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::BaseAddress));
        break;
    case 2:
        m_PlaneBaseAddresses.push_back(m_pBuilder->CreateAdd(m_PlaneBaseAddresses[0],
            m_pBuilder->CreateLShr(m_pBuilder->CreateMul(m_pPitchY, m_pHeightY),
                m_pBuilder->getInt32(8))));
        break;
    case 3:
        m_PlaneBaseAddresses.push_back(m_pBuilder->CreateAdd(m_PlaneBaseAddresses[1],
            m_pBuilder->CreateLShr(m_pBuilder->CreateMul(m_pPitchCb, m_pHeightCb),
                m_pBuilder->getInt32(8))));
        break;
    default:
        //assert
        break;
    }
}

template<>
inline void YCbCrAddressHelper<SqImgRsrcRegHelperGfx9>::GenHeightAndPitch(uint32_t bits, uint32_t bpp, uint32_t xBitCount, bool isTileOptimal, uint32_t planeNum)
{
    Value* pHeight = m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::Height);
    Value* pHeightR1 = m_pBuilder->CreateLShr(pHeight, m_pInt32One);
    m_pHeightY = pHeight;
    m_pHeightCb = pHeightR1;

    Value* pPitch = m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::Pitch);
    Value* pPitchR1 = m_pBuilder->CreateLShr(pPitch, m_pInt32One);
    Value* pPitchR1M1 = m_pBuilder->CreateSub(pPitchR1, m_pInt32One);

    // pPitchY * (m_xBitCount >> 3)
    m_pPitchY = m_pBuilder->CreateMul(pPitch, m_pBuilder->CreateLShr(m_pBuilder->getInt32(xBitCount), 3));

    // pPitchCb = pPitchCb * (m_xBitCount >> 3)
    m_pPitchCb = m_pBuilder->CreateMul(pPitchR1, m_pBuilder->CreateLShr(m_pBuilder->getInt32(xBitCount), 3));

    if (isTileOptimal)
    {
        Value* pIsTileOpt = m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::IsTileOpt);

        Value* pPtchYOpt = m_pBuilder->CreateMul(pPitch, m_pBuilder->CreateLShr(m_pBuilder->getInt32(bits), 3));
        // pPitchY = pIsTileOpt ? (pPtchYOpt << 5) : pPitchY
        m_pPitchY = m_pBuilder->CreateSelect(pIsTileOpt, m_pBuilder->CreateShl(pPtchYOpt, m_pBuilder->getInt32(5)), m_pPitchY);

        // pPitchCbOpt = pPitchCb * (m_bits[0] >> 3)
        Value* pPitchCbOpt = m_pBuilder->CreateMul(pPitchR1, m_pBuilder->CreateLShr(m_pBuilder->getInt32(bits), 3));
        // pPitchCb = pIsTileOpt ? (pPitchCbOpt << 5) : pPitchCb
        m_pPitchCb = m_pBuilder->CreateSelect(pIsTileOpt, m_pBuilder->CreateShl(pPitchCbOpt, m_pBuilder->getInt32(5)), m_pPitchCb);
    }
}

template<>
inline void YCbCrAddressHelper<SqImgRsrcRegHelperGfx10>::GenHeightAndPitch(uint32_t bits, uint32_t bpp, uint32_t xBitCount, bool isTileOptimal, uint32_t planeNum)
{
    const uint32_t elementBytes = bpp >> 3;
    const uint32_t pitchAlign = (256 / elementBytes);

    Value* pHeight = m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::Height);
    m_pHeightY = pHeight;

    Value* pWidth = m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::Width);
    m_pPitchY = Power2Align(pWidth, pitchAlign);
    // pPitchY = pPitchY * pElementBytes
    m_pPitchY = m_pBuilder->CreateMul(m_pPitchY, m_pBuilder->getInt32(elementBytes));

    Value* pHeightR1 = m_pBuilder->CreateLShr(pHeight, m_pInt32One);
    m_pHeightCb = pHeightR1;

    Value* pWidthR1 = m_pBuilder->CreateLShr(pWidth, m_pInt32One);
    m_pPitchCb = Power2Align(pWidthR1, pitchAlign);
    // pPitchCb = pPitchCb * pElementBytes
    m_pPitchCb = m_pBuilder->CreateMul(m_pPitchCb, m_pBuilder->getInt32(elementBytes));

    if (isTileOptimal)
    {
        const uint32_t log2BlkSize = 16;
        const uint32_t log2EleBytes = log2(bpp >> 3);
        const uint32_t log2NumEle = log2BlkSize - log2EleBytes;
        const bool widthPrecedent = 1;
        const uint32_t log2Width = (log2NumEle + (widthPrecedent ? 1 : 0)) / 2;
        const uint32_t pitchAlignOpt = 1u << log2Width;
        const uint32_t heightAlignOpt = 1u << (log2NumEle - log2Width);

        // pPitchY = pPitchY * pElementBytes
        Value* pPtchYOpt = m_pBuilder->CreateMul(Power2Align(pWidth, pitchAlignOpt), m_pBuilder->getInt32(elementBytes));

        // pPitchCb = pPitchCb * pElementBytes
        Value* pPitchCbOpt = m_pBuilder->CreateMul(Power2Align(pWidthR1, pitchAlignOpt), m_pBuilder->getInt32(elementBytes));

        Value* pIsTileOpt = m_pSqImgRsrcRegHelper->GetParam(SqRsrcRegs::IsTileOpt);
        m_pPitchY = m_pBuilder->CreateSelect(pIsTileOpt, pPtchYOpt, m_pPitchY);
        m_pHeightY = m_pBuilder->CreateSelect(pIsTileOpt, Power2Align(pHeight, heightAlignOpt), pHeight);

        m_pPitchCb = m_pBuilder->CreateSelect(pIsTileOpt, pPitchCbOpt, m_pPitchCb);
        m_pHeightCb = m_pBuilder->CreateSelect(pIsTileOpt, Power2Align(pHeightR1, heightAlignOpt), pHeightR1);
    }
}

template<typename T>
inline Value* YCbCrAddressHelper<T>::Power2Align(Value* x, uint32_t align)
{
    uint32_t alignM1 = align - 1;
    uint32_t alignM1Inv = ~alignM1;
    Value* pResult = m_pBuilder->CreateAdd(x, m_pBuilder->getInt32(alignM1));
    return m_pBuilder->CreateAnd(pResult, m_pBuilder->getInt32(alignM1Inv));
}

// =====================================================================================================================
// Represents LLPC sampler YCbCr conversion helper class
class SamplerYCbCrHelper
{
public:
    // Regist Builder
    inline void Register(BuilderImplImage* pBuilder) { m_pBuilder = pBuilder; }

    // Unregist Builder
    inline void UnRegister() { m_pBuilder = nullptr; }

    SamplerYCbCrHelper(BuilderImplImage* pBuilder, SamplerYCbCrConversionMetaData& yCbCrMetaData, YCbCrSampleInfo* pYCbCrSampleInfo, GfxIpVersion* pGfxIp);

    // Set default constructor unchanged
    SamplerYCbCrHelper() = default;

    // Set YCbCr conversion meta data
    void SetYCbCrConversionMetaData(SamplerYCbCrConversionMetaData& yCbCrMetaData);

    // Set YCbCr sample infomation
    void SetYCbCrSampleInfo(YCbCrSampleInfo* pYCbCrSampleInfo);

    // Generate sampler descriptor of chroma channel
    void GenSamplerDescChroma();

    // Generate image descriptor of chroma channel
    template<typename T>
    void GenImgDescChromaTemp();
    void GenImgDescChroma();

    // Convert from YCbCr to RGBA color space
    Value* ConvertColorSpace();

    // Sample YCbCr data from each planes
    // Note: Should be called after GenImgDescChroma and GenSamplerDescChroma completes
    void SampleYCbCrData();

    // Set the ST coords
    void SetCoord(Value* pS, Value* pT);

    // Set the sample infomation of Luma channel
    inline void SetSampleInfoLuma(Type* pResultType) { m_pResultType = pResultType; }

    // Set GfxIp version
    inline void SetGfxIpVersion(GfxIpVersion* pGfxIp) { m_pGfxIp = pGfxIp; }

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
    YCbCrSampleInfo*                m_pYCbCrSampleInfo;
    BuilderImplImage*               m_pBuilder;
    int                             m_planeNum;
    bool                            m_subsampledX;
    bool                            m_subsampledY;
    bool                            m_forceExplicitReconstruct;
    bool                            m_tileOptimal;
    uint32_t                        m_xBitCount;
    uint32_t                        m_yBitCount;
    uint32_t                        m_zBitCount;
    uint32_t                        m_wBitCount;
    uint32_t                        m_bpp;
    ComponentSwizzle                m_swizzleR;
    ComponentSwizzle                m_swizzleG;
    ComponentSwizzle                m_swizzleB;
    ComponentSwizzle                m_swizzleA;
    SamplerFilter                   m_lumaFilter;
    SamplerFilter                   m_chromaFilter;
    ChromaLocation                  m_xChromaOffset;
    ChromaLocation                  m_yChromaOffset;
    SamplerYCbCrRange               m_yCbCrRange;
    SamplerYCbCrModelConversion     m_yCbCrModel;
    uint32_t                        m_bits[3];
    Value*                          m_pWidth;
    Value*                          m_pHeight;
    Value*                          m_pS;
    Value*                          m_pT;
    Value*                          m_pU;
    Value*                          m_pV;
    Value*                          m_pI;
    Value*                          m_pJ;
    Value*                          m_pSamplerDescLuma;
    Value*                          m_pImgDescLuma;
    Value*                          m_pSamplerDescChroma;
    std::vector<Value*>             m_pImgDescChromas;
    Value*                          m_pYCbCrData;
    Value*                          m_pRGBAData;
    Type*                           m_pResultType;
    GfxIpVersion*                   m_pGfxIp;
};

} // Llpc
