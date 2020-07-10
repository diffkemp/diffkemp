//===-- FieldAccessUtils.cpp - functions for working with field accesses --===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementations of functions for working with field
/// access operations.
///
//===----------------------------------------------------------------------===//

#include "FieldAccessUtils.h"
#include <llvm/IR/Module.h>

/// Finds the beginning of a field access operation from an arbitrary
/// instruction in it.
const GetElementPtrInst *getFieldAccessStart(const Value *Val) {
    if (auto Cast = dyn_cast<CastInst>(Val)) {
        // Val is a cast - continue the search on its operand.
        // Note: a field access operation starts with a GEP, therefore this
        // function doesn't return the cast if its operand is not a part of the
        // operation.
        return getFieldAccessStart(Cast->getOperand(0));
    }

    if (auto GEP = dyn_cast<GetElementPtrInst>(Val)) {
        // Val is a GEP - try to continue the search on its operand (there may
        // be another GEP in the field access operation before it).
        // If this search fails, it means that there is no GEP in the operation
        // before this one, therefore this one is the beginning of the
        // operation.
        auto PrevGEP = getFieldAccessStart(GEP->getOperand(0));
        if (PrevGEP)
            return PrevGEP;
        else
            return GEP;
    }

    // Only casts and GEPs are considered parts of an field access operation.
    return nullptr;
}

/// Check if the given instruction is a memory access (i.e. a GEP or a pointer
/// bitcast) to the given pointer. If so, return true and set the size of the
/// offset that the instruction adds to the pointer, otherwise return false.
bool isConstantMemoryAccessToPtr(const Instruction *Inst,
                                 const Value *Ptr,
                                 int &Offset) {
    if (isa<GetElementPtrInst>(Inst)
        || ((isa<CastInst>(Inst) && !isa<PtrToIntInst>(Inst)))) {
        if (Ptr != Inst->getOperand(0))
            return false;
        if (auto *GEP = dyn_cast<GetElementPtrInst>(Inst)) {
            APInt InstOffset(64, 0);
            if (!GEP->accumulateConstantOffset(
                        Inst->getParent()->getModule()->getDataLayout(),
                        InstOffset)) {
                return false;
            }
            Offset = InstOffset.getZExtValue();
        } else {
            Offset = 0;
        }
        return true;
    }
    return false;
}

/// Returns true if NextInst is a part of the same field access operation as
/// Inst and follows it in the operation.
/// This is similar to isConstantMemoryAccessToPtr with the difference being
/// that the offset is not computed and the access doesn't have to be constant.
bool isFollowingFieldAccessInstruction(const Instruction *NextInst,
                                       const Instruction *Inst) {
    if (isa<GetElementPtrInst>(NextInst)
        || ((isa<CastInst>(NextInst) && !isa<PtrToIntInst>(NextInst)))) {
        if (Inst == NextInst->getOperand(0))
            return true;
    }
    return false;
}

/// Extracts source types for all GEPs in a field access operation.
std::vector<Type *> getFieldAccessSourceTypes(const GetElementPtrInst *FA) {
    std::vector<Type *> TypeVec;
    const Instruction *LastFAInst = nullptr;
    for (const Instruction *I = FA; !I->isTerminator(); I = I->getNextNode()) {
        if (LastFAInst && !isFollowingFieldAccessInstruction(I, LastFAInst))
            continue;
        LastFAInst = I;
        if (auto GEP = dyn_cast<GetElementPtrInst>(I)) {
            TypeVec.push_back(GEP->getSourceElementType());
            // If the GEP has a GEP constant expression as its argument, add it
            // to the vector.
            // Note: two casts have to be used because the class for a GEP
            // constant expression (GetElementPtrConstantExpr) is private.
            if (auto InnerConstExpr =
                        dyn_cast<ConstantExpr>(GEP->getPointerOperand())) {
                if (auto InnerGEP = dyn_cast<GEPOperator>(InnerConstExpr))
                    TypeVec.push_back(InnerGEP->getSourceElementType());
            }
        }
    }
    return TypeVec;
}
