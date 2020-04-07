/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "llpcYCbCrAddressHandler.h"
#include "vkgcDefs.h"

using Vkgc::SamplerYCbCrConversionMetaData;

namespace lgc
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
    enum Channel: uint32_t
    {
        Zero = 0,
        One  = 1,
        R    = 4,
        G    = 5,
        B    = 6,
        A    = 7,
    };

    ComponentSwizzle() = default;
    ComponentSwizzle(const Channel swizzle) : m_value(swizzle), m_channel(swizzle - Channel::R) {};
    ComponentSwizzle(const uint32_t swizzle)
    {
        assert((swizzle >= Channel::Zero) && (swizzle <= Channel::A));
        m_value = static_cast<Channel>(swizzle);

        m_channel = (swizzle >= Channel::R) ? (m_value - Channel::R) : (m_value + Channel::R);
    }

    operator Channel() const { return m_value; }

    ComponentSwizzle& operator=(uint32_t op)
    {
        assert((op >= Channel::Zero) && (op <= Channel::A));
        m_value = static_cast<Channel>(op);
        m_channel = (op >= Channel::R) ? (m_value - Channel::R) : (m_value + Channel::R);
        return *this;
    }
    constexpr bool operator!=(ComponentSwizzle::Channel op) const { return m_value != op; }
    constexpr bool operator==(ComponentSwizzle::Channel op) const { return m_value == op; }

    const uint32_t GetChannel() const { return m_channel; }

private:
    ComponentSwizzle::Channel m_value;
    uint32_t                  m_channel;
};

// General YCbCr sample info
struct YCbCrSampleInfo
{
    llvm::Type*              pResultTy;
    uint32_t           dim;
    uint32_t           flags;
    llvm::Value*             pImageDesc;
    llvm::Value*             pSamplerDesc;
    llvm::ArrayRef<llvm::Value*>   address;
    const std::string& instNameStr;
    bool               isSample;
};

// YCbCr sample info for downsampled chroma channels in the X dimension
struct XChromaSampleInfo
{
    YCbCrSampleInfo*   pYCbCrInfo;
    llvm::Value*             pImageDesc1;
    llvm::Value*             pI;
    llvm::Value*             pJ;
    llvm::Value*             pChromaWidth;
    llvm::Value*             pChromaHeight;
    ChromaLocation     xChromaOffset;
};

// YCbCr sample info for downsampled chroma channels in both X and Y dimension
struct XYChromaSampleInfo : XChromaSampleInfo
{
    llvm::Value*             pImageDesc2;
    uint32_t           planeNum;
    ChromaLocation     yChromaOffset;
};

// YCbCr wrapped sample info
struct YCbCrWrappedSampleInfo : XYChromaSampleInfo
{
    llvm::Value*             pImageDesc3;
    bool               subsampledX;
    bool               subsampledY;
};

class BuilderImplImage;

// =====================================================================================================================
// Represents LLPC sampler YCbCr conversion helper class
class SamplerYCbCrHelper
{
public:
    // Regist Builder
    inline void Register(BuilderImplImage* pBuilder) { m_pBuilder = pBuilder; }

    // Unregist Builder
    inline void UnRegister() { m_pBuilder = nullptr; }

    SamplerYCbCrHelper(BuilderImplImage*          pBuilder,
                       SamplerYCbCrConversionMetaData& yCbCrMetaData,
                       YCbCrSampleInfo*                pYCbCrSampleInfo,
                       GfxIpVersion*                   pGfxIp);

    // Set default constructor unchanged
    SamplerYCbCrHelper() = default;

    // Set YCbCr conversion meta data
    void SetYCbCrConversionMetaData(SamplerYCbCrConversionMetaData& yCbCrMetaData);

    // Set YCbCr sample infomation
    void SetYCbCrSampleInfo(YCbCrSampleInfo* pYCbCrSampleInfo);

    // Generate sampler descriptor of chroma channel
    void GenSamplerDescChroma();

    // Generate image descriptor of chroma channel
    void GenImgDescChroma();

    // Convert from YCbCr to RGBA color space
    llvm::Value* ConvertColorSpace();

    // Sample YCbCr data from each planes
    // Note: Should be called after GenImgDescChroma and GenSamplerDescChroma completes
    void SampleYCbCrData();

    // Set the ST coords
    void SetCoord(llvm::Value* pS, llvm::Value* pT);

    // Set the sample infomation of Luma channel
    inline void SetSampleInfoLuma(llvm::Type* pResultType) { m_pResultType = pResultType; }

    // Set GfxIp version
    inline void SetGfxIpVersion(GfxIpVersion* pGfxIp) { m_pGfxIp = pGfxIp; }

    // Implement transfer from ST coordinates to UV coordiantes operation
    llvm::Value* TransferSTtoUVCoords(llvm::Value* pST,
                                llvm::Value* pSize);

    // Implement the adjustment of UV coordiantes when the sample location associated with
    // downsampled chroma channels in the X/XY dimension occurs
    llvm::Value* YCbCrCalculateImplicitChromaUV(ChromaLocation offset,
                                          llvm::Value*         pUV);

    // Transfer IJ coordinates from UV coordinates
    llvm::Value* TransferUVtoIJCoords(SamplerFilter filter,
                                llvm::Value*        pUV);

    // Calculate UV offset to top-left pixel
    llvm::Value* CalculateUVoffset(llvm::Value* pUV);

    // Implement bilinear blending
    llvm::Value* BilinearBlend(llvm::Value* pAlpha,
                         llvm::Value* pBeta,
                         llvm::Value* pTL,
                         llvm::Value* pTR,
                         llvm::Value* pBL,
                         llvm::Value* pBR);

    // Implement wrapped YCbCr sample
    llvm::Value* YCbCrWrappedSample(YCbCrWrappedSampleInfo& wrapInfo);

    // Implement reconstructed YCbCr sample operation for downsampled chroma channels in the X dimension
    llvm::Value* YCbCrReconstructLinearXChromaSample(XChromaSampleInfo& xChromaInfo);

    // Implement reconstructed YCbCr sample operation for downsampled chroma channels in both X and Y dimension
    llvm::Value* YCbCrReconstructLinearXYChromaSample(XYChromaSampleInfo& xyChromaInfo);

    // Implement interanl image sample for YCbCr conversion
    llvm::Value* YCbCrCreateImageSampleInternal(llvm::SmallVectorImpl<llvm::Value*>& coords,
                                                YCbCrSampleInfo*         pYCbCrInfo);

    // Generate sampler descriptor for YCbCr conversion
    llvm::Value* YCbCrGenerateSamplerDesc(llvm::Value*        pSamplerDesc,
                                    SamplerFilter filter,
                                    bool          forceExplicitReconstruction);

    // Implement range expanding operation on checking whether the encoding uses full numerical range on chroma channel
    llvm::Value* YCbCrRangeExpand(SamplerYCbCrRange  range,
                            const uint32_t*    pBits,
                            llvm::Value*             pSample);

    // Implement the color transfer operation for conversion from YCbCr to RGB color model
    llvm::Value* YCbCrConvertColor(llvm::Type*                       pResultTy,
                             SamplerYCbCrModelConversion colorModel,
                             SamplerYCbCrRange           range,
                             uint32_t*                   pBits,
                             llvm::Value*                      pImageOp);

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
    llvm::Value*                          m_pWidth;
    llvm::Value*                          m_pHeight;
    llvm::Value*                          m_pS;
    llvm::Value*                          m_pT;
    llvm::Value*                          m_pU;
    llvm::Value*                          m_pV;
    llvm::Value*                          m_pI;
    llvm::Value*                          m_pJ;
    llvm::Value*                          m_pSamplerDescLuma;
    llvm::Value*                          m_pImgDescLuma;
    llvm::Value*                          m_pSamplerDescChroma;
    std::vector<llvm::Value*>             m_pImgDescChromas;
    llvm::Value*                          m_pYCbCrData;
    llvm::Value*                          m_pRGBAData;
    llvm::Type*                           m_pResultType;
    GfxIpVersion*                   m_pGfxIp;
};

} // Llpc
