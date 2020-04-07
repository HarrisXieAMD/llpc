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
* @file  llpcSamplerYCbCrHelper.cpp
* @brief LLPC source file: Implementation of LLPC utility class SamplerYCbCrHelper
***********************************************************************************************************************
*/
#include "llpcSamplerYCbCrHandler.h"
#include "llpcInternal.h"
#include "llpcTargetInfo.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace lgc;
using namespace llvm;

// =====================================================================================================================
// Implement wrapped YCbCr sample
Value* SamplerYCbCrHelper::YCbCrWrappedSample(
    YCbCrWrappedSampleInfo& wrapInfo) // [In] Wrapped YCbCr sample infomation
{
    SmallVector<Value*, 4> coordsChroma;
    YCbCrSampleInfo* pSampleInfo = wrapInfo.pYCbCrInfo;
    Value* pChromaWidth  = wrapInfo.pChromaWidth;
    Value* pChromaHeight = wrapInfo.pChromaHeight;

    if (wrapInfo.subsampledX)
    {
        pChromaWidth = m_pBuilder->CreateFMul(wrapInfo.pChromaWidth, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
    }

    if (wrapInfo.subsampledY)
    {
        pChromaHeight = m_pBuilder->CreateFMul(wrapInfo.pChromaHeight, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
    }

    coordsChroma.push_back(m_pBuilder->CreateFDiv(wrapInfo.pI, pChromaWidth));
    coordsChroma.push_back(m_pBuilder->CreateFDiv(wrapInfo.pJ, pChromaHeight));

    pSampleInfo->pImageDesc = wrapInfo.pImageDesc1;

    Value* pResult = nullptr;

    if (wrapInfo.planeNum == 1)
    {
        pSampleInfo->pImageDesc = wrapInfo.subsampledX ? wrapInfo.pImageDesc2 : wrapInfo.pImageDesc1;

        Instruction* pImageOp = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));
        pResult = m_pBuilder->CreateShuffleVector(pImageOp, pImageOp, { 0, 2 });
    }
    else if (wrapInfo.planeNum == 2)
    {
        pSampleInfo->pImageDesc = wrapInfo.pImageDesc2;
        Instruction* pImageOp = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));
        pResult = m_pBuilder->CreateShuffleVector(pImageOp, pImageOp, { 0, 2 });
    }
    else if (wrapInfo.planeNum == 3)
    {
        pSampleInfo->pImageDesc = wrapInfo.pImageDesc2;
        Instruction* pImageOp1 = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));

        pSampleInfo->pImageDesc = wrapInfo.pImageDesc3;
        Instruction* pImageOp2 = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));
        pResult = m_pBuilder->CreateShuffleVector(pImageOp2, pImageOp1, { 0, 6 });
    }
    else
    {
        llvm_unreachable("Out of ranged plane number!");
    }

    return pResult;
}

// =====================================================================================================================
// Create YCbCr Reconstruct linear X chroma sample
Value* SamplerYCbCrHelper::YCbCrReconstructLinearXChromaSample(
    XChromaSampleInfo& xChromaInfo) // [In] Infomation for downsampled chroma channels in X dimension
{
    YCbCrSampleInfo* pSampleInfo = xChromaInfo.pYCbCrInfo;
    Value* pIsEvenI = m_pBuilder->CreateICmpEQ(m_pBuilder->CreateSMod(
        m_pBuilder->CreateFPToSI(xChromaInfo.pI, m_pBuilder->getInt32Ty()),
        m_pBuilder->getInt32(2)), m_pBuilder->getInt32(0));

    Value* pSubI = m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor,
                m_pBuilder->CreateFDiv(xChromaInfo.pI, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0)));
    if (xChromaInfo.xChromaOffset != ChromaLocation::CositedEven)
    {
        pSubI = m_pBuilder->CreateSelect(pIsEvenI,
                            m_pBuilder->CreateFSub(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)),
                            pSubI);
    }

    Value* pAlpha = nullptr;
    if (xChromaInfo.xChromaOffset == ChromaLocation::CositedEven)
    {
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0),
                                                    ConstantFP::get(m_pBuilder->getFloatTy(), 0.5));
    }
    else
    {
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.25),
                                                    ConstantFP::get(m_pBuilder->getFloatTy(), 0.75));
    }

    Value* pT = m_pBuilder->CreateFDiv(xChromaInfo.pJ, xChromaInfo.pChromaHeight);

    SmallVector<Value*, 4> coordsChromaA;
    pSampleInfo->pImageDesc = xChromaInfo.pImageDesc1;
    coordsChromaA.push_back(m_pBuilder->CreateFDiv(pSubI, xChromaInfo.pChromaWidth));
    coordsChromaA.push_back(pT);
    Instruction* pImageOpA = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaA, pSampleInfo));

    SmallVector<Value*, 4> coordsChromaB;
    coordsChromaB.push_back(m_pBuilder->CreateFDiv(
        m_pBuilder->CreateFAdd(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), xChromaInfo.pChromaWidth));
    coordsChromaB.push_back(pT);
    Instruction* pImageOpB = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaB, pSampleInfo));

    Value* pResult = m_pBuilder->CreateFMix(pImageOpB, pImageOpA, pAlpha);

    return m_pBuilder->CreateShuffleVector(pResult, pResult, { 0, 2 });
}

// =====================================================================================================================
// Create YCbCr Reconstruct linear XY chroma sample
Value* SamplerYCbCrHelper::YCbCrReconstructLinearXYChromaSample(
    XYChromaSampleInfo& xyChromaInfo) // [In] Infomation for downsampled chroma channels in XY dimension
{
    YCbCrSampleInfo* pSampleInfo = xyChromaInfo.pYCbCrInfo;

    Value* pWidth = xyChromaInfo.pChromaWidth;
    Value* pHeight = xyChromaInfo.pChromaHeight;

    Value* pIsEvenI = m_pBuilder->CreateICmpEQ(
        m_pBuilder->CreateSMod(m_pBuilder->CreateFPToSI(xyChromaInfo.pI, m_pBuilder->getInt32Ty()),
        m_pBuilder->getInt32(2)), m_pBuilder->getInt32(0));
    Value* pIsEvenJ = m_pBuilder->CreateICmpEQ(
        m_pBuilder->CreateSMod(m_pBuilder->CreateFPToSI(xyChromaInfo.pJ, m_pBuilder->getInt32Ty()),
        m_pBuilder->getInt32(2)), m_pBuilder->getInt32(0));

    Value* pSubI = m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor,
                                        m_pBuilder->CreateFDiv(xyChromaInfo.pI,
                                        ConstantFP::get(m_pBuilder->getFloatTy(), 2.0)));
    Value* pSubJ = m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor,
                                        m_pBuilder->CreateFDiv(xyChromaInfo.pJ,
                                        ConstantFP::get(m_pBuilder->getFloatTy(), 2.0)));

    if (xyChromaInfo.xChromaOffset != ChromaLocation::CositedEven)
    {
        pSubI = m_pBuilder->CreateSelect(pIsEvenI,
                             m_pBuilder->CreateFSub(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)),
                             pSubI);
    }

    if (xyChromaInfo.yChromaOffset != ChromaLocation::CositedEven)
    {
        pSubJ = m_pBuilder->CreateSelect(pIsEvenJ,
                             m_pBuilder->CreateFSub(pSubJ, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)),
                             pSubJ);
    }

    Value* pAlpha = nullptr;
    if (xyChromaInfo.xChromaOffset == ChromaLocation::CositedEven)
    {
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0),
        ConstantFP::get(m_pBuilder->getFloatTy(), 0.5));
    }
    else
    {
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.25),
        ConstantFP::get(m_pBuilder->getFloatTy(), 0.75));
    }

    Value* pBeta = nullptr;
    if (xyChromaInfo.yChromaOffset == ChromaLocation::CositedEven)
    {
        pBeta = m_pBuilder->CreateSelect(pIsEvenJ, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0),
        ConstantFP::get(m_pBuilder->getFloatTy(), 0.5));
    }
    else
    {
        pBeta = m_pBuilder->CreateSelect(pIsEvenJ, ConstantFP::get(m_pBuilder->getFloatTy(), 0.25),
        ConstantFP::get(m_pBuilder->getFloatTy(), 0.75));
    }

    SmallVector<Value*, 4> coordsChromaTL;
    SmallVector<Value*, 4> coordsChromaTR;
    SmallVector<Value*, 4> coordsChromaBL;
    SmallVector<Value*, 4> coordsChromaBR;

    Value* pResult = nullptr;
    if (xyChromaInfo.planeNum == 2)
    {
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;

        // Sample TL
        coordsChromaTL.push_back(m_pBuilder->CreateFDiv(pSubI, pWidth));
        coordsChromaTL.push_back(m_pBuilder->CreateFDiv(pSubJ, pHeight));
        Instruction* pTL = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaTL, pSampleInfo));

        // Sample TR
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(pSubJ, pHeight));
        Instruction* pTR = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaTR, pSampleInfo));

        // Sample BL
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(pSubI, pWidth));
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        Instruction* pBL = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaBL, pSampleInfo));

        // Sample BR
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        Instruction* pBR = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaBR, pSampleInfo));

        // Linear interpolate
        pResult = BilinearBlend(pAlpha, pBeta, pTL, pTR, pBL, pBR);
        pResult = m_pBuilder->CreateShuffleVector(pResult, pResult, { 0, 2});
    }
    else if (xyChromaInfo.planeNum == 3)
    {
        // Sample TL
        coordsChromaTL.push_back(m_pBuilder->CreateFDiv(pSubI, pWidth));
        coordsChromaTL.push_back(m_pBuilder->CreateFDiv(pSubJ, pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pTLb = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaTL, pSampleInfo));

        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pTLr = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaTL, pSampleInfo));
        Value* pTL = m_pBuilder->CreateShuffleVector(pTLr, pTLb, { 0, 6});

        // Sample TR
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(pSubJ, pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pTRb = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaTR, pSampleInfo));

        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pTRr = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaTR, pSampleInfo));
        Value* pTR = m_pBuilder->CreateShuffleVector(pTRr, pTRb, { 0, 6});

        // Sample BL
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(pSubI, pWidth));
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pBLb = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaBL, pSampleInfo));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pBLr = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaBL, pSampleInfo));
        Value* pBL = m_pBuilder->CreateShuffleVector(pBLr, pBLb, { 0, 6});

        // Sample BR
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ,
            ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pBRb = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaBR, pSampleInfo));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pBRr = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsChromaBR, pSampleInfo));
        Value* pBR = m_pBuilder->CreateShuffleVector(pBRr, pBRb, { 0, 6});

        // Linear interpolate
        pResult = BilinearBlend(pAlpha, pBeta, pTL, pTR, pBL, pBR);
    }

    return pResult;
}

// =====================================================================================================================
// Create YCbCr image sample internal
Value* SamplerYCbCrHelper::YCbCrCreateImageSampleInternal(
    SmallVectorImpl<Value*>& coords,     // ST coords
    YCbCrSampleInfo*         pYCbCrInfo) // [In] YCbCr smaple information
{
    Value* pCoords = m_pBuilder->CreateInsertElement(Constant::getNullValue(VectorType::get(coords[0]->getType(), 2)),
                                                     coords[0],
                                                     uint64_t(0));

    pCoords = m_pBuilder->CreateInsertElement(pCoords, coords[1], uint64_t(1));

    return m_pBuilder->CreateImageSampleGather(pYCbCrInfo->pResultTy,
                                   pYCbCrInfo->dim,
                                   pYCbCrInfo->flags,
                                   pCoords,
                                   pYCbCrInfo->pImageDesc,
                                   pYCbCrInfo->pSamplerDesc,
                                   pYCbCrInfo->address,
                                   pYCbCrInfo->instNameStr,
                                   pYCbCrInfo->isSample);
}

SamplerYCbCrHelper::SamplerYCbCrHelper(
    BuilderImplImage* pBuilder,
    SamplerYCbCrConversionMetaData& yCbCrMetaData,
    YCbCrSampleInfo* pYCbCrSampleInfo,
    GfxIpVersion*  pGfxIp)
{
    Register(pBuilder);
    SetGfxIpVersion(pGfxIp);
    SetYCbCrConversionMetaData(yCbCrMetaData);
    SetYCbCrSampleInfo(pYCbCrSampleInfo);
    GenSamplerDescChroma();
    GenImgDescChroma();
}

void SamplerYCbCrHelper::SetYCbCrConversionMetaData(
    SamplerYCbCrConversionMetaData& yCbCrMetaData)
{
    m_planeNum                 = yCbCrMetaData.word1.planes;
    m_subsampledX              = yCbCrMetaData.word1.xSubSampled;
    m_subsampledY              = yCbCrMetaData.word1.ySubSampled;
    m_forceExplicitReconstruct = yCbCrMetaData.word0.forceExplicitReconstruct;
    m_xBitCount                = yCbCrMetaData.word2.bitCounts.xBitCount;
    m_yBitCount                = yCbCrMetaData.word2.bitCounts.yBitCount;
    m_zBitCount                = yCbCrMetaData.word2.bitCounts.zBitCount;
    m_wBitCount                = yCbCrMetaData.word2.bitCounts.wBitCount;
    m_bpp                      = (m_planeNum == 1) ? (m_xBitCount + m_yBitCount + m_zBitCount + m_wBitCount)
                                                    : m_xBitCount;
    m_swizzleR                 = yCbCrMetaData.word0.componentMapping.swizzleR;
    m_swizzleG                 = yCbCrMetaData.word0.componentMapping.swizzleG;
    m_swizzleB                 = yCbCrMetaData.word0.componentMapping.swizzleB;
    m_swizzleA                 = yCbCrMetaData.word0.componentMapping.swizzleA;
    m_lumaFilter               = static_cast<SamplerFilter>(yCbCrMetaData.word1.lumaFilter);
    m_chromaFilter             = static_cast<SamplerFilter>(yCbCrMetaData.word1.chromaFilter);
    m_xChromaOffset            = static_cast<ChromaLocation>(yCbCrMetaData.word1.xChromaOffset);
    m_yChromaOffset            = static_cast<ChromaLocation>(yCbCrMetaData.word1.yChromaOffset);
    m_yCbCrRange               = static_cast<SamplerYCbCrRange>(yCbCrMetaData.word0.yCbCrRange);
    m_yCbCrModel               = static_cast<SamplerYCbCrModelConversion>(yCbCrMetaData.word0.yCbCrModel);
    m_tileOptimal              = yCbCrMetaData.word1.tileOptimal;
    m_bits[0]                  = yCbCrMetaData.word0.bitDepth.channelBitsR;
    m_bits[1]                  = yCbCrMetaData.word0.bitDepth.channelBitsG;
    m_bits[2]                  = yCbCrMetaData.word0.bitDepth.channelBitsB;
    m_pMetaData                = &yCbCrMetaData;

    m_pImgDescChromas.resize(3);
}

void SamplerYCbCrHelper::SetYCbCrSampleInfo(
    YCbCrSampleInfo* pYCbCrSampleInfo)
{
    m_pYCbCrSampleInfo = pYCbCrSampleInfo;

    m_pSamplerDescLuma = m_pYCbCrSampleInfo->pSamplerDesc;
    m_pImgDescLuma     = m_pYCbCrSampleInfo->pImageDesc;

    m_pSamplerDescChroma = m_pSamplerDescLuma;
    m_pImgDescChromas[0] = m_pImgDescLuma;

    m_pResultType = m_pYCbCrSampleInfo->pResultTy;
}

void SamplerYCbCrHelper::GenSamplerDescChroma()
{
    m_pSamplerDescChroma = YCbCrGenerateSamplerDesc(m_pSamplerDescLuma,
                                                    m_chromaFilter,
                                                    m_forceExplicitReconstruct);
}

void SamplerYCbCrHelper::GenImgDescChroma()
{
    Value* pInt32One = ConstantInt::get(m_pBuilder->getInt32Ty(), 1);
    lgc::SqImgRsrcRegHandler proxySqRsrcRegHelper(m_pBuilder, m_pImgDescLuma, m_pGfxIp);
    // T proxySqRsrcRegHelper(m_pBuilder, m_pImgDescLuma);
    lgc::YCbCrAddressHandler addrHelper(m_pBuilder, &proxySqRsrcRegHelper, m_pGfxIp);
    addrHelper.GenHeightAndPitch(m_bits[0], m_bpp, m_xBitCount, m_tileOptimal, m_planeNum);
    addrHelper.GenBaseAddress(m_planeNum);

    Value* pWidth = proxySqRsrcRegHelper.GetReg(SqRsrcRegs::Width);
    Value* pHeight = proxySqRsrcRegHelper.GetReg(SqRsrcRegs::Height);
    Value* pWidthR1 = m_pBuilder->CreateLShr(pWidth, pInt32One);
    Value* pHeightR1 = m_pBuilder->CreateLShr(pHeight, pInt32One);
    m_pWidth = m_pBuilder->CreateUIToFP(pWidth, m_pBuilder->getFloatTy());
    m_pHeight = m_pBuilder->CreateUIToFP(pHeight, m_pBuilder->getFloatTy());

    Value* pPitch =  proxySqRsrcRegHelper.GetReg(SqRsrcRegs::Pitch);
    Value* pPitchR1 = m_pBuilder->CreateLShr(pPitch, pInt32One);

    proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Width, pWidthR1);
    proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Format, m_pBuilder->getInt32(m_pMetaData->word3.sqImgRsrcWord1));

    switch (m_planeNum)
    {
    case 1:
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::DstSelXYZW, m_pBuilder->getInt32(m_pMetaData->word1.dstSelXYZW));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BcSwizzle, m_pBuilder->getInt32(6));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Pitch, addrHelper.GetPitchCb());
        m_pImgDescChromas[1] = proxySqRsrcRegHelper.GetRegister();
        break;
    case 2:
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BaseAddress, addrHelper.GetPlane(1));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Height, pHeightR1);
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::DstSelXYZW, m_pBuilder->getInt32(m_pMetaData->word1.dstSelXYZW));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BcSwizzle, m_pBuilder->getInt32(6));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Pitch, pPitchR1);
        m_pImgDescChromas[1] = proxySqRsrcRegHelper.GetRegister();
        break;
    case 3:
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BaseAddress, addrHelper.GetPlane(1));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Height, pHeightR1);
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::DstSelXYZW, m_pBuilder->getInt32(0x300));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BcSwizzle, m_pBuilder->getInt32(4));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::Pitch, pPitchR1);
        m_pImgDescChromas[1] = proxySqRsrcRegHelper.GetRegister();

        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BaseAddress, addrHelper.GetPlane(2));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::DstSelXYZW, m_pBuilder->getInt32(0x204));
        proxySqRsrcRegHelper.SetReg(SqRsrcRegs::BcSwizzle, m_pBuilder->getInt32(5));
        m_pImgDescChromas[2] = proxySqRsrcRegHelper.GetRegister();
        break;
    default:
        break;
    }
}

Value* SamplerYCbCrHelper::ConvertColorSpace()
{
    return YCbCrConvertColor(m_pResultType,
                             m_yCbCrModel,
                             m_yCbCrRange,
                             m_bits,
                             m_pYCbCrData);
}

void SamplerYCbCrHelper::SetCoord(
    Value* pS,
    Value* pT)
{
    m_pS = pS;
    m_pT = pT;

    m_pU = TransferSTtoUVCoords(m_pS, m_pWidth);
    m_pV = TransferSTtoUVCoords(m_pT, m_pHeight);
    m_pI = TransferUVtoIJCoords(m_lumaFilter, m_pU);
    m_pJ = TransferUVtoIJCoords(m_lumaFilter, m_pV);
}

// =====================================================================================================================
// Generate sampler descriptor for YCbCr conversion
Value* SamplerYCbCrHelper::YCbCrGenerateSamplerDesc(
    Value*           pSamplerDesc,                // [in] Sampler descriptor
    SamplerFilter    filter,                      // The type of sampler filter
    bool             forceExplicitReconstruction) // Enable/Disable force explict chroma reconstruction
{
    SqImgSampRegHandler imgRegHelper(m_pBuilder, pSamplerDesc, m_pGfxIp);
    //imgRegHelper.Register(m_pBuilder);
    //imgRegHelper.BoundRegData(pSamplerDesc, 4);

    /// Determines if "TexFilter" should be ignored or not.
    // enum class TexFilterMode : uint32
    // {
    //     Blend = 0x0, ///< Use the filter method specified by the TexFilter enumeration
    //     Min   = 0x1, ///< Use the minimum value returned by the sampler, no blending op occurs
    //     Max   = 0x2, ///< Use the maximum value returned by the sampler, no blending op occurs
    // };
    // Force use blend mode
    // imgRegHelper.SetFilterMode(m_pBuilder->getInt32(0b00));
    imgRegHelper.SetReg(SqSampRegs::FilterMode, m_pBuilder->getInt32(0b00));

    /// Enumeration which defines the mode for magnification and minification sampling
    // enum XyFilter : uint32
    // {
    //     XyFilterPoint = 0,          ///< Use single point sampling
    //     XyFilterLinear,             ///< Use linear sampling
    //     XyFilterAnisotropicPoint,   ///< Use anisotropic with single point sampling
    //     XyFilterAnisotropicLinear,  ///< Use anisotropic with linear sampling
    //     XyFilterCount
    // };
    if ((filter == SamplerFilter::Nearest) || forceExplicitReconstruction)
    {
        // imgRegHelper.SetXYMagFilter(m_pBuilder->getInt32(0b00));
        // imgRegHelper.SetXYMinFilter(m_pBuilder->getInt32(0b00));
        imgRegHelper.SetReg(SqSampRegs::xyMagFilter, m_pBuilder->getInt32(0b00));
        imgRegHelper.SetReg(SqSampRegs::xyMinFilter, m_pBuilder->getInt32(0b00));
    }
    else //filter == SamplerFilter::Linear
    {
        // imgRegHelper.SetXYMagFilter(m_pBuilder->getInt32(0b01));
        // imgRegHelper.SetXYMinFilter(m_pBuilder->getInt32(0b01));
        imgRegHelper.SetReg(SqSampRegs::xyMagFilter, m_pBuilder->getInt32(0b01));
        imgRegHelper.SetReg(SqSampRegs::xyMinFilter, m_pBuilder->getInt32(0b01));
    }

    return imgRegHelper.GetRegister();
}

// =====================================================================================================================
// Implement range expanding operation on checking whether the encoding uses full numerical range on luma channel
Value* SamplerYCbCrHelper::YCbCrRangeExpand(
    SamplerYCbCrRange    range,   // Specifies whether the encoding uses the full numerical range
    const uint32_t*      pBits,   // Channel bits
    Value*               pSample) // [in] Sample results which need range expansion, assume in sequence => Cr, Y, Cb
{
    switch (range)
    {
    case SamplerYCbCrRange::ItuFull:
    {
        //              [2^(n - 1)/((2^n) - 1)]
        // pConvVec1 =  [         0.0         ]
        //              [2^(n - 1)/((2^n) - 1)]
        float row0Num = static_cast<float>(0x1u << (pBits[0] - 0x1u)) / ((0x1u << pBits[0]) - 1u);
        float row2Num = static_cast<float>(0x1u << (pBits[2] - 0x1u)) / ((0x1u << pBits[2]) - 1u);

        Value* pConvVec1 = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
        pConvVec1 = m_pBuilder->CreateInsertElement(pConvVec1, ConstantFP::get(m_pBuilder->getFloatTy(), row0Num), uint64_t(0));
        pConvVec1 = m_pBuilder->CreateInsertElement(pConvVec1, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0f), uint64_t(1));
        pConvVec1 = m_pBuilder->CreateInsertElement(pConvVec1, ConstantFP::get(m_pBuilder->getFloatTy(), row2Num), uint64_t(2));

        //          [Cr]   pConvVec1[0]
        // result = [ Y] - pConvVec1[1]
        //          [Cb]   pConvVec1[2]
        return m_pBuilder->CreateFSub(pSample, pConvVec1);
    }
    case SamplerYCbCrRange::ItuNarrow:
    {
        //             [(2^n - 1)/(224 x (2^(n - 8))]
        // pConvVec1 = [(2^n - 1)/(219 x (2^(n - 8))]
        //             [(2^n - 1)/(224 x (2^(n - 8))]
        float row0Num = static_cast<float>((0x1u << pBits[0]) - 1u) / (224u * (0x1u << (pBits[0] - 8)));
        float row1Num = static_cast<float>((0x1u << pBits[1]) - 1u) / (219u * (0x1u << (pBits[1] - 8)));
        float row2Num = static_cast<float>((0x1u << pBits[2]) - 1u) / (224u * (0x1u << (pBits[2] - 8)));

        Value* pConvVec1 = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
        pConvVec1 = m_pBuilder->CreateInsertElement(pConvVec1, ConstantFP::get(m_pBuilder->getFloatTy(), row0Num), uint64_t(0));
        pConvVec1 = m_pBuilder->CreateInsertElement(pConvVec1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Num), uint64_t(1));
        pConvVec1 = m_pBuilder->CreateInsertElement(pConvVec1, ConstantFP::get(m_pBuilder->getFloatTy(), row2Num), uint64_t(2));

        //             [(128 x (2^(n - 8))/(224 x (2^(n - 8))]
        // pConvVec2 = [( 16 x (2^(n - 8))/(219 x (2^(n - 8))]
        //             [(128 x (2^(n - 8))/(224 x (2^(n - 8))]
        row0Num = static_cast<float>(128u * (0x1u << (pBits[0] - 8))) / (224u * (0x1u << (pBits[0] - 8)));
        row1Num = static_cast<float>( 16u * (0x1u << (pBits[1] - 8))) / (219u * (0x1u << (pBits[1] - 8)));
        row2Num = static_cast<float>(128u * (0x1u << (pBits[2] - 8))) / (224u * (0x1u << (pBits[2] - 8)));

        Value* pConvVec2 = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
        pConvVec2 = m_pBuilder->CreateInsertElement(pConvVec2, ConstantFP::get(m_pBuilder->getFloatTy(), row0Num), uint64_t(0));
        pConvVec2 = m_pBuilder->CreateInsertElement(pConvVec2, ConstantFP::get(m_pBuilder->getFloatTy(), row1Num), uint64_t(1));
        pConvVec2 = m_pBuilder->CreateInsertElement(pConvVec2, ConstantFP::get(m_pBuilder->getFloatTy(), row2Num), uint64_t(2));

        //          pConvVec1[0]   [Cr]   pConvVec2[0]
        // result = pConvVec1[1] * [ Y] - pConvVec2[1]
        //          pConvVec1[2]   [Cb]   pConvVec2[2]
        return m_pBuilder->CreateFSub(m_pBuilder->CreateFMul(pSample, pConvVec1), pConvVec2);
    }

    default:
        llvm_unreachable("Unknown range expanding type!");
        return nullptr;
    }
}

void SamplerYCbCrHelper::SampleYCbCrData()
{
    SmallVector<Value*, 4> coordsLuma;
    SmallVector<Value*, 4> coordsChroma;

    // pI -> pS
    coordsLuma.push_back(m_pS);
    // pJ -> pT
    coordsLuma.push_back(m_pT);

    // Sample Y and A channels
    Value* pImageOpLuma = cast<Instruction>(YCbCrCreateImageSampleInternal(coordsLuma, m_pYCbCrSampleInfo));
    pImageOpLuma = m_pBuilder->CreateShuffleVector(pImageOpLuma, pImageOpLuma, { 1, 3 });

    // Init sample chroma info
    m_pYCbCrSampleInfo->pSamplerDesc = m_pSamplerDescChroma;

    // Init chroma pWidth and pHeight
    Value* pChromaWidth = m_pBuilder->CreateFMul(m_pWidth, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
    Value* pChromaHeight = m_pBuilder->CreateFMul(m_pHeight, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));

    // Init smaple chroma info for downsampled chroma channels in the x dimension
    XChromaSampleInfo xChromaInfo = {};
    xChromaInfo.pYCbCrInfo = m_pYCbCrSampleInfo;
    xChromaInfo.pImageDesc1 = m_pImgDescChromas[1];
    xChromaInfo.pI = m_pI;
    xChromaInfo.pJ = m_pJ;
    xChromaInfo.pChromaWidth = pChromaWidth;
    xChromaInfo.pChromaHeight = m_pHeight;
    xChromaInfo.xChromaOffset = m_xChromaOffset;

    // Init smaple chroma info for downsampled chroma channels in xy dimension
    XYChromaSampleInfo xyChromaInfo = {};
    xyChromaInfo.pYCbCrInfo = m_pYCbCrSampleInfo;
    xyChromaInfo.pImageDesc1 = m_pImgDescChromas[1];
    xyChromaInfo.pImageDesc2 = m_pImgDescChromas[2];
    xyChromaInfo.pI = m_pI;
    xyChromaInfo.pJ = m_pJ;
    xyChromaInfo.pChromaWidth = pChromaWidth;
    xyChromaInfo.pChromaHeight = pChromaHeight;
    xyChromaInfo.planeNum = m_planeNum;
    xyChromaInfo.xChromaOffset = m_xChromaOffset;
    xyChromaInfo.yChromaOffset = m_yChromaOffset;

    // Init wrapped smaple chroma info
    YCbCrWrappedSampleInfo wrappedSampleInfo = {};
    wrappedSampleInfo.pYCbCrInfo = m_pYCbCrSampleInfo;
    wrappedSampleInfo.pChromaWidth = m_pWidth;
    wrappedSampleInfo.pChromaHeight = m_pHeight;
    wrappedSampleInfo.pI = m_pU;
    wrappedSampleInfo.pJ = m_pV;
    wrappedSampleInfo.pImageDesc1 = m_pImgDescChromas[0];
    wrappedSampleInfo.pImageDesc2 = m_pImgDescChromas[1];
    wrappedSampleInfo.pImageDesc3 = m_pImgDescChromas[2];
    wrappedSampleInfo.planeNum = m_planeNum;
    wrappedSampleInfo.subsampledX = m_subsampledX;
    wrappedSampleInfo.subsampledY = m_subsampledY;

    Value* pImageOpChroma = nullptr;

    if (m_lumaFilter == SamplerFilter::Nearest)
    {
        if (m_forceExplicitReconstruct || !(m_subsampledX || m_subsampledY))
        {
            if ((m_chromaFilter == SamplerFilter::Nearest) || !m_subsampledX)
            {
                // Reconstruct using nearest if needed, otherwise, just take what's already there.
                wrappedSampleInfo.subsampledX = false;
                wrappedSampleInfo.subsampledY = false;

                pImageOpChroma = YCbCrWrappedSample(wrappedSampleInfo);
            }
            else // SamplerFilter::Linear
            {
                if (m_subsampledY)
                {
                    pImageOpChroma = YCbCrReconstructLinearXYChromaSample(xyChromaInfo);
                }
                else
                {
                    pImageOpChroma = YCbCrReconstructLinearXChromaSample(xChromaInfo);
                }
            }
        }
        else
        {
            if (m_subsampledX)
            {
                wrappedSampleInfo.pI = YCbCrCalculateImplicitChromaUV(m_xChromaOffset, m_pU);
            }

            if (m_subsampledY)
            {
                wrappedSampleInfo.pJ = YCbCrCalculateImplicitChromaUV(m_yChromaOffset, m_pV);
            }

            pImageOpChroma = YCbCrWrappedSample(wrappedSampleInfo);
        }
    }
    else //lumaFilter == SamplerFilter::Linear
    {
        if (m_forceExplicitReconstruct || !(m_subsampledX || m_subsampledY))
        {
            Value* pLumaA = CalculateUVoffset(m_pU);
            Value* pLumaB = CalculateUVoffset(m_pV);
            Value* pSubIPlusOne = m_pBuilder->CreateFAdd(m_pI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0f));
            Value* pSubJPlusOne = m_pBuilder->CreateFAdd(m_pJ, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0f));

            if ((m_chromaFilter == SamplerFilter::Nearest) || !m_subsampledX)
            {
                if (!m_subsampledX)
                {
                    wrappedSampleInfo.subsampledX = false;
                    wrappedSampleInfo.subsampledY = false;
                    pImageOpChroma = YCbCrWrappedSample(wrappedSampleInfo);
                }
                else
                {
                    Value* pSubI = m_pI;
                    Value* pSubJ = m_pJ;
                    if (m_subsampledX)
                    {
                        pSubI = m_pBuilder->CreateFDiv(m_pI, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0));
                        pSubIPlusOne = m_pBuilder->CreateFDiv(pSubIPlusOne, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0));
                    }

                    if (m_subsampledY)
                    {
                        pSubJ = m_pBuilder->CreateFDiv(m_pJ, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0));
                        pSubJPlusOne = m_pBuilder->CreateFDiv(pSubJPlusOne, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0));
                    }

                    wrappedSampleInfo.pI = pSubI;
                    wrappedSampleInfo.pJ = pSubJ;
                    Value* pTL = YCbCrWrappedSample(wrappedSampleInfo);

                    wrappedSampleInfo.pI = pSubIPlusOne;
                    Value* pTR = YCbCrWrappedSample(wrappedSampleInfo);

                    wrappedSampleInfo.pJ = pSubJPlusOne;
                    Value* pBR = YCbCrWrappedSample(wrappedSampleInfo);

                    wrappedSampleInfo.pI = pSubI;
                    Value* pBL = YCbCrWrappedSample(wrappedSampleInfo);

                    pImageOpChroma = BilinearBlend(pLumaA, pLumaB, pTL, pTR, pBL, pBR);
                }
            }
            else // vk::VK_FILTER_LINEAR
            {
                if (m_subsampledY)
                {
                    // Linear, Reconstructed xy chroma samples with explicit linear filtering
                    Value* pTL = YCbCrReconstructLinearXYChromaSample(xyChromaInfo);

                    xyChromaInfo.pI = pSubIPlusOne;
                    Value* pTR = YCbCrReconstructLinearXYChromaSample(xyChromaInfo);

                    xyChromaInfo.pJ = pSubJPlusOne;
                    Value* pBR = YCbCrReconstructLinearXYChromaSample(xyChromaInfo);

                    xyChromaInfo.pI = m_pI;
                    Value* pBL = YCbCrReconstructLinearXYChromaSample(xyChromaInfo);

                    pImageOpChroma = BilinearBlend(pLumaA, pLumaB, pTL, pTR, pBL, pBR);
                }
                else
                {
                    // Linear, Reconstructed X chroma samples with explicit linear filtering
                    Value* pTL = YCbCrReconstructLinearXChromaSample(xChromaInfo);

                    xChromaInfo.pI = pSubIPlusOne;
                    Value* pTR = YCbCrReconstructLinearXChromaSample(xChromaInfo);

                    xChromaInfo.pJ = pSubJPlusOne;
                    Value* pBR = YCbCrReconstructLinearXChromaSample(xChromaInfo);

                    xChromaInfo.pI = m_pI;
                    Value* pBL = YCbCrReconstructLinearXChromaSample(xChromaInfo);

                    pImageOpChroma = BilinearBlend(pLumaA, pLumaB, pTL, pTR, pBL, pBR);
                }
            }
        }
        else
        {
            if (m_subsampledX)
            {
                wrappedSampleInfo.pI = YCbCrCalculateImplicitChromaUV(m_xChromaOffset, m_pU);
            }

            if (m_subsampledY)
            {
                wrappedSampleInfo.pJ = YCbCrCalculateImplicitChromaUV(m_yChromaOffset, m_pV);
            }

            pImageOpChroma = YCbCrWrappedSample(wrappedSampleInfo);
        }
    }

    // Adjust channel sequence to R,G,B,A
    m_pYCbCrData = m_pBuilder->CreateShuffleVector(pImageOpLuma, pImageOpChroma, { 2, 0, 3, 1});

    // Shuffle channels if necessary
    m_pYCbCrData = m_pBuilder->CreateShuffleVector(m_pYCbCrData,
                                                   m_pYCbCrData,
                                                   { m_swizzleR.GetChannel(),
                                                     m_swizzleG.GetChannel(),
                                                     m_swizzleB.GetChannel(),
                                                     m_swizzleA.GetChannel() });
}

// =====================================================================================================================
// Implement the color transfer operation for conversion from  YCbCr to RGB color model
Value* SamplerYCbCrHelper::YCbCrConvertColor(
    Type*                          pResultTy,  // [in] Result type, assumed in <4 x f32>
    SamplerYCbCrModelConversion    colorModel, // The color conversion model
    SamplerYCbCrRange              range,      // Specifies whether the encoding uses the full numerical range
    uint32_t*                      pBits,      // Channel bits
    Value*                         pImageOp)   // [in] Results which need color conversion, in sequence => Cr, Y, Cb
{
    Value* pSubImage = m_pBuilder->CreateShuffleVector(pImageOp, pImageOp, { 0, 1, 2 });

    Value* pMinVec = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
    pMinVec = m_pBuilder->CreateInsertElement(pMinVec, ConstantFP::get(m_pBuilder->getFloatTy(), -0.5), uint64_t(0));
    pMinVec = m_pBuilder->CreateInsertElement(pMinVec, ConstantFP::get(m_pBuilder->getFloatTy(),  0.0), uint64_t(1));
    pMinVec = m_pBuilder->CreateInsertElement(pMinVec, ConstantFP::get(m_pBuilder->getFloatTy(), -0.5), uint64_t(2));

    Value* pMaxVec = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
    pMaxVec = m_pBuilder->CreateInsertElement(pMaxVec, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5), uint64_t(0));
    pMaxVec = m_pBuilder->CreateInsertElement(pMaxVec, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0), uint64_t(1));
    pMaxVec = m_pBuilder->CreateInsertElement(pMaxVec, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5), uint64_t(2));

    Value* pResult = UndefValue::get(pResultTy);

    switch (colorModel)
    {
    case SamplerYCbCrModelConversion::RgbIdentity:
    {
        //pResult[Cr] = C'_rgba [R]
        //pResult[Y]  = C'_rgba [G]
        //pResult[Cb] = C'_rgba [B]
        //pResult[a]  = C'_rgba [A]
        pResult = pImageOp;
        break;
    }
    case SamplerYCbCrModelConversion::YCbCrIdentity:
    {
        // pResult = RangeExpaned(C'_rgba)
        pSubImage = m_pBuilder->CreateFClamp(YCbCrRangeExpand(range, pBits, pSubImage), pMinVec, pMaxVec);
        Value* pOutputR = m_pBuilder->CreateExtractElement(pSubImage, m_pBuilder->getInt64(0));
        Value* pOutputG = m_pBuilder->CreateExtractElement(pSubImage, m_pBuilder->getInt64(1));
        Value* pOutputB = m_pBuilder->CreateExtractElement(pSubImage, m_pBuilder->getInt64(2));
        Value* pOutputA = m_pBuilder->CreateExtractElement(pImageOp, m_pBuilder->getInt64(3));

        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputR, m_pBuilder->getInt64(0));
        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputG, m_pBuilder->getInt64(1));
        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputB, m_pBuilder->getInt64(2));
        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputA, m_pBuilder->getInt64(3));
        break;
    }
    case SamplerYCbCrModelConversion::YCbCr601:
    case SamplerYCbCrModelConversion::YCbCr709:
    case SamplerYCbCrModelConversion::YCbCr2020:
    {
        // pInputVec = RangeExpaned(C'_rgba)
        Value* pInputVec = m_pBuilder->CreateFClamp(YCbCrRangeExpand(range, pBits, pSubImage), pMinVec, pMaxVec);

        Value* pRow0 = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
        Value* pRow1 = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));
        Value* pRow2 = UndefValue::get(VectorType::get(m_pBuilder->getFloatTy(), 3));

        if (colorModel == SamplerYCbCrModelConversion::YCbCr601)
        {
            //           [            1.402f,   1.0f,               0.0f]
            // convMat = [-0.419198 / 0.587f,   1.0f, -0.202008 / 0.587f]
            //           [              0.0f,   1.0f,             1.772f]

            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(), 1.402f), uint64_t(0));
            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(),   1.0f), uint64_t(1));
            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(),   0.0f), uint64_t(2));

            float row1Col0 = static_cast<float>(-0.419198 / 0.587);
            float row1Col2 = static_cast<float>(-0.202008 / 0.587);

            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Col0), uint64_t(0));
            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(),     1.0f), uint64_t(1));
            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Col2), uint64_t(2));

            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(),   0.0f), uint64_t(0));
            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(),   1.0f), uint64_t(1));
            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(), 1.772f), uint64_t(2));
        }
        else if (colorModel == SamplerYCbCrModelConversion::YCbCr709)
        {
            //           [              1.5748f,   1.0f,                  0.0f]
            // convMat = [-0.33480248 / 0.7152f,   1.0f, -0.13397432 / 0.7152f]
            //           [                 0.0f,   1.0f,               1.8556f]

            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(), 1.5748f), uint64_t(0));
            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(),    1.0f), uint64_t(1));
            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(),    0.0f), uint64_t(2));

            float row1Col0 = static_cast<float>(-0.33480248 / 0.7152);
            float row1Col2 = static_cast<float>(-0.13397432 / 0.7152);

            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Col0), uint64_t(0));
            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(),     1.0f), uint64_t(1));
            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Col2), uint64_t(2));

            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(),    0.0f), uint64_t(0));
            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(),    1.0f), uint64_t(1));
            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(), 1.8556f), uint64_t(2));
        }
        else
        {
            //           [              1.4746f,   1.0f,                  0.0f]
            // convMat = [-0.38737742 / 0.6780f,   1.0f, -0.11156702 / 0.6780f]
            //           [                 0.0f,   1.0f,               1.8814f]

            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(), 1.4746f), uint64_t(0));
            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(),    1.0f), uint64_t(1));
            pRow0 = m_pBuilder->CreateInsertElement(pRow0, ConstantFP::get(m_pBuilder->getFloatTy(),    0.0f), uint64_t(2));

            float row1Col0 = static_cast<float>(-0.38737742 / 0.6780);
            float row1Col2 = static_cast<float>(-0.11156702 / 0.6780);

            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Col0), uint64_t(0));
            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(),     1.0f), uint64_t(1));
            pRow1 = m_pBuilder->CreateInsertElement(pRow1, ConstantFP::get(m_pBuilder->getFloatTy(), row1Col2), uint64_t(2));

            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(),    0.0f), uint64_t(0));
            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(),    1.0f), uint64_t(1));
            pRow2 = m_pBuilder->CreateInsertElement(pRow2, ConstantFP::get(m_pBuilder->getFloatTy(), 1.8814f), uint64_t(2));
        }

        // output[R]             [Cr]
        // output[G] = convMat * [ Y]
        // output[B]             [Cb]

        Value* pOutputR = m_pBuilder->CreateDotProduct(pRow0, pInputVec);
        Value* pOutputG = m_pBuilder->CreateDotProduct(pRow1, pInputVec);
        Value* pOutputB = m_pBuilder->CreateDotProduct(pRow2, pInputVec);
        Value* pOutputA = m_pBuilder->CreateExtractElement(pImageOp, m_pBuilder->getInt64(3));

        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputR, m_pBuilder->getInt64(0));
        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputG, m_pBuilder->getInt64(1));
        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputB, m_pBuilder->getInt64(2));
        pResult = m_pBuilder->CreateInsertElement(pResult, pOutputA, m_pBuilder->getInt64(3));
        break;
    }

    default:
        llvm_unreachable("Unknown color model!");
        break;
    }

    if (colorModel != SamplerYCbCrModelConversion::YCbCrIdentity)
    {
        pResult = m_pBuilder->CreateFClamp(pResult,
                               m_pBuilder->CreateVectorSplat(4, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0)),
                               m_pBuilder->CreateVectorSplat(4, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)));
    }

    return pResult;
}

// =====================================================================================================================
// Implement transfer form  ST coordinates to UV coordiantes operation
Value* SamplerYCbCrHelper::TransferSTtoUVCoords(
    Value* pST,   // [in] ST coords
    Value* pSize) // [in] with/height
{
    return m_pBuilder->CreateFMul(pST, pSize);
}

// =====================================================================================================================
// Implement the adjustment of UV coordinates when the sample location associated with
// downsampled chroma channels in the X/XY dimension occurs
Value* SamplerYCbCrHelper::YCbCrCalculateImplicitChromaUV(
    ChromaLocation offset, // The sample location associated with downsampled chroma channels in X dimension
    Value*         pUV)    // [in] UV coordinates
{
    if (offset == ChromaLocation::CositedEven)
    {
        pUV = m_pBuilder->CreateFAdd(pUV, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
    }

    return m_pBuilder->CreateFMul(pUV, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
}

// =====================================================================================================================
// Transfer IJ coordinates from UV coordinates
Value* SamplerYCbCrHelper::TransferUVtoIJCoords(
    SamplerFilter filter, // Nearest or Linear sampler filter
    Value*        pUV)    // [in] UV coordinates
{
    assert((filter == SamplerFilter::Nearest) || (filter == SamplerFilter::Linear));

    if (filter == SamplerFilter::Linear)
    {
        pUV = m_pBuilder->CreateFSub(pUV, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
    }

    return m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor, pUV);
}

// =====================================================================================================================
// Calculate UV offset to top-left pixel
Value* SamplerYCbCrHelper::CalculateUVoffset(
    Value* pUV) // [in] UV coordinates
{
    Value* pUVBaised = m_pBuilder->CreateFSub(pUV, ConstantFP::get(m_pBuilder->getFloatTy(), 0.5f));
    Value* pIJ = m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor, pUVBaised);
    return m_pBuilder->CreateFSub(pUVBaised, pIJ);
}

// =====================================================================================================================
// Implement bilinear blending
Value* SamplerYCbCrHelper::BilinearBlend(
    Value*   pAlpha, // [In] Horizen weight
    Value*   pBeta,  // [In] Vertical weight
    Value*   pTL,    // [In] Top-left pixel
    Value*   pTR,    // [In] Top-right pixel
    Value*   pBL,    // [In] Bottom-left pixel
    Value*   pBR)    // [In] Bottm-right pixel
{
    Value* pTop = m_pBuilder->CreateFMix(pTL, pTR, pAlpha);
    Value* pBot = m_pBuilder->CreateFMix(pBL, pBR, pAlpha);

    return m_pBuilder->CreateFMix(pTop, pBot, pBeta);
}