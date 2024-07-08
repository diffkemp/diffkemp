//===------------ ModuleComparator.cpp - Comparing LLVM modules -----------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of methods of the ModuleComparator class that
/// can be used for semantic comparison of two LLVM modules.
///
//===----------------------------------------------------------------------===//

#include "ModuleComparator.h"
#include "Config.h"
#include "DifferentialFunctionComparator.h"
#include "Logger.h"
#include "Utils.h"
#include "passes/FunctionAbstractionsGenerator.h"
#include "passes/SimplifyKernelFunctionCallsPass.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

/// Updates the statistics based on the results of function comparison.
void ModuleComparator::updateStats(
        Result &result, const DifferentialFunctionComparator &fComp) {
    result.First.stats.instCnt = fComp.ComparedInstL;
    result.First.stats.instEqualCnt = fComp.InstEqual;
    result.First.stats.linesCnt = fComp.ComparedLinesL.size();

    result.Second.stats.instCnt = fComp.ComparedInstR;
    result.Second.stats.instEqualCnt = fComp.InstEqual;
    result.Second.stats.linesCnt = fComp.ComparedLinesR.size();
}

/// Semantic comparison of functions.
/// Function declarations are equal if they have the same name.
/// Functions with body are compared using custom FunctionComparator that
/// is designed for comparing functions between different modules.
void ModuleComparator::compareFunctions(Function *FirstFun,
                                        Function *SecondFun) {
    LOG("Comparing \"" << FirstFun->getName() << "\" and \""
                       << SecondFun->getName() << "\" { ");
    LOG_INDENT();
    ComparedFuns.emplace(std::make_pair(FirstFun, SecondFun),
                         Result(FirstFun, SecondFun));

    // Check if the functions is in the ignored list.
    if (ResCache.isFunctionPairCached(FirstFun, SecondFun)) {
        LOG("ignored }\n");
        ComparedFuns.at({FirstFun, SecondFun}).kind = Result::UNKNOWN;
        return;
    }

    // Comparing function declarations (function without bodies).
    if (FirstFun->isDeclaration() || SecondFun->isDeclaration()) {
        // Drop suffixes of function names. This is necessary in order to
        // successfully compare an original void-returning function with one
        // generated by RemoveUnusedReturnValuesPass, which will have a number
        // suffix.
        auto FirstFunName = FirstFun->getName().str();
        if (hasSuffix(FirstFunName))
            FirstFunName = dropSuffix(FirstFunName);
        auto SecondFunName = SecondFun->getName().str();
        if (hasSuffix(SecondFunName))
            SecondFunName = dropSuffix(SecondFunName);

        if (config.Patterns.ControlFlowOnly) {
            // If checking control flow only, it suffices that one of the
            // functions is a declaration to treat them equal.
            if (FirstFunName == SecondFunName)
                ComparedFuns.at({FirstFun, SecondFun}).kind =
                        Result::ASSUMED_EQUAL;
            else
                ComparedFuns.at({FirstFun, SecondFun}).kind = Result::NOT_EQUAL;
        } else {
            if (FirstFun->isDeclaration() && SecondFun->isDeclaration()
                && FirstFunName == SecondFunName)
                ComparedFuns.at({FirstFun, SecondFun}).kind =
                        Result::ASSUMED_EQUAL;
            else if (FirstFunName != SecondFunName)
                ComparedFuns.at({FirstFun, SecondFun}).kind = Result::NOT_EQUAL;
            else {
                // One function has a body, the second one does not; add the
                // missing definition
                if (FirstFunName == SecondFunName)
                    ComparedFuns.at({FirstFun, SecondFun}).kind =
                            Result::ASSUMED_EQUAL;
                if (FirstFun->isDeclaration())
                    this->MissingDefs.push_back({FirstFun, nullptr});
                else if (SecondFun->isDeclaration())
                    this->MissingDefs.push_back({nullptr, SecondFun});
            }
        }

        LOG_UNINDENT();
        switch (ComparedFuns.at({FirstFun, SecondFun}).kind) {
        case Result::NOT_EQUAL:
            LOG_NO_INDENT("declaration, names are "
                          << Color::makeRed("not equal") << " }\n");
            break;
        case Result::ASSUMED_EQUAL:
            LOG_NO_INDENT("declaration, " << Color::makeGreen("assumed equal")
                                          << " }\n");
            break;
        default:
            break;
        };

        return;
    }
    LOG_NO_INDENT("\n");

    // Comparing functions with bodies using custom FunctionComparator.
    DifferentialFunctionComparator fComp(
            FirstFun, SecondFun, config, DI, &CustomPatterns, this);
    int result = fComp.compare();
    updateStats(ComparedFuns.at({FirstFun, SecondFun}), fComp);

    LOG_UNINDENT();
    if (result == 0) {
        LOG("} " << Color::makeGreen("equal\n"));
        ComparedFuns.at({FirstFun, SecondFun}).kind = Result::EQUAL;
    } else {
        LOG("} " << Color::makeRed("not equal\n")
                 << Color::makeRed("========== ")
                 << "Found difference between \"" << FirstFun->getName()
                 << "\" and \"" << SecondFun->getName() << "\""
                 << Color::makeRed(" ==========\n"));
        ComparedFuns.at({FirstFun, SecondFun}).kind = Result::NOT_EQUAL;

        std::set<ConstFunPair> inlinedPairs;

        while (tryInline.first || tryInline.second) {
            // Try to inline the problematic function calls
            CallInst *callFirst = findCallInst(tryInline.first, FirstFun);
            CallInst *callSecond = findCallInst(tryInline.second, SecondFun);
            auto calledFirst = getCalledFunction(callFirst);
            auto calledSecond = getCalledFunction(callSecond);

            auto inlineResultFirst = tryToInline(
                    callFirst, Program::First, config.Patterns.FunctionSplits);
            auto inlineResultSecond =
                    tryToInline(callSecond,
                                Program::Second,
                                config.Patterns.FunctionSplits);

            // If some function to be inlined does not have a declaration,
            // store it into MissingDefs (will be reported at the end).
            if (inlineResultFirst == MissingDef
                || inlineResultSecond == MissingDef)
                MissingDefs.emplace_back(calledFirst, calledSecond);

            // If nothing was inlined, do not continue
            if (inlineResultFirst != InliningResult::Inlined
                && inlineResultSecond != InliningResult::Inlined) {
                break;
            }
            inlinedPairs.emplace(calledFirst, calledSecond);

            // Always simplify both functions even if inlining was done in one
            // of them only - this is to keep them synchronized.
            simplifyFunction(FirstFun);
            simplifyFunction(SecondFun);

            LOG_VERBOSE_EXTRA("Functions after inlining:\n"
                              << "L:\n"
                              << *FirstFun << "R:\n"
                              << *SecondFun);

            // Reset the function diff result
            ComparedFuns.at({FirstFun, SecondFun}).kind = Result::UNKNOWN;

            LOG("Comparing \"" << FirstFun->getName() << "\" and \""
                               << SecondFun->getName()
                               << "\" (after inlining) {\n");
            LOG_INDENT();

            // Re-run the comparison
            DifferentialFunctionComparator fCompSecond(
                    FirstFun, SecondFun, config, DI, &CustomPatterns, this);
            result = fCompSecond.compare();
            updateStats(ComparedFuns.at({FirstFun, SecondFun}), fCompSecond);

            // If the functions are equal after the inlining and there is a
            // call to the inlined function, mark it as weak.
            if (result == 0) {
                if (calledFirst)
                    for (const CallInfo &CI :
                         ComparedFuns.at({FirstFun, SecondFun}).First.calls) {
                        if (CI.name == calledFirst->getName().str())
                            CI.weak = true;
                    }
                if (calledSecond)
                    for (const CallInfo &CI :
                         ComparedFuns.at({FirstFun, SecondFun}).Second.calls) {
                        if (CI.name == calledSecond->getName().str())
                            CI.weak = true;
                    }
            }

            LOG_UNINDENT();
            if (result == 0) {
                // If the functions are equal after inlining, results for all
                // inlined functions must be reset as they may pollute
                // the overall output otherwise
                for (auto &inlinedPair : inlinedPairs)
                    ComparedFuns.erase(inlinedPair);

                LOG("} " << Color::makeGreen("equal\n"));
                ComparedFuns.at({FirstFun, SecondFun}).kind = Result::EQUAL;
            } else {
                LOG("} still " << Color::makeRed("not equal\n"));
                ComparedFuns.at({FirstFun, SecondFun}).kind = Result::NOT_EQUAL;
            }
        }
    }
}

/// Try to inline a function call.
/// \param Call                  Call instruction to inline
/// \param program               Program in which the inlining is done
/// \param FunctionSplitsEnabled Whether the necessary built-in pattern is on
/// \return InliningResult::Inlined    when inlining was successful
///         InliningResult::NotInlined when inlining was unsuccessful
///         Inlining::MissingDef       when inlining was unsuccessful due to
///                                    missing function definition
ModuleComparator::InliningResult ModuleComparator::tryToInline(
        CallInst *Call, Program program, bool FunctionSplitsEnabled) const {
    if (!Call)
        return NotInlined;

    Function *toInline = getCalledFunction(Call);

    if (!FunctionSplitsEnabled && !isSimpllAbstraction(toInline))
        return NotInlined;

    LOG("Inlining \"" << toInline->getName() << "\" in " << programName(program)
                      << "\n");
    if (toInline->isDeclaration()) {
        LOG("Missing definition\n");
        if (!toInline->isIntrinsic() && !isSimpllAbstraction(toInline))
            return MissingDef;
    } else if (!isKernelSimplifiedFunction(toInline->getName().str())) {
        InlineFunctionInfo ifi;
        if (inlineCall(Call)) {
            return Inlined;
        }
    }
    return NotInlined;
}
