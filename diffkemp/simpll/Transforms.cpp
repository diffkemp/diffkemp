//===------------ Transforms.cpp - Simplifications of modules -------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementations of functions simplifying two LLVM modules
/// so that they can be more easily compared for semantic difference.
///
//===----------------------------------------------------------------------===//

#include "Transforms.h"
#include "DebugInfo.h"
#include "DifferentialGlobalNumberState.h"
#include "DifferentialFunctionComparator.h"
#include "ModuleComparator.h"
#include "passes/CalledFunctionsAnalysis.h"
#include "passes/FunctionAbstractionsGenerator.h"
#include "passes/PrintContentRemovalPass.h"
#include "passes/RemoveLifetimeCallsPass.h"
#include "passes/RemoveUnusedReturnValuesPass.h"
#include "passes/VarDependencySlicer.h"
#include "Utils.h"
#include "passes/UnifyMemcpyPass.h"
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/LowerExpectIntrinsic.h>

/// Preprocessing functions run on each module at the beginning.
/// The following transformations are applied:
/// 1. Slicing of program w.r.t. to the value of some global variable.
///    Keeps only those instructions whose value or execution depends on
///    the value of the global variable.
///    This is only run if Var is specified.
/// 2. Removal of the arguments of calls to printing functions.
///    These arguments do not affect the code functionallity.
///    TODO: this should be switchable by a CLI option.
/// 3. Unification of memcpy variants so that all use the llvm.memcpy intrinsic.
/// 4. Dead code elimination.
/// 5. Removing calls to llvm.expect.
void preprocessModule(Module &Mod, Function *Main, GlobalVariable *Var) {
    if (Var) {
        // Slicing of the program w.r.t. the value of a global variable
        PassManager<Function, FunctionAnalysisManager, GlobalVariable *> fpm;
        FunctionAnalysisManager fam(false);
        PassBuilder pb;
        pb.registerFunctionAnalyses(fam);

        fpm.addPass(VarDependencySlicer {});
        fpm.run(*Main, fam, Var);
    }

    // Function passes
    FunctionPassManager fpm(false);
    FunctionAnalysisManager fam(false);
    PassBuilder pb;
    pb.registerFunctionAnalyses(fam);

    fpm.addPass(PrintContentRemovalPass{});
    fpm.addPass(UnifyMemcpyPass {});
    fpm.addPass(DCEPass {});
    fpm.addPass(LowerExpectIntrinsicPass {});

    for (auto &Fun : Mod)
        fpm.run(Fun, fam);
}

/// Simplification of modules to ease the semantic diff.
/// Removes all the code that is syntactically same between modules (hence it
/// must not be checked for semantic equivalence).
/// The following transformations are applied:
/// 1. Replacing indirect function calls and inline assemblies by abstraction
///    functions.
/// 2. Transformation of functions returning a value into void functions in case
///    the return value is never used within the module.
/// 3. Using debug information to compute offsets of the corresponding GEP
///    indices. Offsets are stored inside LLVM metadata.
/// 4. Removing bodies of functions that are syntactically equivalent.
std::vector<FunPair> simplifyModulesDiff(Config &config) {
    // Generate abstractions of indirect function calls and for inline
    // assemblies. Then, unify the abstractions between the modules so that
    // the corresponding abstractions get the same name.
    AnalysisManager<Module, Function *> mam(false);
    mam.registerPass([] { return CalledFunctionsAnalysis(); });
    mam.registerPass([] { return FunctionAbstractionsGenerator(); });
    unifyFunctionAbstractions(
            mam.getResult<FunctionAbstractionsGenerator>(*config.First,
                                                         config.FirstFun),
            mam.getResult<FunctionAbstractionsGenerator>(*config.Second,
                                                         config.SecondFun));

    // Module passes
    PassManager<Module, AnalysisManager<Module, Function *>, Function *,
        Module *> mpm;
    mpm.addPass(RemoveUnusedReturnValuesPass {});
    mpm.run(*config.First, mam, config.FirstFun, config.Second.get());
    mpm.run(*config.Second, mam, config.SecondFun, config.First.get());

    DebugInfo DI(*config.First, *config.Second,
                 config.FirstFun, config.SecondFun,
                 mam.getResult<CalledFunctionsAnalysis>(*config.First,
                                                        config.FirstFun));
#ifdef DEBUG
    llvm::errs() << "StructFieldNames size: " <<
        DI.StructFieldNames.size() << '\n';
#endif

    std::vector<FunPair> result;
    // Compare functions for syntactical equivalence
    ModuleComparator modComp(*config.First, *config.Second, &DI);
    if (config.FirstFun && config.SecondFun) {
        modComp.compareFunctions(config.FirstFun, config.SecondFun);

#ifdef DEBUG
        errs() << "Syntactic comparison results:\n";
#endif
        bool allEqual = true;
        for (auto &funPair : modComp.ComparedFuns) {
            if (funPair.second == ModuleComparator::NOT_EQUAL) {
                allEqual = false;
                result.emplace_back(funPair.first.first, funPair.first.second);
#ifdef DEBUG
                errs() << funPair.first.first->getName()
                       << " are syntactically different\n";
#endif
            }
        }
        if (allEqual) {
            // Functions are equal iff all functions that were compared by
            // module comparator(i.e. those that are recursively called by the
            // main functions) are equal.
#ifdef DEBUG
            errs() << "All functions are syntactically equal\n";
#endif
            config.FirstFun->deleteBody();
            config.SecondFun->deleteBody();
            deleteAliasToFun(*config.First, config.FirstFun);
            deleteAliasToFun(*config.Second, config.SecondFun);
        }
    } else {
        for (auto &FunFirst : *config.First) {
            if (auto FunSecond =
                    config.Second->getFunction(FunFirst.getName())) {
                modComp.compareFunctions(&FunFirst, FunSecond);
            }
        }
    }
    return result;
}

/// Recursively mark callees of a function with 'alwaysinline' attribute.
void markCalleesAlwaysInline(Function &Fun,
                             const std::set<Function *> IgnoreFuns) {
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                auto CalledFun = CallInstr->getCalledFunction();
                if (!CalledFun || CalledFun->isDeclaration() ||
                        CalledFun->isIntrinsic() ||
                        IgnoreFuns.find(CalledFun) != IgnoreFuns.end())
                    continue;

                if (!CalledFun->hasFnAttribute(
                        Attribute::AttrKind::AlwaysInline)) {
                    CalledFun->addFnAttr(Attribute::AttrKind::AlwaysInline);
                    errs() << "Inlining: " << CalledFun->getName() << "\n";
                    markCalleesAlwaysInline(*CalledFun, IgnoreFuns);
                }
            }
        }
    }
}

/// Preprocessing functions run on each module at the beginning.
/// The following transformations are applied:
/// 1. Removing debugging information.
///    TODO this should be configurable via CLI option.
/// 2. Inlining all functions called by the analysed function (if possible).
void postprocessModule(Module &Mod, const std::set<Function *> &MainFuns) {
    if (MainFuns.empty())
        return;

    errs() << "Postprocess\n";

    for (auto *Main : MainFuns) {
        errs() << "  " << Main->getName() << "\n";
        // Do not inline function that will be compared
        if (Main->hasFnAttribute(Attribute::AttrKind::AlwaysInline))
            Main->removeFnAttr(Attribute::AttrKind::AlwaysInline);
        // Inline all other functions
        markCalleesAlwaysInline(*Main, MainFuns);
    }

    // Function passes
    PassBuilder pb;
    FunctionPassManager fpm(false);
    FunctionAnalysisManager fam(false);
    pb.registerFunctionAnalyses(fam);
    fpm.addPass(RemoveDebugInfoPass {});
    for (auto &F : Mod)
        fpm.run(F, fam);

    // Module passes
    ModulePassManager mpm(false);
    ModuleAnalysisManager mam(false);
    pb.registerModuleAnalyses(mam);
    mpm.addPass(AlwaysInlinerPass {});
    mpm.addPass(RemoveLifetimeCallsPass {});
    mpm.run(Mod, mam);
}
