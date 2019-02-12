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
    if (Result) {
        // Do not make difference between signed and unsigned for control flow
        // only
        if (controlFlowOnly && isa<ICmpInst>(L) && isa<ICmpInst>(R)) {
            auto *ICmpL = dyn_cast<ICmpInst>(L);
            auto *ICmpR = dyn_cast<ICmpInst>(R);
            if (ICmpL->getUnsignedPredicate()
                    == ICmpR->getUnsignedPredicate()) {
                return 0;
            }
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

/// Detect cast instructions and ignore them when comparing the control flow
/// only.
/// Note: this function was copied from FunctionComparator.
int DifferentialFunctionComparator::cmpBasicBlocks(const BasicBlock *BBL,
        const BasicBlock *BBR) const {
    BasicBlock::const_iterator InstL = BBL->begin(), InstLE = BBL->end();
    BasicBlock::const_iterator InstR = BBR->begin(), InstRE = BBR->end();

    do {
        bool needToCmpOperands = true;

        // Skip bitcast instructions.
        // Note: this has to be done before calling cmpOperations, because
        // otherwise the serial number counter used by cmpValues would be
        // increased, causing the next instruction to compare as non-equal.
        if (controlFlowOnly)
            if (InstL->isCast() && InstR->isCast()) {
                InstL++; InstR++;
                continue;
            } else if (InstL->isCast()) {
                InstL++;
                continue;
            } else if (InstR->isCast()) {
                InstR++;
                continue;
            }

        if (int Res = cmpOperations(&*InstL, &*InstR, needToCmpOperands))
           return Res;
        if (needToCmpOperands) {
            assert(InstL->getNumOperands() == InstR->getNumOperands());

            for (unsigned i = 0, e = InstL->getNumOperands(); i != e; ++i) {
                Value *OpL = InstL->getOperand(i);
                Value *OpR = InstR->getOperand(i);
                if (int Res = cmpValues(OpL, OpR))
                    return Res;
                // cmpValues should ensure this is true.
                assert(cmpTypes(OpL->getType(), OpR->getType()) == 0);
            }
        }

        ++InstL;
        ++InstR;
    } while (InstL != InstLE && InstR != InstRE);

    if (InstL != InstLE && InstR == InstRE)
        return 1;
    if (InstL == InstLE && InstR != InstRE)
        return -1;
    return 0;
}

/// Handle values generated from macros and enums whose value changed.
/// The new values are pre-computed by DebugInfo.
/// Also handles comparing in case at least one of the values is a cast -
/// when comparing the control flow only, it compares the original value instead
/// of the cast.
int DifferentialFunctionComparator::cmpValues(const Value *L,
                                              const Value *R) const {
    // Detect casts and use the original value instead when comparing the
    // control flow only.
    const CastInst *CIL = dyn_cast<CastInst>(L);
    const CastInst *CIR = dyn_cast<CastInst>(R);

    if (CIL && CIR && controlFlowOnly) {
        // Both instruction are casts - compare the original values before
        // the cast
        return cmpValues(CIL->getOperand(0), CIR->getOperand(0));
    } else if (CIL) {
        // The left value is a cast - use the original value of it in the
        // comparison
        return cmpValues(CIL->getOperand(0), R);
    } else if (CIR) {
        // The right value is a cast - use the original value of it in the
        // comparison
        return cmpValues(L, CIR->getOperand(0));
    }

    int result = FunctionComparator::cmpValues(L, R);
    if (result) {
        if (isa<Constant>(L) && isa<Constant>(R)) {
            auto *ConstantL = dyn_cast<Constant>(L);
            auto *ConstantR = dyn_cast<Constant>(R);
            auto MacroMapping = DI->MacroConstantMap.find(ConstantL);
            if (MacroMapping != DI->MacroConstantMap.end() &&
                    MacroMapping->second == valueAsString(ConstantR))
                return 0;
        } else if (isa<BasicBlock>(L) && isa<BasicBlock>(R)) {
            // In case functions have different numbers of BBs, they may be
            // compared as unequal here. However, this can be caused by moving
            // part of the functionality into a function and hence we'll
            // treat the BBs as equal here to continue comparing and maybe
            // try inlining.
            // We also need to synchronize number maps since they now may have
            // different size.
            if (sn_mapL.size() != sn_mapR.size()) {
                auto *smaller = sn_mapL.size() < sn_mapR.size() ? &sn_mapL
                                                                : &sn_mapR;
                while (sn_mapL.size() != sn_mapR.size())
                    smaller->insert(std::make_pair(
                            nullptr,
                            sn_mapL.size()));
            }
            return 0;
        }
    }
    return result;
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
    // Compare integer types as the same when comparing the control flow only.
    if (L->isIntegerTy() && R->isIntegerTy() && controlFlowOnly)
        return 0;

    if (!L->isArrayTy() || !R->isArrayTy() || !controlFlowOnly)
        return FunctionComparator::cmpTypes(L, R);

    ArrayType *AL = dyn_cast<ArrayType>(L);
    ArrayType *AR = dyn_cast<ArrayType>(R);

    return cmpTypes(AL->getElementType(), AR->getElementType());
}

/// Do not compare bitwidth when comparing the control flow only.
int DifferentialFunctionComparator::cmpAPInts(const APInt &L, const APInt &R)
    const {
    int Result = FunctionComparator::cmpAPInts(L, R);
    if (!controlFlowOnly || !Result) {
        return Result;
    } else {
        // The function ugt uses APInt::compare, which can compare only integers
        // of the same bitwidth. When we want to also compare integers of
        // different bitwidth, a different approach has to be used.
        return cmpNumbers(L.getZExtValue(), R.getZExtValue());
    }

    return 0;
}