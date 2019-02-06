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
/// Uses data saved in StructFieldNames.
int DifferentialFunctionComparator::cmpGEPs(
        const GEPOperator *GEPL,
        const GEPOperator *GEPR) const {
    int OriginalResult = FunctionComparator::cmpGEPs(GEPL, GEPR);

    if(OriginalResult == 0)
        // The original function says the GEPs are equal - return the value
        return OriginalResult;

    if (!isa<StructType>(GEPL->getSourceElementType()) ||
        !isa<StructType>(GEPR->getSourceElementType()))
       // One of the types in not a structure - the original function is
       // sufficient for correct comparison
       return OriginalResult;

    if (getStructTypeName(dyn_cast<StructType>(
                GEPL->getSourceElementType())) !=
            getStructTypeName(dyn_cast<StructType>(
                GEPR->getSourceElementType())))
        // Different structure names - the indices may be same by coincidence,
        // therefore index comparison can't be used
        return OriginalResult;

    unsigned int ASL = GEPL->getPointerAddressSpace();
    unsigned int ASR = GEPR->getPointerAddressSpace();

    if (int Res = cmpNumbers(ASL, ASR))
        return Res;

    if (int Res = cmpNumbers(GEPL->getNumIndices(), GEPR->getNumIndices()))
        return Res;

    if (GEPL->hasAllConstantIndices() && GEPR->hasAllConstantIndices()) {
        std::vector<Value *> IndicesL;
        std::vector<Value *> IndicesR;

        const GetElementPtrInst *GEPiL = dyn_cast<GetElementPtrInst>(GEPL);
        const GetElementPtrInst *GEPiR = dyn_cast<GetElementPtrInst>(GEPR);

        if (!GEPiL || !GEPiR)
            return OriginalResult;

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
                if (int Res = cmpValues(idxL->get(), idxR->get()))
                    return Res;

                IndicesL.push_back(*idxL);
                IndicesR.push_back(*idxR);

                continue;
            }

            // The indexed type is a structure type - compare the names of the
            // structure members from StructFieldNames.
            auto MemberNameL = DI->StructFieldNames.find(
                {dyn_cast<StructType>(ValueTypeL),
                NumericIndexL.getZExtValue()});

            auto MemberNameR = DI->StructFieldNames.find(
                {dyn_cast<StructType>(ValueTypeR),
                NumericIndexR.getZExtValue()});

            if (MemberNameL == DI->StructFieldNames.end() ||
                MemberNameR == DI->StructFieldNames.end() ||
                !MemberNameL->second.equals(MemberNameR->second))
                if (int Res = cmpValues(idxL->get(), idxR->get()))
                    return Res;

            IndicesL.push_back(*idxL);
            IndicesR.push_back(*idxR);
        }
    } else
        // Indicies can't be compared by name, because they are not constant
        return OriginalResult;

    return 0;
}


/// Remove chosen attributes from the attribute set at the given index of
/// the given attribute list. Since attribute lists are immutable, they must be
/// copied all over.
AttributeList cleanAttributes(AttributeList AS, unsigned Idx, LLVMContext &C) {
    AttributeList result = AS;
    result = result.removeAttribute(C, Idx, Attribute::AttrKind::AlwaysInline);
    result = result.removeAttribute(C, Idx, Attribute::AttrKind::InlineHint);
    result = result.removeAttribute(C, Idx, Attribute::AttrKind::NoInline);
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

/// Compare allocation instructions using separate cmpAllocs function in case
/// standard comparison returns something other than zero.
int DifferentialFunctionComparator::cmpOperations(
    const Instruction *L, const Instruction *R, bool &needToCmpOperands) const {
    int Result = FunctionComparator::cmpOperations(L, R, needToCmpOperands);

    // Check whether the instruction is a call instruction.
    if (isa<CallInst>(L) || isa<CallInst>(R)) {
        if (isa<CallInst>(L) && isa<CallInst>(R)) {
            const CallInst *CL = dyn_cast<CallInst>(L);
            const CallInst *CR = dyn_cast<CallInst>(R);

            Function *CalledL = CL->getCalledFunction();
            Function *CalledR = CR->getCalledFunction();
            if (CalledL && CalledR
                    && CalledL->getName() == CalledR->getName()) {
                // Check whether both instructions call an alloc function.
                if (isAllocFunction(*CalledL)) {
                    if (!cmpAllocs(CL, CR, needToCmpOperands))
                        return 0;
                }

                if (Result && controlFlowOnly &&
                        abs(CL->getNumOperands() - CR->getNumOperands()) == 1) {
                    needToCmpOperands = false;
                    return cmpCallsWithExtraArg(CL, CR);
                }
            }
        } else {
            // If just one of the instructions is a call, it is possible that
            // some logic has been moved into a function. We'll try to inline
            // that function and compare again.
            const CallInst *Call = dyn_cast<CallInst>(isa<CallInst>(L) ? L : R);
            ModComparator->tryInline = Call->getCalledFunction();
        }
    }
    return Result;
}

/// Handle comparing of memory allocation function in cases where the size
/// of the composite type is different.
int DifferentialFunctionComparator::cmpAllocs(
    const CallInst* CL, const CallInst* CR, bool &needToCmpOperands) const {
     // Look whether the sizes for allocation match. If yes, then return zero
     // (ignore flags).
     if (cmpValues(CL->op_begin()->get(), CR->op_begin()->get()) == 0) {
         needToCmpOperands = false;
         return 0;
     }

    // The instruction is a call instrution calling the function kzalloc. Now
    // look whether the next instruction is a BitCastInst casting to a structure
    // type.
    if (!isa<BitCastInst>(CL->getNextNode()) ||
        !isa<BitCastInst>(CR->getNextNode()))
        return 1;

    // Check if kzalloc has constant size of the allocated memory
    if (!isa<ConstantInt>(CL->getOperand(0)) ||
        !isa<ConstantInt>(CR->getOperand(0)))
        return 1;

    const BitCastInst *NextInstL = dyn_cast<BitCastInst>(CL->getNextNode());
    const BitCastInst *NextInstR = dyn_cast<BitCastInst>(CR->getNextNode());

    // The kzalloc instruction is followed by a bitcast instruction,
    // therefore it is possible to compare the structure type itself
    PointerType *PTyL =
        dyn_cast<PointerType>(dyn_cast<BitCastInst>(NextInstL)->getDestTy());
    PointerType *PTyR =
        dyn_cast<PointerType>(dyn_cast<BitCastInst>(NextInstR)->getDestTy());

    if (!isa<StructType>(PTyL->getElementType()) ||
        !isa<StructType>(PTyR->getElementType()))
        return 1;

    StructType *STyL = dyn_cast<StructType>(PTyL->getElementType());
    StructType *STyR = dyn_cast<StructType>(PTyR->getElementType());

    // Look whether the first argument of kzalloc corresponds to the type size.
    uint64_t TypeSizeL =
            dyn_cast<ConstantInt>(CL->getOperand(0))->getZExtValue();
    uint64_t TypeSizeR =
            dyn_cast<ConstantInt>(CR->getOperand(0))->getZExtValue();

    if (TypeSizeL != LayoutL.getTypeStoreSize(STyL) ||
        TypeSizeR != LayoutR.getTypeStoreSize(STyR))
        return 1;

    // Look whether the types the memory is allocated for are the same.
    if (STyL->getName() != STyR->getName())
        return 1;

    // As of now this function ignores kzalloc flags, therefore the argument
    // comparison is complete.
    needToCmpOperands = false;
    return 0;
}

int DifferentialFunctionComparator::cmpCallsWithExtraArg(
        const CallInst *CL,
        const CallInst *CR) const {
    // Distinguish which call has more parameters
    const CallInst *CallExtraArg;
    const CallInst *CallOther;
    if (CL->getNumOperands() > CR->getNumOperands()) {
        CallExtraArg = CL;
        CallOther = CR;
    } else {
        CallExtraArg = CR;
        CallOther = CL;
    }

    // The last extra argument must be 0 (false) or NULL
    auto *LastOp = CallExtraArg->getOperand(CallExtraArg->getNumOperands() - 2);
    if (auto ConstLastOp = dyn_cast<Constant>(LastOp)) {
        if (!(ConstLastOp->isNullValue() || ConstLastOp->isZeroValue()))
            return 1;

        // Compare function return types (types of the call instructions)
        if (int Res = cmpTypes(CallExtraArg->getType(), CallOther->getType()))
            return Res;

        // For each argument (except the extra one), compare its type and value.
        // Last argument is not compared since it is the called function.
        for (unsigned i = 0, e = CallOther->getNumOperands() - 1; i != e; ++i) {
            auto Arg1 = CallExtraArg->getOperand(i);
            auto Arg2 = CallOther->getOperand(i);
            if (int Res = cmpTypes(Arg1->getType(), Arg2->getType()))
                return Res;
            if (int Res = cmpValues(Arg1, Arg2))
                return Res;
        }
        return 0;
    }
    return 1;
}

/// Compares array types with equivalent element types as equal when
/// comparing the control flow only.
int DifferentialFunctionComparator::cmpTypes(Type *L, Type *R) const {
    if (!L->isArrayTy() || !R->isArrayTy() || !controlFlowOnly)
        return FunctionComparator::cmpTypes(L, R);

    ArrayType *AL = dyn_cast<ArrayType>(L);
    ArrayType *AR = dyn_cast<ArrayType>(R);

    return cmpTypes(AL->getElementType(), AR->getElementType());
}
