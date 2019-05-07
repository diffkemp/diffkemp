//===----------- ModuleComparator.cpp - Numbering global symbols ----------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of methods of the ModuleComparator class that
/// can be used for syntactical comparison of two LLVM modules.
///
//===----------------------------------------------------------------------===//

#include "ModuleComparator.h"
#include "DifferentialFunctionComparator.h"
#include "Utils.h"
#include "Config.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

/// Syntactical comparison of functions.
/// Function declarations are equal if they have the same name.
/// Functions with body are compared using custom FunctionComparator that
/// is designed for comparing functions between different modules.
void ModuleComparator::compareFunctions(Function *FirstFun,
                                        Function *SecondFun) {
    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << "Comparing " << FirstFun->getName() << " and "
                           << SecondFun->getName() << "\n");
    ComparedFuns.emplace(std::make_pair(FirstFun, SecondFun), Result::UNKNOWN);

    // Comparing function declarations (function without bodies).
    if (FirstFun->isDeclaration() || SecondFun->isDeclaration()) {
        if (controlFlowOnly) {
            // If checking control flow only, it suffices that one of the
            // functions is a declaration to treat them equal.
            if (FirstFun->getName() == SecondFun->getName())
                ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
            else
                ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
        } else {
            if (FirstFun->isDeclaration() && SecondFun->isDeclaration()
                    && FirstFun->getName() == SecondFun->getName())
                ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
            else if (FirstFun->getName() != SecondFun->getName())
                ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
            else {
                // One function has a body, the second one does not; add the
                // missing definition
                if (FirstFun->isDeclaration())
                    this->MissingDefs.push_back({FirstFun, nullptr});
                else if (SecondFun->isDeclaration())
                    this->MissingDefs.push_back({nullptr, SecondFun});
            }
        }
        return;
    }

    // Comparing functions with bodies using custom FunctionComparator.
    DifferentialFunctionComparator fComp(FirstFun, SecondFun, controlFlowOnly,
                                         &GS, DI, this);
    if (fComp.compare() == 0) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << "Function " << FirstFun->getName()
                               << " is same in both modules\n");
        ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
    } else {
        ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
        while (tryInline.first || tryInline.second) {
            // Try to inline the problematic function calls
            CallInst *inlineFirst = findCallInst(tryInline.first, FirstFun);
            CallInst *inlineSecond = findCallInst(tryInline.second, SecondFun);

            ConstFunPair missingDefs;
            bool inlined = false;
            // If the called function is a declaration, add it to missingDefs.
            // Otherwise, inline the call and simplify the function.
            // The above is done for the first and the second call to inline.
            if (inlineFirst) {
                const Function *toInline =
                        getCalledFunction(inlineFirst->getCalledValue());
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Try to inline "
                                       << toInline->getName()
                                       << " in first.\n");
                if (toInline->isDeclaration()) {
                    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                    dbgs() << "Missing definition\n");
                    if (!toInline->isIntrinsic()
                            && toInline->getName().find("simpll__")
                                    == std::string::npos)
                        missingDefs.first = toInline;
                } else {
                    InlineFunctionInfo ifi;
                    if (InlineFunction(inlineFirst, ifi)) {
                        simplifyFunction(FirstFun);
                        simplifyFunction(SecondFun);
                        inlined = true;
                    }
                }
            }
            if (inlineSecond) {
                const Function *toInline =
                        getCalledFunction(inlineSecond->getCalledValue());
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << "Try to inline "
                                       << toInline->getName()
                                       << " in second.\n");
                if (toInline->isDeclaration()) {
                    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                    dbgs() << "Missing definition\n");
                    if (!toInline->isIntrinsic()
                            && toInline->getName().find("simpll__")
                                    == std::string::npos)
                        missingDefs.second = toInline;
                } else {
                    InlineFunctionInfo ifi;
                    if (InlineFunction(inlineSecond, ifi)) {
                        simplifyFunction(FirstFun);
                        simplifyFunction(SecondFun);
                        inlined = true;
                    }
                }
            }
            // If some function to be inlined does not have a declaration,
            // store it into MissingDefs (will be reported at the end).
            if (missingDefs.first || missingDefs.second) {
                MissingDefs.push_back(missingDefs);
            }
            tryInline = {nullptr, nullptr};
            // If nothing was inlined, do not continue
            if (!inlined)
                break;
            // Reset the function diff result
            ComparedFuns.at({FirstFun, SecondFun}) = Result::UNKNOWN;
            // Re-run the comparison
            DifferentialFunctionComparator fCompSecond(FirstFun, SecondFun,
                                                       controlFlowOnly,
                                                       &GS, DI, this);
            if (fCompSecond.compare() == 0) {
                ComparedFuns.at({FirstFun, SecondFun}) = Result::EQUAL;
            } else {
                ComparedFuns.at({FirstFun, SecondFun}) = Result::NOT_EQUAL;
            }
        }
    }
}
