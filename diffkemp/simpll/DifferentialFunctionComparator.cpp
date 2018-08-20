//=== DifferentialFunctionComparator.cpp - Comparing functions for equality ==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementation of specific comparison functions used
/// to compare functions from different modules on equality.
///
//===----------------------------------------------------------------------===//

#include "DifferentialFunctionComparator.h"
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

/// Compare GEPs. This code is copied from FunctionComparator::cmpGEPs since it
/// was not possible to simply call the original function.
/// Handles offset between matching GEP indices in the compared modules.
/// Assumes that the offset alignment is stored inside the GEP instruction as
/// metadata.
int DifferentialFunctionComparator::cmpGEPs(
        const GEPOperator *GEPL,
        const GEPOperator *GEPR) const {
    unsigned int ASL = GEPL->getPointerAddressSpace();
    unsigned int ASR = GEPR->getPointerAddressSpace();

    if (int Res = cmpNumbers(ASL, ASR))
        return Res;

    // When we have target data, we can reduce the GEP down to the value in
    // bytes added to the address.
    const DataLayout &DL = FnL->getParent()->getDataLayout();
    unsigned BitWidth = DL.getPointerSizeInBits(ASL);
    APInt OffsetL(BitWidth, 0), OffsetR(BitWidth, 0);
    if (GEPL->accumulateConstantOffset(DL, OffsetL) &&
            GEPR->accumulateConstantOffset(DL, OffsetR)) {
        if (auto GEPLInstr = dyn_cast<GetElementPtrInst>(GEPL)) {
            auto GEPRInstr = dyn_cast<GetElementPtrInst>(GEPR);
            auto TypeItL = gep_type_begin(GEPLInstr);
            auto TypeItR = gep_type_begin(GEPRInstr);
            auto ix = GEPL->idx_begin();
            for (unsigned i = 0; i < GEPL->getNumIndices();
                 ++i, ++ix, TypeItL++, TypeItR++) {
                if (auto idxMD = GEPLInstr->getMetadata(
                        "idx_align_" + std::to_string(i))) {
                    // Add alignment of the offset computed from debug info
                    auto *ixAlign = dyn_cast<ConstantInt>(
                            dyn_cast<ConstantAsMetadata>(
                                    idxMD->getOperand(0))->getValue());

                    // Get value of the old offset from the current index
                    auto oldOffset = DL.getStructLayout(
                            TypeItL.getStructType())->getElementOffset(
                            dyn_cast<ConstantInt>(ix)->getZExtValue());
                    // Get value of the new offset (from metadata)
                    auto newOffset = DL.getStructLayout(
                            TypeItR.getStructType())->getElementOffset(
                            ixAlign->getValue().getZExtValue());
                    OffsetL += APInt(BitWidth, newOffset - oldOffset, true);
                }
            }
        }
        return cmpAPInts(OffsetL, OffsetR);
    }
    if (int Res = cmpTypes(GEPL->getSourceElementType(),
                           GEPR->getSourceElementType()))
        return Res;

    if (int Res = cmpNumbers(GEPL->getNumOperands(), GEPR->getNumOperands()))
        return Res;

    for (unsigned i = 0, e = GEPL->getNumOperands(); i != e; ++i) {
        if (int Res = cmpValues(GEPL->getOperand(i), GEPR->getOperand(i)))
            return Res;
    }

    return 0;
}