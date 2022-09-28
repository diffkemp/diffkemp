//===----------- PatternSet.cpp - Unordered set of code patterns ----------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the unordered LLVM code pattern
/// set. Pattern sets are generated from the given pattern configuration file
/// and hold all valid patterns that have been referenced there. Patterns may
/// be instruction-based, or value-based. Instruction-based patterns are
/// represented by multiple LLVM IR instructions, while value-based patterns
/// contain only a single return instruction, which describes a difference in
/// a single value.
///
//===----------------------------------------------------------------------===//

#include "PatternSet.h"
#include "Config.h"
#include "ModuleAnalysis.h"
#include <algorithm>
#include <llvm/Support/Debug.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLTraits.h>

using namespace llvm::yaml;
using namespace llvm::sys::path;

// YAML to PatternConfiguration mapping
namespace llvm {
namespace yaml {
template <> struct MappingTraits<PatternConfiguration> {
    static void mapping(IO &IO, PatternConfiguration &Config) {
        IO.mapOptional("on_parse_failure", Config.OnParseFailure);
        IO.mapOptional("patterns", Config.PatternFiles);
    }
};
} // namespace yaml
} // namespace llvm

/// Metadata operand counts for different kinds of pattern metadata.
const StringMap<int> PatternMetadata::MetadataOperandCounts = {
        {"pattern-start", 0},
        {"pattern-end", 0},
        {"group-start", 0},
        {"group-end", 0},
        {"arbitrary-constant", 1},
        {"function-name-regex", 2},
        {"enable-name-comparison", 0},
        {"disable-name-comparison", 0},
        {"enable-align-comparison", 0},
        {"disable-align-comparison", 0},
        {"not-an-input", 0},
        {"no-value-pattern-detection", 0}};

/// Default DiffKemp prefix for all pattern information.
const std::string PatternSet::DefaultPrefix = "diffkemp.";
/// Prefix for the left (old) side of difference patterns.
const std::string PatternSet::PrefixL = "old.";
/// Prefix for the right (new) side of difference patterns.
const std::string PatternSet::PrefixR = "new.";
/// Complete prefix for the old side of difference patterns.
const std::string PatternSet::FullPrefixL =
        PatternSet::DefaultPrefix + PatternSet::PrefixL;
/// Complete prefix for the right side of difference patterns.
const std::string PatternSet::FullPrefixR =
        PatternSet::DefaultPrefix + PatternSet::PrefixR;
/// Name for the function defining output instuction mapping.
const std::string PatternSet::OutputMappingFunName =
        PatternSet::DefaultPrefix + "output_mapping";
/// Name for constants that represent an arbitrary value.
const std::string PatternSet::ArbitraryValueConstName =
        PatternSet::DefaultPrefix + "any";
/// Structure name representing an arbitrary type.
const std::string PatternSet::ArbitraryTypeStructName =
        "struct." + PatternSet::DefaultPrefix + "any";
/// Name for pattern metadata nodes.
const std::string PatternSet::MetadataName =
        PatternSet::DefaultPrefix + "pattern";

/// Create a new pattern set based on the given configuration.
PatternSet::PatternSet(std::string ConfigPath) {
    if (ConfigPath.empty()) {
        return;
    }

    // If a pattern is used as a configuration file, only load the pattern.
    if (extension(ConfigPath) == ".ll") {
        addPattern(ConfigPath);
    } else {
        loadConfig(ConfigPath);
    }
}

/// Retrives pattern metadata attached to the given instruction.
Optional<PatternMetadata>
        PatternSet::getPatternMetadata(const Instruction &Inst) const {
    auto InstMetadata = Inst.getMetadata(MetadataName);
    if (!InstMetadata) {
        return None;
    }

    unsigned int OperandIndex = 0;
    PatternMetadata Metadata;
    while (OperandIndex < InstMetadata->getNumOperands()) {
        /// Parse the current pattern metadata operand as well as the operands
        /// that depend on it.
        if (auto Type = dyn_cast<MDString>(
                    InstMetadata->getOperand(OperandIndex).get())) {
            auto TypeName = Type->getString();
            // Validate operand count.
            int TotalOperandCount =
                    PatternMetadata::MetadataOperandCounts.lookup(TypeName) + 1;
            if ((TotalOperandCount + OperandIndex)
                > InstMetadata->getNumOperands()) {
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Missing operands for metadata type "
                                       << TypeName << " in node "
                                       << *InstMetadata << ".\n");
                return None;
            }
            // Parse the metadata operand.
            bool InvalidOperands = false;
            if (TypeName == "pattern-start") {
                // pattern-start metadata: string type.
                Metadata.PatternStart = true;
            } else if (TypeName == "pattern-end") {
                // pattern-end metadata: string type.
                Metadata.PatternEnd = true;
            } else if (TypeName == "group-start") {
                // group-start metadata: string type.
                Metadata.GroupStart = true;
            } else if (TypeName == "group-end") {
                // group-end metadata: string type.
            } else if (TypeName == "arbitrary-constant") {
                // arbitrary-constant metadata: string type,
                //                              string arbitrary-constant.
                auto ArbitraryConstOp = dyn_cast<MDString>(
                        InstMetadata->getOperand(OperandIndex + 1).get());
                if (ArbitraryConstOp) {
                    auto ParentModule =
                            Inst.getParent()->getParent()->getParent();
                    auto ArbitraryGlobalVar = ParentModule->getGlobalVariable(
                            ArbitraryConstOp->getString());
                    if (!ArbitraryGlobalVar
                        || !isa<Constant>(ArbitraryGlobalVar)) {
                        InvalidOperands = true;
                    } else {
                        auto ArbitraryConst =
                                cast<Constant>(ArbitraryGlobalVar);
                        Metadata.ArbitraryGEPConst = ArbitraryConst;
                    }
                } else {
                    InvalidOperands = true;
                }
            } else if (TypeName == "function-name-regex") {
                // function-name-regex metadata: string type,
                //                               string regex-to-match,
                //                               string constant-to-tie-to.
                auto RegexOp = dyn_cast<MDString>(
                        InstMetadata->getOperand(OperandIndex + 1).get());
                auto TyingConstOp = dyn_cast<MDString>(
                        InstMetadata->getOperand(OperandIndex + 2).get());
                if (RegexOp && TyingConstOp) {
                    auto ParentModule =
                            Inst.getParent()->getParent()->getParent();
                    auto TyingGlobalVar = ParentModule->getGlobalVariable(
                            TyingConstOp->getString());
                    if (!TyingGlobalVar || !isa<Constant>(TyingGlobalVar)) {
                        InvalidOperands = true;
                    } else {
                        auto TyingConst = cast<Constant>(TyingGlobalVar);
                        Metadata.FunctionNameRegexes.emplace_back(
                                RegexOp->getString(), TyingConst);
                    }
                } else {
                    InvalidOperands = true;
                }
            } else if (TypeName == "enable-name-comparison") {
                // enable-name-comparison metadata: string type.
                Metadata.EnableNameComparison = true;
            } else if (TypeName == "disable-name-comparison") {
                // disable-name-comparison metadata: string type.
                Metadata.DisableNameComparison = true;
            } else if (TypeName == "enable-align-comparison") {
                // enable-align-comparison metadata: string type.
                Metadata.EnableAlignComparison = true;
            } else if (TypeName == "disable-align-comparison") {
                // disable-align-comparison metadata: string type.
                Metadata.DisableAlignComparison = true;
            } else if (TypeName == "not-an-input") {
                // not-an-input metadata: string type.
                Metadata.NotAnInput = true;
            } else if (TypeName == "no-value-pattern-detection") {
                // no-value-pattern-detection metadata: string type.
                Metadata.NoValuePatternDetection = true;
            } else {
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Invalid metadata type " << TypeName
                                       << " in node " << *InstMetadata
                                       << ".\n");
                return None;
            }
            if (InvalidOperands) {
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Invalid operands for metadata type "
                                       << TypeName << " in node "
                                       << *InstMetadata << ".\n");
                return None;
            }
            // Shift the operand offset accordingly.
            OperandIndex += TotalOperandCount;
        } else {
            return None;
        }
    }
    return {Metadata};
}

/// Load the given LLVM IR based difference pattern YAML configuration.
void PatternSet::loadConfig(std::string &ConfigPath) {
    auto ConfigFile = MemoryBuffer::getFile(ConfigPath);
    if (std::error_code EC = ConfigFile.getError()) {
        DEBUG_WITH_TYPE(
                DEBUG_SIMPLL,
                dbgs() << getDebugIndent() << "Failed to open difference "
                       << "pattern configuration " << ConfigPath << ".\n");
        return;
    }

    // Parse the configuration file.
    Input YamlFile(ConfigFile.get()->getBuffer());
    PatternConfiguration Config;
    YamlFile >> Config;

    if (YamlFile.error()) {
        DEBUG_WITH_TYPE(
                DEBUG_SIMPLL,
                dbgs() << getDebugIndent() << "Failed to parse difference "
                       << "pattern configuration " << ConfigPath << ".\n");
        return;
    }

    // Load all pattern files included in the configuration.
    for (auto &PatternFile : Config.PatternFiles) {
        addPattern(PatternFile);
    }
}

/// Add a new LLVM IR difference pattern file.
void PatternSet::addPattern(std::string &Path) {
    // Try to load the pattern module.
    SMDiagnostic err;
    auto PatternModule = parseIRFile(Path, err, PatternContext);
    if (!PatternModule) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "Failed to parse difference pattern "
                               << "module " << Path << ".\n");
        return;
    }

    for (auto &Function : PatternModule->getFunctionList()) {
        // Select only defined functions that start with the left prefix.
        if (Function.isDeclaration()
            || !Function.getName().startswith(FullPrefixL))
            continue;

        std::string Name = Function.getName().substr(FullPrefixL.size()).str();
        std::string NameR = FullPrefixR + Name;

        // Find the corresponding pattern function with the right prefix.
        auto FunctionR = PatternModule->getFunction(NameR);
        if (!FunctionR)
            continue;
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "Loading a new difference pattern " << Name
                               << " from module " << Path << ".\n");

        switch (getPatternType(&Function, FunctionR)) {
        case PatternType::INST: {
            InstPattern NewInstPattern(Name, &Function, FunctionR);
            if (initializeInstPattern(NewInstPattern)) {
                InstPatterns.emplace(NewInstPattern);
            }
            break;
        }

        case PatternType::VALUE: {
            ValuePattern NewValuePattern(Name, &Function, FunctionR);
            if (initializeValuePattern(NewValuePattern)) {
                ValuePatterns.emplace(NewValuePattern);
            }
            break;
        }

        default:
            DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                            dbgs() << getDebugIndent()
                                   << "An unknown pattern type has "
                                      "been used.\n");
            break;
        }
    }

    // Keep the module pointer.
    PatternModules.push_back(std::move(PatternModule));
}

/// Finds the pattern type associated with the given pattern functions.
PatternType PatternSet::getPatternType(const Function *FnL,
                                       const Function *FnR) {
    // Value patterns should only contain a single return instruction.
    auto EntryBBL = &FnL->getEntryBlock(), EntryBBR = &FnR->getEntryBlock();
    if (EntryBBL->size() == 1 && EntryBBR->size() == 1) {
        // The value pattern detection might be disabled for this pattern.
        auto PatMetadataL = getPatternMetadata(*EntryBBL->begin());
        auto PatMetadataR = getPatternMetadata(*EntryBBR->begin());
        if ((!PatMetadataL || !PatMetadataL->NoValuePatternDetection)
            && (!PatMetadataR || !PatMetadataR->NoValuePatternDetection)) {
            return PatternType::VALUE;
        }
    }

    return PatternType::INST;
}

/// Initializes an instruction pattern, loading all metadata, start positions,
/// and the output instruction mapping. Unless the start position is chosen by
/// metadata, it is set to the first differing pair of pattern instructions.
/// Patterns with conflicting differences in concurrent branches are skipped,
/// returning false.
bool PatternSet::initializeInstPattern(InstPattern &Pat) {
    OutputMappingInfo OutputMappingInfoL, OutputMappingInfoR;

    // Initialize both pattern sides.
    initializeInstPatternSide(Pat, OutputMappingInfoL, true);
    initializeInstPatternSide(Pat, OutputMappingInfoR, false);

    // Map input arguments from the left side to the right side.
    if (Pat.PatternL->arg_size() != Pat.PatternR->arg_size()) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "The number of input arguments does not "
                               << "match in pattern " << Pat.Name << ".\n");
        return false;
    }
    for (auto L = Pat.PatternL->arg_begin(), R = Pat.PatternR->arg_begin();
         L != Pat.PatternL->arg_end();
         ++L, ++R) {
        Pat.ArgumentMapping[&*L] = &*R;
    }

    // Create references for the expected output instruction mapping.
    if (OutputMappingInfoL.second != OutputMappingInfoR.second) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "The number of output instructions does "
                               << "not match in pattern " << Pat.Name << ".\n");
        return false;
    }
    if (OutputMappingInfoL.first && OutputMappingInfoR.first) {
        for (int i = 0; i < OutputMappingInfoL.second; ++i) {
            auto MappedInstL = dyn_cast<Instruction>(
                    OutputMappingInfoL.first->getOperand(i));
            auto MappedInstR = dyn_cast<Instruction>(
                    OutputMappingInfoR.first->getOperand(i));
            if (!MappedInstL || !MappedInstR) {
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Output instruction mapping in "
                                       << "pattern " << Pat.Name << " contains "
                                       << "values that do not reference "
                                       << "instructions.\n");
                return false;
            }
            Pat.OutputMapping[MappedInstL] = MappedInstR;
        }
    }

    return true;
}

/// Initializes a single side of a pattern, loading all metadata, start
/// positions, and retrevies instruction mapping information.
void PatternSet::initializeInstPatternSide(InstPattern &Pat,
                                           OutputMappingInfo &OutputMapInfo,
                                           bool IsLeftSide) {
    Pattern::ValueSet *PatternInput;
    bool PatternEndFound = false;
    const Function *PatternSide;
    const Instruction **StartPosition;
    const Instruction *OutputMappingInstruction = nullptr;
    if (IsLeftSide) {
        PatternInput = &Pat.InputL;
        PatternSide = Pat.PatternL;
        StartPosition = &Pat.StartPositionL;
    } else {
        PatternInput = &Pat.InputR;
        PatternSide = Pat.PatternR;
        StartPosition = &Pat.StartPositionR;
    }

    // Initialize input from pattern function arguments.
    for (auto &&Arg : PatternSide->args()) {
        PatternInput->insert(&Arg);
    }
    // Analyse instruction data of the selected pattern side.
    for (auto &&BB : PatternSide->getBasicBlockList()) {
        for (auto &Inst : BB) {
            // Load instruction metadata.
            auto Call = dyn_cast<CallInst>(&Inst);
            auto PatMetadata = getPatternMetadata(Inst);
            if (PatMetadata) {
                Pat.MetadataMap[&Inst] = *PatMetadata;
                // Register pattern start and end positions.
                if (PatMetadata->PatternStart) {
                    if (*StartPosition) {
                        DEBUG_WITH_TYPE(
                                DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Duplicit start instruction found "
                                       << "in pattern " << Pat.Name << ". "
                                       << "Using the first one.\n");
                    } else {
                        *StartPosition = &Inst;
                    }
                }
                if (PatMetadata->PatternEnd) {
                    PatternEndFound = true;
                }
            }
            // Load input from instructions placed before the first difference
            // metadata. Do not include terminator intructions as these should
            // only be used as separators.
            if (!*StartPosition && !Inst.isTerminator()
                && (!PatMetadata || !PatMetadata->NotAnInput)) {
                PatternInput->insert(&Inst);
            }
            // Load output mapping information from the first mapping call
            // or pattern function return.
            if (!OutputMappingInstruction
                && (isa<ReturnInst>(Inst)
                    || (Call && Call->getCalledFunction()
                        && Call->getCalledFunction()->getName()
                                   == OutputMappingFunName))) {
                OutputMappingInstruction = &Inst;
            }
            // Register loads from global variables that represent arbitrary
            // values. Input ignoration metadata is automatically added to such
            // loads if not already present.
            if (auto Load = dyn_cast<LoadInst>(&Inst)) {
                auto LoadPtrConstant =
                        dyn_cast<Constant>(Load->getPointerOperand());
                auto StrippedPtrName =
                        dropSuffixes(Load->getPointerOperand()->getName());
                if (LoadPtrConstant
                    && StrippedPtrName == ArbitraryValueConstName) {
                    Pat.ArbitraryValues[Load] = LoadPtrConstant;
                    if (!PatMetadata) {
                        Pat.MetadataMap[&Inst] = PatternMetadata();
                    }
                    Pat.MetadataMap[&Inst].NotAnInput = true;
                }
            }
        }
    }

    // When no start metadata is present, use the first instruction.
    if (!*StartPosition) {
        *StartPosition = &*PatternSide->getEntryBlock().begin();
    }

    int MappedOperandsCount = 0;
    if (OutputMappingInstruction) {
        // When end metadata is missing, add it to the output mapping
        // instruction.
        if (!PatternEndFound) {
            auto OriginalMetadata =
                    Pat.MetadataMap.find(OutputMappingInstruction);
            if (OriginalMetadata == Pat.MetadataMap.end()) {
                PatternMetadata PatMetadata;
                PatMetadata.PatternEnd = true;
                Pat.MetadataMap[OutputMappingInstruction] = PatMetadata;
            } else {
                Pat.MetadataMap[OutputMappingInstruction].PatternEnd = true;
            }
        }

        // Get the number of possible instruction mapping operands.
        MappedOperandsCount = OutputMappingInstruction->getNumOperands();
        // Ignore the last operand of calls since it references the called
        // function.
        if (isa<CallInst>(OutputMappingInstruction)) {
            --MappedOperandsCount;
        }
    }
    // Generate mapping information.
    OutputMapInfo = {OutputMappingInstruction, MappedOperandsCount};
}

/// Initializes a value pattern loading value differences from both sides of
/// the pattern.
bool PatternSet::initializeValuePattern(ValuePattern &Pat) {
    // Find the compared return instruction on both sides.
    auto EntryBBL = &Pat.PatternL->getEntryBlock(),
         EntryBBR = &Pat.PatternR->getEntryBlock();
    auto TermL = EntryBBL->getTerminator(), TermR = EntryBBR->getTerminator();

    // Read the compared values.
    Pat.ValueL = TermL->getOperand(0);
    Pat.ValueR = TermR->getOperand(0);

    // Pointers in value patterns should reference global variables.
    bool IsValidPtrL = (!Pat.ValueL->getType()->isPointerTy()
                        || isa<GlobalVariable>(Pat.ValueL));
    bool IsValidPtrR = (!Pat.ValueR->getType()->isPointerTy()
                        || isa<GlobalVariable>(Pat.ValueR));
    if (!IsValidPtrL || !IsValidPtrR) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "Failed to load value pattern " << Pat.Name
                               << " since it uses pointers to parameters.\n");
        return false;
    }

    return true;
}
