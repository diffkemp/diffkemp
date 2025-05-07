//===----- CPatternPass.cpp - preprocessing pass for custom C patterns ----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Kucma, tomaskucma2@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains implementation of preprocessing pass for custom C
/// patterns.
///
//===----------------------------------------------------------------------===//

#include "CPatternPass.h"
#include "CustomPatternSet.h"
#include "ModuleAnalysis.h"
#include "Utils.h"
#include "../patterns/diffkemp_patterns.h"
#include "passes/ReduceFunctionMetadataPass.h"
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/Metadata.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <utility>

/// Run the preprocessing pass.
void CPatternPass::run(Module *Mod) {
    initialize(Mod);

    for (auto &Fun : *Mod) {
        renameFunction(Fun);
    }

    tagPatterns(Mod);

    if (auto CPatternIndicator = Mod->getNamedValue(CPATTERN_INDICATOR)) {
        CPatternIndicator->eraseFromParent();
    }
}

/// Find functions starting with the old pattern prefix, find matching new
/// versions, and store them in the patterns map.
void CPatternPass::initialize(Module *Mod) {
    patterns.clear();
    for (Function &FunL : *Mod) {
        if (FunL.isDeclaration()
            || !FunL.getName().startswith(CPATTERN_OLD_PREFIX)) {
            continue;
        }
        std::string PatternName{
                FunL.getName().substr(strlen(CPATTERN_OLD_PREFIX))};
        std::string NameR{CPATTERN_NEW_PREFIX + PatternName};
        Function *FunPtrR = Mod->getFunction(NameR);
        if (FunPtrR && !FunPtrR->isDeclaration()) {
            std::pair<Function *, Function *> PatternPair{&FunL, FunPtrR};
            patterns.emplace(PatternName, PatternPair);
        }
    }
}

/// Tag pattern start, using SimpLL library for comparison, and tag pattern end.
void CPatternPass::tagPatterns(Module *Mod) {
    constexpr BuiltinPatterns BuiltinPatternsConfig{
            .StructAlignment = false,
            .FunctionSplits = false,
            .UnusedReturnTypes = false,
            .KernelPrints = false,
            .DeadCode = false,
            .NumericalMacros = false,
            .Relocations = false,
            .TypeCasts = false,
            .ControlFlowOnly = false,
            .InverseConditions = false,
    };

    // Clone the module, as comparing module with itself is not possible.
    std::unique_ptr<Module> ModClone{CloneModule(*Mod)};

    // It is necessary to set the debug flag to false as a workaround,
    // since configuration of verbosity doesn't work properly when used
    // recursively.
    bool DebugFlagBackup = DebugFlag;
    DebugFlag = false;

    for (auto &[name, PatternPair] : patterns) {
        auto &[PatternL, PatternR] = PatternPair;

        // Compare the patterns.
        Config config{
                std::string{PatternL->getName().str()}, // FirstFunName,
                std::string{PatternR->getName().str()}, // SecondFunName,
                Mod,                                    // FirstModule,
                ModClone.get(),                         // SecondModule,
                std::string{},                          // FirstOutFile,
                std::string{},                          // SecondOutFile,
                std::string{},                          // CacheDir,
                std::string{},                          // CustomPattern,
                BuiltinPatternsConfig,                  // BuiltinPatterns,
                std::string{},                          // Variable,
                false,                                  // OutputLlvmIR,
                false,                                  // PrintAsmDiffs,
                false,                                  // PrintCallStacks,
                false,                                  // ExtendedStat,
                0                                       // Verbosity,
        };
        OverallResult Result;
        processAndCompare(config, Result);

        // Tag the pattern start.
        for (const auto &FunRes : Result.functionResults) {
            if (FunRes.First.name == PatternL->getName().str()
                && FunRes.Second.name == PatternR->getName().str()) {
                auto [InstL, InstR] = FunRes.DifferingInstructions;
                if (InstL && InstR) {
                    appendMetadata(const_cast<Instruction *>(InstL),
                                   CustomPatternSet::MetadataName,
                                   "pattern-start");
                    appendMetadata(const_cast<Instruction *>(InstR),
                                   CustomPatternSet::MetadataName,
                                   "pattern-start");
                }
                break;
            }
        }

        // Copy the compared and tagged right-side pattern function to the
        // original module. This is necessary, because the pattern comparison
        // modifies the right-side function.
        replaceFunctionWithClone(
                Mod, ModClone.get(), PatternR->getName().str());

        tagPatternEnd(*PatternL);
        tagPatternEnd(*PatternR);
    }

    DebugFlag = DebugFlagBackup;
}

/// Rename function to a proper LLVM pattern name.
void CPatternPass::renameFunction(Function &Fun) {
    if (Fun.getName().startswith(CPATTERN_OLD_PREFIX)) {
        Fun.setName(CustomPatternSet::FullPrefixL
                    + Fun.getName().substr(strlen(CPATTERN_OLD_PREFIX)));
    } else if (Fun.getName().startswith(CPATTERN_NEW_PREFIX)) {
        Fun.setName(CustomPatternSet::FullPrefixR
                    + Fun.getName().substr(strlen(CPATTERN_NEW_PREFIX)));
    } else if (Fun.getName().equals(CPATTERN_OUTPUT_MAPPING_NAME)) {
        Fun.setName(CustomPatternSet::OutputMappingFunName);
    }
}

/// Check whether the given instruction is part of the pattern body.
bool CPatternPass::isPatternBody(Instruction *Inst) {
    if (isa<ReturnInst>(Inst)) {
        return false;
    }
    if (auto Call = dyn_cast<CallInst>(Inst)) {
        Function *Fun = getCalledFunction(Call);
        if (Fun && Fun->hasName()
            && Fun->getName().equals(CustomPatternSet::OutputMappingFunName)) {
            return false;
        }
    }
    return true;
}

/// Tag the pattern end in a given function.
void CPatternPass::tagPatternEnd(Function &Fun) {
    auto Inst = inst_begin(&Fun);

    // Handle the case when the pattern body is empty.
    if (Inst != inst_end(&Fun) && !isPatternBody(&*Inst)) {
        appendMetadata(&*Inst, CustomPatternSet::MetadataName, "pattern-end");
        return;
    }

    for (; Inst != inst_end(&Fun); Inst++) {
        Instruction *NextInst{Inst->getNextNonDebugInstruction()};
        if (!NextInst) {
            if (Inst->getNumSuccessors() == 1) {
                NextInst =
                        Inst->getSuccessor(0)->getFirstNonPHIOrDbgOrLifetime();
            }
        }
        if (NextInst && isPatternBody(&*Inst) && !isPatternBody(NextInst)) {
            appendMetadata(
                    NextInst, CustomPatternSet::MetadataName, "pattern-end");
        }
    }
}
