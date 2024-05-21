//===-------- CustomPatternSet.cpp - Unordered set of code patterns -------===//
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

#include "CustomPatternSet.h"
#include "Config.h"
#include "Logger.h"
#include "ModuleAnalysis.h"
#include "diffkemp_patterns.h"
#include <algorithm>
#include <llvm/Support/Debug.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLTraits.h>

using namespace llvm::yaml;
using namespace llvm::sys::path;

// YAML mappings
namespace llvm {
namespace yaml {

/// YAML to unordered_map mapping.
template <typename K, typename V>
struct MappingTraits<std::unordered_map<K, V>> {
    static void mapping(IO &IO, std::unordered_map<K, V> &Map) {
        std::vector<K> *Keys = static_cast<std::vector<K> *>(IO.getContext());
        if (Keys) {
            for (auto &Key : *Keys) {
                IO.mapOptional(Key.c_str(), Map[Key]);
            }
        }
    }
};

/// YAML to PatternConfiguration mapping.
template <> struct MappingTraits<PatternConfiguration> {
    static void mapping(IO &IO, PatternConfiguration &Config) {
        IO.mapOptional("on_parse_failure", Config.OnParseFailure);
        IO.mapOptional("patterns", Config.PatternFiles);
        IO.setContext(&Config.PatternFiles);
        IO.mapOptional("clang_append", Config.ClangAppend);
    }
};

} // namespace yaml
} // namespace llvm

/// Metadata operand counts for different kinds of pattern metadata.
const StringMap<int> CustomPatternMetadata::MetadataOperandCounts = {
        {"pattern-start", 0},
        {"pattern-end", 0},
        {"group-start", 0},
        {"group-end", 0},
        {"disable-name-comparison", 0},
        {"not-an-input", 0},
        {"no-value-pattern-detection", 0}};

/// Default DiffKemp prefix for all pattern information.
const std::string CustomPatternSet::DefaultPrefix = "diffkemp.";
/// Prefix for the left (old) side of difference patterns.
const std::string CustomPatternSet::PrefixL = "old.";
/// Prefix for the right (new) side of difference patterns.
const std::string CustomPatternSet::PrefixR = "new.";
/// Complete prefix for the old side of difference patterns.
const std::string CustomPatternSet::FullPrefixL =
        CustomPatternSet::DefaultPrefix + CustomPatternSet::PrefixL;
/// Complete prefix for the right side of difference patterns.
const std::string CustomPatternSet::FullPrefixR =
        CustomPatternSet::DefaultPrefix + CustomPatternSet::PrefixR;
/// Name for the function defining output instuction mapping.
const std::string CustomPatternSet::OutputMappingFunName =
        CustomPatternSet::DefaultPrefix + "output_mapping";
/// Name for pattern metadata nodes.
const std::string CustomPatternSet::MetadataName =
        CustomPatternSet::DefaultPrefix + "pattern";

/// Create a new pattern set based on the given configuration, which can be
/// either a YAML config file or a single LLVM IR file.
CustomPatternSet::CustomPatternSet(std::string ConfigPath) {
    if (ConfigPath.empty()) {
        return;
    }

    // If a pattern is used as a configuration file, only load the pattern.
    if (extension(ConfigPath) == ".ll" || extension(ConfigPath) == ".c") {
        addPatternFromFile(ConfigPath);
    } else {
        addPatternsFromConfig(ConfigPath);
    }
}

/// Retrives pattern metadata attached to the given instruction.
std::optional<CustomPatternMetadata>
        CustomPatternSet::getPatternMetadata(const Instruction &Inst) const {
    auto InstMetadata = Inst.getMetadata(MetadataName);
    if (!InstMetadata) {
        return {};
    }

    unsigned int OperandIndex = 0;
    CustomPatternMetadata Metadata;
    while (OperandIndex < InstMetadata->getNumOperands()) {
        /// Parse the current pattern metadata operand as well as the operands
        /// that depend on it.
        if (auto Type = dyn_cast<MDString>(
                    InstMetadata->getOperand(OperandIndex).get())) {
            auto TypeName = Type->getString();
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
                Metadata.GroupEnd = true;
            } else if (TypeName == "disable-name-comparison") {
                // disable-name-comparison metadata: string type.
                Metadata.DisableNameComparison = true;
            } else if (TypeName == "not-an-input") {
                // not-an-input metadata: string type.
                Metadata.NotAnInput = true;
            } else if (TypeName == "no-value-pattern-detection") {
                // no-value-pattern-detection metadata: string type.
                Metadata.NoValuePatternDetection = true;
            } else {
                LOG("Invalid metadata type " << TypeName << " in node "
                                             << *InstMetadata << ".\n");
                return {};
            }
            // Shift the operand offset accordingly.
            OperandIndex += CustomPatternMetadata::MetadataOperandCounts.lookup(
                                    TypeName)
                            + 1;
        } else {
            return {};
        }
    }
    return {Metadata};
}

/// Load the given LLVM IR based difference pattern YAML configuration.
void CustomPatternSet::addPatternsFromConfig(std::string &ConfigPath) {
    auto ConfigFile = MemoryBuffer::getFile(ConfigPath);
    if (std::error_code EC = ConfigFile.getError()) {
        LOG("Failed to open difference "
            << "pattern configuration " << ConfigPath << ".\n");
        return;
    }

    // Parse the configuration file.
    Input YamlFile(ConfigFile.get()->getBuffer());
    PatternConfiguration Config;
    YamlFile >> Config;

    if (YamlFile.error()) {
        LOG("Failed to parse difference "
            << "pattern configuration " << ConfigPath << ".\n");
        return;
    }

    // Load all pattern files included in the configuration.
    for (auto &PatternFile : Config.PatternFiles) {
        addPatternFromFile(PatternFile);
    }
}

/// Load a pattern from the given LLVM IR module file.
void CustomPatternSet::addPatternFromFile(std::string &Path) {
    if (extension(Path) == ".c") {
        Path.pop_back(); // remove the 'c'
        Path += "ll";
    }
    // Try to load the pattern module.
    SMDiagnostic err;
    auto PatternModule = parseIRFile(Path, err, PatternContext);
    if (!PatternModule) {
        LOG("Failed to parse difference pattern module " << Path << ".\n");
        return;
    }
    if (PatternModule->getNamedValue(CPATTERN_INDICATOR)) {
        LOG("Preprocessing custom C pattern module " << Path << ".\n");
        CPass.run(PatternModule.get());
        std::error_code EC;
        raw_fd_ostream output{Path, EC};
        PatternModule->print(output, nullptr);
    }
    LOG("Loading difference patterns from module " << Path << ".\n");
    LOG_INDENT();
    addPatternFromModule(std::move(PatternModule));
    LOG_UNINDENT();
}

/// Load a pattern from an LLVM module.
void CustomPatternSet::addPatternFromModule(
        std::unique_ptr<Module> PatternModule) {
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
        LOG("Loading the difference pattern " << Name << ".\n");

        switch (getPatternType(&Function, FunctionR)) {
        case CustomPatternType::INST: {
            InstPattern NewInstPattern(Name, &Function, FunctionR);
            if (initializeInstPattern(NewInstPattern)) {
                InstPatterns.emplace(NewInstPattern);
            }
            break;
        }

        case CustomPatternType::VALUE: {
            ValuePattern NewValuePattern(Name, &Function, FunctionR);
            if (initializeValuePattern(NewValuePattern)) {
                ValuePatterns.emplace(NewValuePattern);
            }
            break;
        }

        default:
            LOG("An unknown pattern type has been used.\n");
            break;
        }
    }

    // Keep the module pointer.
    PatternModules.push_back(std::move(PatternModule));
}

/// Finds the pattern type associated with the given pattern functions.
CustomPatternType CustomPatternSet::getPatternType(const Function *FnL,
                                                   const Function *FnR) {
    // Value patterns should only contain a single return instruction.
    auto EntryBBL = &FnL->getEntryBlock(), EntryBBR = &FnR->getEntryBlock();
    if (EntryBBL->size() == 1 && EntryBBR->size() == 1) {
        // The value pattern detection might be disabled for this pattern.
        auto PatMetadataL = getPatternMetadata(*EntryBBL->begin());
        auto PatMetadataR = getPatternMetadata(*EntryBBR->begin());
        if ((!PatMetadataL || !PatMetadataL->NoValuePatternDetection)
            && (!PatMetadataR || !PatMetadataR->NoValuePatternDetection)) {
            return CustomPatternType::VALUE;
        }
    }

    return CustomPatternType::INST;
}

/// Initializes an instruction pattern, loading all metadata, start positions,
/// and the output instruction mapping. Unless the start position is chosen by
/// metadata, it is set to the first differing pair of pattern instructions.
/// Patterns with conflicting differences in concurrent branches are skipped,
/// returning false.
bool CustomPatternSet::initializeInstPattern(InstPattern &Pat) {
    OutputMappingInfo OutputMappingInfoL, OutputMappingInfoR;

    // Initialize both pattern sides.
    initializeInstPatternSide(Pat, OutputMappingInfoL, true);
    initializeInstPatternSide(Pat, OutputMappingInfoR, false);

    // Map input arguments from the left side to the right side.
    if (Pat.PatternL->arg_size() != Pat.PatternR->arg_size()) {
        LOG("The number of input arguments does not "
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
        LOG("The number of output instructions does "
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
                LOG("Output instruction mapping in pattern "
                    << Pat.Name << " contains "
                    << "values that do not reference instructions.\n");
                return false;
            }
            Pat.OutputMapping[MappedInstL] = MappedInstR;
        }
    }

    return true;
}

/// Initializes a single side of a pattern, loading all metadata, start
/// positions, and retrevies instruction mapping information.
void CustomPatternSet::initializeInstPatternSide(
        InstPattern &Pat, OutputMappingInfo &OutputMapInfo, bool IsLeftSide) {
    Pattern::InputSet *PatternInput;
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
    for (auto &BB : *PatternSide) {
        for (auto &Inst : BB) {
            // Load instruction metadata.
            auto PatMetadata = getPatternMetadata(Inst);
            if (PatMetadata) {
                Pat.MetadataMap[&Inst] = *PatMetadata;
                // If present, register start position metadata.
                if (PatMetadata->PatternStart) {
                    if (*StartPosition) {
                        LOG("Duplicit start instruction found in pattern "
                            << Pat.Name << ". Using the first one.\n");
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
            auto Call = dyn_cast<CallInst>(&Inst);
            if (!OutputMappingInstruction
                && (isa<ReturnInst>(Inst)
                    || (Call && Call->getCalledFunction()
                        && Call->getCalledFunction()->getName()
                                   == OutputMappingFunName))) {
                OutputMappingInstruction = &Inst;
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
                CustomPatternMetadata PatMetadata;
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
bool CustomPatternSet::initializeValuePattern(ValuePattern &Pat) {
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
        LOG("Failed to load value pattern "
            << Pat.Name << " since it uses pointers to parameters.\n");
        return false;
    }

    return true;
}
