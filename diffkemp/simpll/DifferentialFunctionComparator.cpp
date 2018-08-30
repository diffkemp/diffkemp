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
#include <llvm/Support/raw_ostream.h>

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

    if (int Res = cmpNumbers(GEPL->getNumIndices(), GEPR->getNumIndices()))
        return Res;

    bool allIndicesSame = true;

    // Check whether the indices are identical
    for (auto idxL = GEPL->idx_begin(), idxR = GEPR->idx_begin();
         idxL != GEPL->idx_end() && idxR != GEPR->idx_end();
         ++idxL, ++idxR)
        if (cmpValues(idxL->get(), idxR->get()))
            allIndicesSame = false;

    if (!allIndicesSame &&
        GEPL->hasAllConstantIndices() && GEPR->hasAllConstantIndices()) {
        allIndicesSame = true;

        std::vector<Value *> IndicesL;
        std::vector<Value *> IndicesR;

        const GetElementPtrInst *GEPiL = dyn_cast<GetElementPtrInst>(GEPL);
        const GetElementPtrInst *GEPiR = dyn_cast<GetElementPtrInst>(GEPR);

        if (!GEPiL || !GEPiR)
            return FunctionComparator::cmpGEPs(GEPL, GEPR);

        for (auto idxL = GEPL->idx_begin(), idxR = GEPR->idx_begin();
         idxL != GEPL->idx_end() && idxR != GEPR->idx_end();
         ++idxL, ++idxR) {
            auto ValueTypeL = GEPiL->getIndexedType(GEPiL->getSourceElementType(),
                                                    ArrayRef<Value *>(
                                                            IndicesL));

            auto ValueTypeR = GEPiR->getIndexedType(GEPiR->getSourceElementType(),
                                                    ArrayRef<Value *>(
                                                            IndicesR));

            auto NumericIndexL = dyn_cast<ConstantInt>(idxL->get())->getValue();
            auto NumericIndexR = dyn_cast<ConstantInt>(idxR->get())->getValue();

            if (!ValueTypeL->isStructTy() || !ValueTypeR->isStructTy()) {
                // If the indexed type is not a structure type, the indices have
                // to match in order for the instructions to be equivalent
                if (!cmpValues(idxL->get(), idxR->get()))
                    allIndicesSame = false;

                IndicesL.push_back(*idxL);
                IndicesR.push_back(*idxR);
                continue;
            }

            // The indexed type is a structure type - compare the names of the
            // structure members from the ElementIndexToNameMap
            auto MemberNameL = EINM->find({dyn_cast<StructType>(ValueTypeL),
                NumericIndexL.getZExtValue()});

            auto MemberNameR = EINM->find({dyn_cast<StructType>(ValueTypeR),
                NumericIndexR.getZExtValue()});

            if (MemberNameL == EINM->end() || MemberNameL == EINM->end() ||
               !MemberNameL->second.equals(MemberNameR->second))
                if (cmpValues(idxL->get(), idxR->get()))
                    allIndicesSame = false;

            IndicesL.push_back(*idxL);
            IndicesR.push_back(*idxR);
        }
    }

    if (!allIndicesSame)
        return FunctionComparator::cmpGEPs(GEPL, GEPR);

    return 0;
}


/// Remove chosen attributes from the attribute set at the given index of
/// the given attribute list. Since attribute lists are immutable, they must be
/// copied all over.
AttributeList cleanAttributes(AttributeList AS, unsigned Idx, LLVMContext &C) {
    AttributeList result = AS;
    result = result.removeAttribute(C, Idx, Attribute::AttrKind::AlwaysInline);
    result = result.removeAttribute(C, Idx, Attribute::AttrKind::InlineHint);
    return result;
}

int DifferentialFunctionComparator::cmpAttrs(const AttributeList L,
                                             const AttributeList R) const {
    AttributeList LNew = L;
    AttributeList RNew = R;
    for (unsigned i = L.index_begin(), e = L.index_end(); i != e; ++i) {
        LNew = cleanAttributes(LNew, i, LNew.getContext());
        RNew = cleanAttributes(RNew, i, RNew.getContext());
    }
    return FunctionComparator::cmpAttrs(LNew, RNew);
}
