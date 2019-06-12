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
#include "Config.h"
#include "SourceCodeUtils.h"
#include "passes/FunctionAbstractionsGenerator.h"
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <set>

// If a operand of a call instruction is detected to be generated from one of
// these macros, it should be always compared as equal.
std::set<std::string> ignoredMacroList = {
        "__COUNTER__", "__FILE__", "__LINE__", "__DATE__", "__TIME__"
};

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
    } else if (GEPL->getNumIndices() == 1 && GEPR->getNumIndices() == 1) {
        // If there is just a single (non-constant) index, it is an array
        // element access. We do not need to compare source element type since
        // members of the elements are not accessed here.
        // Just the index itself is compared.
        return cmpValues(GEPL->getOperand(1), GEPR->getOperand(1));
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
    result = result.removeAttribute(C, Idx, Attribute::AttrKind::NoUnwind);
    return result;
}

int DifferentialFunctionComparator::cmpAttrs(const AttributeList L,
                                             const AttributeList R) const {
    AttributeList LNew = L;
    AttributeList RNew = R;
    for (unsigned i = L.index_begin(), e = L.index_end(); i != e; ++i) {
        LNew = cleanAttributes(LNew, i, LNew.getContext());
    }
    for (unsigned i = R.index_begin(), e = R.index_end(); i != e; ++i) {
        RNew = cleanAttributes(RNew, i, RNew.getContext());
    }
    LNew = cleanAttributeList(LNew);
    RNew = cleanAttributeList(RNew);
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
            if (CalledL && CalledR) {
                if (CalledL->getName() == CalledR->getName()) {
                    // Check whether both instructions call an alloc function.
                    if (isAllocFunction(*CalledL)) {
                        if (!cmpAllocs(CL, CR)) {
                            needToCmpOperands = false;
                            return 0;
                        }
                    }

                    if (CalledL->getIntrinsicID() == Intrinsic::memset &&
                            CalledR->getIntrinsicID() == Intrinsic::memset) {
                        if (!cmpMemset(CL, CR)) {
                            needToCmpOperands = false;
                            return 0;
                        }
                    }

                    if (Result && controlFlowOnly &&
                            abs(CL->getNumOperands() - CR->getNumOperands())
                                    == 1) {
                        needToCmpOperands = false;
                        return cmpCallsWithExtraArg(CL, CR);
                    }
                }
                if ((Result || CalledL->getName() != CalledR->getName()) &&
                    !CalledL->getName().startswith("simpll") &&
                    !CalledR->getName().startswith("simpll")) {
                    // If the call instructions are different (cmpOperations
                    // doesn't compare the called functions) or the called
                    // functions have different names, try inlining them.
                    ModComparator->tryInline = {CL, CR};
                }
            }
        } else {
            // If just one of the instructions is a call, it is possible that
            // some logic has been moved into a function. We'll try to inline
            // that function and compare again.
            if (isa<CallInst>(L) && !getCalledFunction(
                    dyn_cast<CallInst>(L)->getCalledValue())->getName().
                    startswith("simpll"))
                ModComparator->tryInline = {dyn_cast<CallInst>(L), nullptr};
            else if (isa<CallInst>(R) && !getCalledFunction(
                     dyn_cast<CallInst>(R)->getCalledValue())->getName().
                     startswith("simpll"))
                ModComparator->tryInline = {nullptr, dyn_cast<CallInst>(R)};
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
        // Handle alloca of a structure type with changed layout
        if (isa<AllocaInst>(L) && isa<AllocaInst>(R)) {
            StructType *TypeL = dyn_cast<StructType>(
                    dyn_cast<AllocaInst>(L)->getAllocatedType());
            StructType *TypeR = dyn_cast<StructType>(
                    dyn_cast<AllocaInst>(R)->getAllocatedType());
            if (TypeL && TypeR &&
                    TypeL->getStructName() == TypeR->getStructName())
                return cmpNumbers(dyn_cast<AllocaInst>(L)->getAlignment(),
                                  dyn_cast<AllocaInst>(R)->getAlignment());
        }
    }

    if (Result) {
        auto macroDiffs = findMacroDifferences(L, R);
        ModComparator->DifferingObjects.insert(
            ModComparator->DifferingObjects.end(),
            macroDiffs.begin(), macroDiffs.end());
    }

    return Result;
}

/// Compare structure size with a constant.
/// @return 0 if equal, 1 otherwise.
int DifferentialFunctionComparator::cmpStructTypeSizeWithConstant(
        StructType *Type,
        const Value *Const) const {
    uint64_t ConstValue = dyn_cast<ConstantInt>(Const)->getZExtValue();
    return ConstValue != LayoutL.getTypeStoreSize(Type);
}

/// Handle comparing of memory allocation function in cases where the size
/// of the composite type is different.
int DifferentialFunctionComparator::cmpAllocs(const CallInst *CL,
                                              const CallInst *CR) const {
     // Look whether the sizes for allocation match. If yes, then return zero
     // (ignore flags).
     if (cmpValues(CL->op_begin()->get(), CR->op_begin()->get()) == 0) {
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

    // Get the allocated structure types
    StructType *STyL = getStructType(CL->getNextNode());
    StructType *STyR = getStructType(CR->getNextNode());

    // Return 0 (equality) if both allocated types are structs of the same name
    // and each struct has a size equal to the size of the allocated memory.
    return !STyL || !STyR ||
           cmpStructTypeSizeWithConstant(STyL, CL->getOperand(0)) ||
           cmpStructTypeSizeWithConstant(STyR, CR->getOperand(0)) ||
           STyL->getStructName() != STyR->getStructName();
}

/// Check if the given operation can be ignored (it does not affect semantics).
bool mayIgnore(const Instruction *Inst) {
    return isa<AllocaInst>(Inst) || Inst->isCast();
}

bool mayIgnoreMacro(std::string macro) {
    return ignoredMacroList.find(macro) != ignoredMacroList.end();
}

/// Detect cast instructions and ignore them when comparing the control flow
/// only.
/// Note: this function was copied from FunctionComparator.
int DifferentialFunctionComparator::cmpBasicBlocks(const BasicBlock *BBL,
        const BasicBlock *BBR) const {
    BasicBlock::const_iterator InstL = BBL->begin(), InstLE = BBL->end();
    BasicBlock::const_iterator InstR = BBR->begin(), InstRE = BBR->end();

    while (InstL != InstLE && InstR != InstRE) {
        bool needToCmpOperands = true;

        if (int Res = cmpOperations(&*InstL, &*InstR, needToCmpOperands)) {
            // Some operations not affecting semantics and control flow may be
            // ignored (currently allocas and casts). This may help to handle
            // some small changes that do not affect semantics (it is also
            // useful in combination with function inlining).
            if (controlFlowOnly && (mayIgnore(&*InstL) || mayIgnore(&*InstR))) {
                // Reset serial counters
                sn_mapL.erase(&*InstL);
                sn_mapR.erase(&*InstR);
                // One of the compared operations will be skipped and the
                // comparison will be repeated.
                if (mayIgnore(&*InstL))
                    InstL++;
                else
                    InstR++;
                continue;
            }
            return Res;
        }
        if (needToCmpOperands) {
            assert(InstL->getNumOperands() == InstR->getNumOperands());

            for (unsigned i = 0, e = InstL->getNumOperands(); i != e; ++i) {
                Value *OpL = InstL->getOperand(i);
                Value *OpR = InstR->getOperand(i);
                if (int Res = cmpValues(OpL, OpR)) {
                    // Try to compare operands with the help of C source code
                    std::vector<std::string> CArgsL, CArgsR;
                    // In case both instructions are call instructions, try to
                    // prepare C source argument values to be used in operand
                    // comparison.

                    if (isa<CallInst>(&*InstL) && isa<CallInst>(&*InstR)) {
                        const CallInst *CIL = dyn_cast<CallInst>(&*InstL);
                        const CallInst *CIR = dyn_cast<CallInst>(&*InstR);
                        const Function *CFL = getCalledFunction(
                                CIL->getCalledValue());
                        const Function *CFR = getCalledFunction(
                                CIR->getCalledValue());

                        // Use the appropriate C source call argument extraction
                        // function depending on whether it is an inline asm
                        // call or not.
                        if (CFL->getName().startswith(SimpllInlineAsmPrefix))
                            CArgsL = findInlineAssemblySourceArguments(
                                InstL->getDebugLoc(), InstL->getModule(),
                                ModComparator->AsmToStringMapL[CFL->getName()]);
                        else
                            CArgsL = findFunctionCallSourceArguments(
                                InstL->getDebugLoc(), InstL->getModule(),
                                CFL->getName());

                        if (CFR->getName().startswith(SimpllInlineAsmPrefix))
                            CArgsR = findInlineAssemblySourceArguments(
                                InstR->getDebugLoc(), InstR->getModule(),
                                ModComparator->AsmToStringMapR[CFR->getName()]);
                        else
                            CArgsR = findFunctionCallSourceArguments(
                                InstR->getDebugLoc(), InstR->getModule(),
                                CFL->getName());
                    }

                    if ((CArgsL.size() > i) && (CArgsR.size() > i)) {
                        if (mayIgnoreMacro(CArgsL[i]) &&
                                mayIgnoreMacro(CArgsR[i]) &&
                                (CArgsL[i] == CArgsR[i])) {
                            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Comparing integers as equal "
                                       << "because of correspondence to an "
                                       << "ignored macro\n");
                            Res = 0;
                        }

                        if (StringRef(CArgsL[i]).startswith("sizeof") &&
                            StringRef(CArgsR[i]).startswith("sizeof") &&
                            isa<ConstantInt>(OpL) && isa<ConstantInt>(OpR)) {
                            // Both arguments are sizeofs; look whether they
                            // correspond to a changed size of the same
                            // structure
                            int IntL =
                                dyn_cast<ConstantInt>(OpL)->getZExtValue();
                            int IntR =
                                dyn_cast<ConstantInt>(OpR)->getZExtValue();
                            auto SizeL = ModComparator->StructSizeMapL.find(
                                IntL);
                            auto SizeR = ModComparator->StructSizeMapR.find(
                                IntR);

                            if (SizeL != ModComparator->StructSizeMapL.end() &&
                                SizeR != ModComparator->StructSizeMapR.end() &&
                                SizeL->second == SizeR->second) {
                                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                    dbgs() << "Comparing integers as equal "
                                           << "because of correspondence to "
                                           << "structure type sizes \n");
                                Res = 0;
                            }
                        }
                    }

                    if (Res) {
                        // Try to find macros that could be causing the
                        // difference
                        auto macroDiffs = findMacroDifferences(&*InstL, &*InstR);
                        ModComparator->DifferingObjects.insert(
                            ModComparator->DifferingObjects.end(),
                            macroDiffs.begin(), macroDiffs.end());

                        // Try to find assembly functions causing the difference
                        auto asmDiffs = findAsmDifference(OpL, OpR,
                            BBL->getParent(), BBR->getParent());
                        ModComparator->DifferingObjects.insert(
                            ModComparator->DifferingObjects.end(),
                            asmDiffs.begin(), asmDiffs.end());

                        return Res;
                    }
                }
                // cmpValues should ensure this is true.
                assert(cmpTypes(OpL->getType(), OpR->getType()) == 0);
            }
        }

        ++InstL;
        ++InstR;
    }

    if (InstL != InstLE && InstR == InstRE)
        return 1;
    if (InstL == InstLE && InstR != InstRE)
        return -1;
    return 0;
}

/// Looks for inline assembly differences between the certain values.
/// Note: passing the parent function is necessary in order to properly generate
/// the SyntaxDifference object.
std::vector<SyntaxDifference> DifferentialFunctionComparator::findAsmDifference(
        const Value *L, const Value *R, const Function *ParentL,
        const Function *ParentR) const {
    auto FunL = dyn_cast<Function>(L);
    auto FunR = dyn_cast<Function>(R);

    if (!FunL || !FunR)
        // Both values have to be functions
        return {};

    if (!FunL->getName().startswith(SimpllInlineAsmPrefix) ||
        !FunR->getName().startswith(SimpllInlineAsmPrefix))
        // Both functions have to be assembly abstractions
        return {};

    StringRef AsmL = ModComparator->AsmToStringMapL[FunL->getName()];
    StringRef AsmR = ModComparator->AsmToStringMapR[FunR->getName()];
    if (AsmL == AsmR)
        // The difference is somewhere else
        return {};

    // Create difference object.
    // Note: the call stack is left empty here, it will be added in reportOutput
    SyntaxDifference diff;
    diff.BodyL = AsmL;
    diff.BodyR = AsmR;
    diff.StackL = CallStack();
    diff.StackL.push_back(CallInfo {
        "(generated assembly code)",
        ParentL->getSubprogram()->getFilename(),
        ParentL->getSubprogram()->getLine()
    });
    diff.StackR = CallStack();
    diff.StackR.push_back(CallInfo {
        "(generated assembly code)",
        ParentR->getSubprogram()->getFilename(),
        ParentR->getSubprogram()->getLine()
    });
    diff.function = ParentL->getName();
    diff.name = "assembly code";

    return {diff};
}

/// Handle values generated from macros and enums whose value changed.
/// The new values are pre-computed by DebugInfo.
/// Also handles comparing in case at least one of the values is a cast -
/// when comparing the control flow only, it compares the original value instead
/// of the cast.
int DifferentialFunctionComparator::cmpValues(const Value *L,
                                              const Value *R) const {
    if (controlFlowOnly) {
        // Detect casts and use the original value instead when comparing the
        // control flow only.
        const CastInst *CIL = dyn_cast<CastInst>(L);
        const CastInst *CIR = dyn_cast<CastInst>(R);

        if (CIL && CIR) {
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
            // We also need to remove a BB that was newly inserted in cmpValues
            // since the serial maps would not be synchronized otherwise.
            if (sn_mapL.size() != sn_mapR.size()) {
                if (sn_mapL[L] == (sn_mapL.size() - 1))
                    sn_mapL.erase(L);
                if (sn_mapR[R] == (sn_mapR.size() - 1))
                    sn_mapR.erase(R);
            }
            return 0;
        }
    }
    return result;
}

/// Specific comparing of constants. If one of them (or both) is a cast
/// constant expression, compare its operand.
int DifferentialFunctionComparator::cmpConstants(const Constant *L,
        const Constant *R) const {
    int Result = FunctionComparator::cmpConstants(L, R);

    if (Result == 0)
        return Result;

    if (controlFlowOnly) {
        // Look whether the constants is a cast ConstantExpr
        const ConstantExpr *UEL = dyn_cast<ConstantExpr>(L);
        const ConstantExpr *UER = dyn_cast<ConstantExpr>(R);

        // We want to compare only casts by their operands
        if (UEL && !UEL->isCast())
            UEL = nullptr;
        if (UER && !UER->isCast())
            UER = nullptr;

        if (UEL && UER) {
            return cmpConstants(UEL->getOperand(0), UER->getOperand(0));
        } else if (UEL) {
            return cmpConstants(UEL->getOperand(0), R);
        } else if (UER) {
            return cmpConstants(L, UER->getOperand(0));
        }
    }

    return Result;
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

/// Comparison of memset functions.
/// Handles situation when memset sets the memory occupied by a structure, but
/// the structure size changed.
int DifferentialFunctionComparator::cmpMemset(const CallInst *CL,
                                              const CallInst *CR) const {
    // Compare all except the third operand (size to set).
    for (unsigned i = 0; i < CL->getNumArgOperands(); ++i) {
        if (i == 2)
            continue;
        if (int Res = cmpValues(CL->getArgOperand(i), CR->getArgOperand(i)))
            return Res;
    }

    if (!cmpValues(CL->getArgOperand(2), CR->getArgOperand(2)))
        return 0;

    // Get the struct types of memset destinations.
    StructType *STyL = getStructType(CL->getOperand(0));
    StructType *STyR = getStructType(CR->getOperand(0));

    // Return 0 (equality) if both memory destinations are structs of the same
    // name and each memset size is equal to the corresponding struct size.
    return !STyL || !STyR ||
            cmpStructTypeSizeWithConstant(STyL, CL->getOperand(2)) ||
            cmpStructTypeSizeWithConstant(STyR, CR->getOperand(2)) ||
            STyL->getStructName() != STyR->getStructName();
}
