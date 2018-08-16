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
#include "passes/FunctionAbstractionsGenerator.h"
#include "passes/PrintContentRemovalPass.h"
#include "passes/RemoveLifetimeCallsPass.h"
#include "passes/RemoveUnusedReturnValuesPass.h"
#include "passes/VarDependencySlicer.h"
#include "Utils.h"
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>
#include <llvm/Transforms/Scalar/DCE.h>

/// Preprocessing functions run on each module at the beginning.
/// The following transformations are applied:
/// 1. Slicing of program w.r.t. to the value of some global variable.
///    Keeps only those instructions whose value or execution depends on
///    the value of the global variable.
///    This is only run if Var is specified.
/// 2. Removal of the arguments of calls to printing functions.
///    These arguments do not affect the code functionallity.
///    TODO: this should be switchable by a CLI option.
/// 3. Dead code elimination.
/// 4. Transformation of functions returning a value into void functions in case
///    the return value is never used within the module.
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
    fpm.addPass(DCEPass {});

    for (auto &Fun : Mod)
        fpm.run(Fun, fam);

    // Module passes
    PassManager<Module, ModuleAnalysisManager, Function *> mpmFunctionArg;
    ModuleAnalysisManager mam(false);
    pb.registerModuleAnalyses(mam);

    // Register passes here
    mpmFunctionArg.addPass(RemoveUnusedReturnValuesPass {});

    mpmFunctionArg.run(Mod, mam, Main);
}

/// Delete alias to a function.
void deleteAliasToFun(Module &Mod, const Function *Fun) {
    std::vector<GlobalAlias *> toRemove;
    for (auto &alias : Mod.aliases()) {
        if (alias.getAliasee() == Fun)
            toRemove.push_back(&alias);
    }
    for (auto &alias : toRemove)
        alias->eraseFromParent();
}

/// Simplification of modules to ease the semantic diff.
/// Removes all the code that is syntactically same between modules (hence it
/// must not be checked for semantic equivalence).
/// The following transformations are applied:
/// 1. Using debug information to compute offsets of the corresponding GEP
///    indices. Offsets are stored inside LLVM metadata.
/// 2. Replacing indirect function calls and inline assemblies by abstraction
///    functions.
/// 3. Removing bodies of functions that are syntactically equivalent.
void simplifyModulesDiff(Config &config) {
    DebugInfo(*config.First, *config.Second, config.FirstFun, config.SecondFun);

    // Generate abstractions of indirect function calls and for inline
    // assemblies. Then, unify the abstractions between the modules so that
    // the corresponding abstractions get the same name.
    AnalysisManager<Module, Function *> mam(false);
    mam.registerPass([] { return FunctionAbstractionsGenerator(); });
    unifyFunctionAbstractions(
            mam.getResult<FunctionAbstractionsGenerator>(*config.First,
                                                         config.FirstFun),
            mam.getResult<FunctionAbstractionsGenerator>(*config.Second,
                                                         config.SecondFun));

    // Compare functions for syntactic equivalence.
    for (auto &FunFirst : *config.First) {
        if (FunFirst.isDeclaration() ||
                !(config.FirstFun && (&FunFirst == config.FirstFun ||
                        callsTransitively(*config.FirstFun,
                                          FunFirst))))
            continue;
        auto FunSecond = config.Second->getFunction(FunFirst.getName());

        if (!FunSecond)
            continue;

        if (FunSecond->isDeclaration() ||
                !(config.SecondFun && (FunSecond == config.SecondFun ||
                        callsTransitively(*config.SecondFun,
                                          *FunSecond))))
            continue;

        DifferentialGlobalNumberState gs(config.First.get(),
                                         config.Second.get());
        DifferentialFunctionComparator fComp(&FunFirst, FunSecond, &gs);
        if (fComp.compare() == 0) {
#ifdef DEBUG
            errs() << "Function " << FunFirst.getName()
                   << " is same in both modules\n";
#endif
            FunFirst.deleteBody();
            deleteAliasToFun(*config.First, &FunFirst);
            FunSecond->deleteBody();
            deleteAliasToFun(*config.Second, FunSecond);
        }
    }
}

/// Recursively mark callees of a function with 'alwaysinline' attribute.
void markCalleesAlwaysInline(Function &Fun) {
    for (auto &BB : Fun) {
        for (auto &Instr : BB) {
            if (auto CallInstr = dyn_cast<CallInst>(&Instr)) {
                auto CalledFun = CallInstr->getCalledFunction();
                CallInstr->dump();
                if (!CalledFun || CalledFun->isDeclaration())
                    continue;

                if (!CalledFun->hasFnAttribute(
                        Attribute::AttrKind::AlwaysInline)) {
                    CalledFun->addFnAttr(Attribute::AttrKind::AlwaysInline);
                    markCalleesAlwaysInline(*CalledFun);
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
void postprocessModule(Module &Mod, Function *Main) {
    if (!Main)
        return;

    errs() << "Postprocess\n";

    markCalleesAlwaysInline(*Main);

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

    errs() << "Function " << Main->getName() << " after inlining:\n";
    Main->dump();
}