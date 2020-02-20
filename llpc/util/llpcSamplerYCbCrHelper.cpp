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
* @file  llpcSamplerYCbCrHelper.cpp
* @brief LLPC source file: Implementation of LLPC utility class SamplerYCbCrHelper
***********************************************************************************************************************
*/
#include "llpcSamplerYCbCrHelper.h"

#include "llpcInternal.h"
#include "llpcTargetInfo.h"

#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsAMDGPU.h"

using namespace Llpc;
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

        Instruction* pImageOp = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));
        pResult = m_pBuilder->CreateShuffleVector(pImageOp, pImageOp, { 0, 2 });
    }
    else if (wrapInfo.planeNum == 2)
    {
        pSampleInfo->pImageDesc = wrapInfo.pImageDesc2;
        Instruction* pImageOp = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));
        pResult = m_pBuilder->CreateShuffleVector(pImageOp, pImageOp, { 0, 2 });
    }
    else if (wrapInfo.planeNum == 3)
    {
        pSampleInfo->pImageDesc = wrapInfo.pImageDesc2;
        Instruction* pImageOp1 = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));

        pSampleInfo->pImageDesc = wrapInfo.pImageDesc3;
        Instruction* pImageOp2 = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChroma, pSampleInfo));
        pResult = m_pBuilder->CreateShuffleVector(pImageOp2, pImageOp1, { 0, 6 });
    }
    else
    {
        LLPC_NEVER_CALLED();
    }

    return pResult;
}

// =====================================================================================================================
// Create YCbCr Reconstruct linear X chroma sample
Value* SamplerYCbCrHelper::YCbCrReconstructLinearXChromaSample(
    XChromaSampleInfo& xChromaInfo) // [In] Infomation for downsampled chroma channels in X dimension
{
    YCbCrSampleInfo* pSampleInfo = xChromaInfo.pYCbCrInfo;
    Value* pIsEvenI = m_pBuilder->CreateICmpEQ(m_pBuilder->CreateSMod(m_pBuilder->CreateFPToSI(xChromaInfo.pI, m_pBuilder->getInt32Ty()), m_pBuilder->getInt32(2)), m_pBuilder->getInt32(0));

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
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0), ConstantFP::get(m_pBuilder->getFloatTy(), 0.5));
    }
    else
    {
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.25), ConstantFP::get(m_pBuilder->getFloatTy(), 0.75));
    }

    Value* pT = m_pBuilder->CreateFDiv(xChromaInfo.pJ, xChromaInfo.pChromaHeight);

    SmallVector<Value*, 4> coordsChromaA;
    pSampleInfo->pImageDesc = xChromaInfo.pImageDesc1;
    coordsChromaA.push_back(m_pBuilder->CreateFDiv(pSubI, xChromaInfo.pChromaWidth));
    coordsChromaA.push_back(pT);
    Instruction* pImageOpA = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaA, pSampleInfo));

    SmallVector<Value*, 4> coordsChromaB;
    coordsChromaB.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), xChromaInfo.pChromaWidth));
    coordsChromaB.push_back(pT);
    Instruction* pImageOpB = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaB, pSampleInfo));

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

    Value* pIsEvenI = m_pBuilder->CreateICmpEQ(m_pBuilder->CreateSMod(m_pBuilder->CreateFPToSI(xyChromaInfo.pI, m_pBuilder->getInt32Ty()), m_pBuilder->getInt32(2)), m_pBuilder->getInt32(0));
    Value* pIsEvenJ = m_pBuilder->CreateICmpEQ(m_pBuilder->CreateSMod(m_pBuilder->CreateFPToSI(xyChromaInfo.pJ, m_pBuilder->getInt32Ty()), m_pBuilder->getInt32(2)), m_pBuilder->getInt32(0));

    Value* pSubI = m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor,
                                        m_pBuilder->CreateFDiv(xyChromaInfo.pI, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0)));
    Value* pSubJ = m_pBuilder->CreateUnaryIntrinsic(Intrinsic::floor,
                                        m_pBuilder->CreateFDiv(xyChromaInfo.pJ, ConstantFP::get(m_pBuilder->getFloatTy(), 2.0)));

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
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0), ConstantFP::get(m_pBuilder->getFloatTy(), 0.5));
    }
    else
    {
        pAlpha = m_pBuilder->CreateSelect(pIsEvenI, ConstantFP::get(m_pBuilder->getFloatTy(), 0.25), ConstantFP::get(m_pBuilder->getFloatTy(), 0.75));
    }

    Value* pBeta = nullptr;
    if (xyChromaInfo.yChromaOffset == ChromaLocation::CositedEven)
    {
        pBeta = m_pBuilder->CreateSelect(pIsEvenJ, ConstantFP::get(m_pBuilder->getFloatTy(), 0.0), ConstantFP::get(m_pBuilder->getFloatTy(), 0.5));
    }
    else
    {
        pBeta = m_pBuilder->CreateSelect(pIsEvenJ, ConstantFP::get(m_pBuilder->getFloatTy(), 0.25), ConstantFP::get(m_pBuilder->getFloatTy(), 0.75));
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
        Instruction* pTL = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaTL, pSampleInfo));

        // Sample TR
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(pSubJ, pHeight));
        Instruction* pTR = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaTR, pSampleInfo));

        // Sample BL
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(pSubI, pWidth));
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        Instruction* pBL = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaBL, pSampleInfo));

        // Sample BR
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        Instruction* pBR = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaBR, pSampleInfo));

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
        Value* pTLb = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaTL, pSampleInfo));

        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pTLr = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaTL, pSampleInfo));
        Value* pTL = m_pBuilder->CreateShuffleVector(pTLr, pTLb, { 0, 6});

        // Sample TR
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaTR.push_back(m_pBuilder->CreateFDiv(pSubJ, pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pTRb = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaTR, pSampleInfo));

        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pTRr = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaTR, pSampleInfo));
        Value* pTR = m_pBuilder->CreateShuffleVector(pTRr, pTRb, { 0, 6});

        // Sample BL
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(pSubI, pWidth));
        coordsChromaBL.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pBLb = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaBL, pSampleInfo));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pBLr = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaBL, pSampleInfo));
        Value* pBL = m_pBuilder->CreateShuffleVector(pBLr, pBLb, { 0, 6});

        // Sample BR
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubI, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pWidth));
        coordsChromaBR.push_back(m_pBuilder->CreateFDiv(m_pBuilder->CreateFAdd(pSubJ, ConstantFP::get(m_pBuilder->getFloatTy(), 1.0)), pHeight));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc1;
        Value* pBRb = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaBR, pSampleInfo));
        pSampleInfo->pImageDesc = xyChromaInfo.pImageDesc2;
        Value* pBRr = static_cast<Instruction*>(YCbCrCreateImageSampleInternal(coordsChromaBR, pSampleInfo));
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

// =====================================================================================================================
// Replace [beginBit, beginBit + adjustBits) bits with data in specific word
Value* SamplerYCbCrHelper::ReplaceBitsInWord(
    Value*      pWord,      // [in] Target wowrd
    uint32_t    beginBit,   // The first bit to be replaced
    uint32_t    adjustBits, // The number of bits should be replaced
    Value*      pData)      // [in] The data used to replace specific bits
{
    uint32_t mask = ((1 << adjustBits) - 1) << beginBit;

    Value* pMask = m_pBuilder->getInt32(mask);
    Value* pInvMask = m_pBuilder->getInt32(~mask);
    Value* pBeginBit = m_pBuilder->getInt32(beginBit);

    if (auto pWordVecTy = dyn_cast<VectorType>(pWord->getType()))
    {
        if (isa<VectorType>(pData->getType()) == false)
        {
            pData = m_pBuilder->CreateVectorSplat(pWordVecTy->getNumElements(), pData);
        }
        pMask = m_pBuilder->CreateVectorSplat(pWordVecTy->getNumElements(), pMask);
        pInvMask = m_pBuilder->CreateVectorSplat(pWordVecTy->getNumElements(), pInvMask);
        pBeginBit = m_pBuilder->CreateVectorSplat(pWordVecTy->getNumElements(), pBeginBit);
    }
    else
    {
        if (auto pDataVecTy = dyn_cast<VectorType>(pData->getType()))
        {
            pWord = m_pBuilder->CreateVectorSplat(pDataVecTy->getNumElements(), pWord);
            pMask = m_pBuilder->CreateVectorSplat(pDataVecTy->getNumElements(), pMask);
            pInvMask = m_pBuilder->CreateVectorSplat(pDataVecTy->getNumElements(), pInvMask);
            pBeginBit = m_pBuilder->CreateVectorSplat(pDataVecTy->getNumElements(), pBeginBit);
        }
    }

    Value* pNewWord = m_pBuilder->CreateAnd(pWord, pInvMask); // (pWord & ~mask)
    pData = m_pBuilder->CreateAnd(m_pBuilder->CreateShl(pData, pBeginBit), pMask); // ((pData << beginBit) & mask)
    pNewWord = m_pBuilder->CreateOr(pNewWord, pData);
    return pNewWord;
}

// =====================================================================================================================
// Generate sampler descriptor for YCbCr conversion
Value* SamplerYCbCrHelper::YCbCrGenerateSamplerDesc(
    Value*           pSamplerDesc,                // [in] Sampler descriptor
    SamplerFilter    filter,                      // The type of sampler filter
    bool             forceExplicitReconstruction) // Enable/Disable force explict chroma reconstruction
{
    Value* pSamplerDescNew = UndefValue::get(pSamplerDesc->getType());

    Value* pSampWord0 = m_pBuilder->CreateExtractElement(pSamplerDesc, m_pBuilder->getInt64(0));
    Value* pSampWord1 = m_pBuilder->CreateExtractElement(pSamplerDesc, m_pBuilder->getInt64(1));
    Value* pSampWord2 = m_pBuilder->CreateExtractElement(pSamplerDesc, m_pBuilder->getInt64(2));
    Value* pSampWord3 = m_pBuilder->CreateExtractElement(pSamplerDesc, m_pBuilder->getInt64(3));

    /// Determines if "TexFilter" should be ignored or not.
    // enum class TexFilterMode : uint32
    // {
    //     Blend = 0x0, ///< Use the filter method specified by the TexFilter enumeration
    //     Min   = 0x1, ///< Use the minimum value returned by the sampler, no blending op occurs
    //     Max   = 0x2, ///< Use the maximum value returned by the sampler, no blending op occurs
    // };
    pSampWord0 = ReplaceBitsInWord(pSampWord0, 30, 2, m_pBuilder->getInt32(0b00)); // Force use blend mode

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
        pSampWord2 = ReplaceBitsInWord(pSampWord2, 20, 4, m_pBuilder->getInt32(0b0000));
    }
    else //filter == SamplerFilter::Linear
    {
        pSampWord2 = ReplaceBitsInWord(pSampWord2, 20, 4, m_pBuilder->getInt32(0b0101));
    }

    pSamplerDescNew = m_pBuilder->CreateInsertElement(pSamplerDescNew, pSampWord0, m_pBuilder->getInt64(0));
    pSamplerDescNew = m_pBuilder->CreateInsertElement(pSamplerDescNew, pSampWord1, m_pBuilder->getInt64(1));
    pSamplerDescNew = m_pBuilder->CreateInsertElement(pSamplerDescNew, pSampWord2, m_pBuilder->getInt64(2));
    pSamplerDescNew = m_pBuilder->CreateInsertElement(pSamplerDescNew, pSampWord3, m_pBuilder->getInt64(3));

    return pSamplerDescNew;
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
        LLPC_NEVER_CALLED();
        return nullptr;
    }
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
        LLPC_NEVER_CALLED();
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
    LLPC_ASSERT((filter == SamplerFilter::Nearest) || (filter == SamplerFilter::Linear));

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