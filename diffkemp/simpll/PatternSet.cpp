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
/// and hold all valid patterns that have been referenced there.
///
//===----------------------------------------------------------------------===//

#include "PatternSet.h"
#include "Config.h"
#include "ModuleAnalysis.h"
#include "Utils.h"
#include "library/ModuleLoader.h"
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

/// Name for the function defining final instuction mapping.
const std::string PatternSet::MappingFunctionName = "diffkemp.mapping";
/// Name for pattern metadata nodes.
const std::string PatternSet::MetadataName = "diffkemp.pattern";
/// Prefix for the new side of difference patterns.
const std::string PatternSet::NewPrefix = "new_";
/// Prefix for the old side of difference patterns.
const std::string PatternSet::OldPrefix = "old_";

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

/// Destroy the pattern set, clearing all modules and contexts.
PatternSet::~PatternSet() {
    PatternModules.clear();
    PatternContexts.clear();
}

/// Retrives pattern metadata attached to the given instruction, returning
/// true for valid pattern metadata nodes.
bool PatternSet::getPatternMetadata(PatternMetadata &Metadata,
                                    const Instruction &Inst) const {
    auto InstMetadata = Inst.getMetadata(MetadataName);
    if (!InstMetadata) {
        return false;
    }

    int OperandIndex = 0;
    while (OperandIndex < InstMetadata->getNumOperands()) {
        int IndexOffset =
                parseMetadataOperand(Metadata, InstMetadata, OperandIndex);

        // Continue only when the metadata operand gets parsed successfully.
        if (IndexOffset < 0) {
            return false;
        }

        OperandIndex += IndexOffset;
    }

    return true;
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
    Module *PatternModule = loadModule(Path, PatternModules, PatternContexts);
    if (!PatternModule) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "Failed to parse difference pattern "
                               << "module " << Path << ".\n");
        return;
    }

    for (auto &Function : PatternModule->getFunctionList()) {
        // Select only functions starting with the new prefix.
        if (Function.getName().startswith_lower(NewPrefix)) {
            StringRef Name = Function.getName().substr(NewPrefix.size());
            std::string OldName = OldPrefix;
            OldName += Name;

            // Find the corresponding pattern function with the old prefix.
            auto OldFunction = PatternModule->getFunction(OldName);
            if (OldFunction) {
                Pattern NewPattern(Name.str(), &Function, OldFunction);
                if (initializePattern(NewPattern)) {
                    Patterns.emplace(NewPattern);

                    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                    dbgs() << getDebugIndent()
                                           << "Loaded difference pattern "
                                           << Name.str() << " from module "
                                           << Path << ".\n");
                }
            }
        }
    }
}

/// Initializes a pattern, loading all metadata, start positions, and the final
/// instruction mapping. Unless the start position is chosen by metadata, it is
/// set to the first differing pair of pattern instructions. Patterns with
/// conflicting differences in concurrent branches are skipped, returning false.
bool PatternSet::initializePattern(Pattern &Pat) {
    MappingInfo NewMappingInfo;
    MappingInfo OldMappingInfo;

    // Initialize both pattern sides.
    initializePatternSide(Pat, NewMappingInfo, true);
    initializePatternSide(Pat, OldMappingInfo, false);

    // Create references for the expected final instruction mapping.
    if (NewMappingInfo.second != OldMappingInfo.second) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "The number of mapped instructions does "
                               << "not match in pattern " << Pat.Name << ".\n");
        return false;
    }
    if (NewMappingInfo.first && OldMappingInfo.first) {
        for (int i = 0; i < NewMappingInfo.second; ++i) {
            auto NewMappedInst =
                    dyn_cast<Instruction>(NewMappingInfo.first->getOperand(i));
            auto OldMappedInst =
                    dyn_cast<Instruction>(OldMappingInfo.first->getOperand(i));
            if (!NewMappedInst || !OldMappedInst) {
                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Instruction mapping in pattern "
                                       << Pat.Name << " contains values that "
                                       << "do not reference instructions.\n");
                return false;
            }
            Pat.FinalMapping[NewMappedInst] = OldMappedInst;
        }
    }

    // TODO: Search for the first difference if not given in metadata.
    if (!Pat.NewStartPosition || !Pat.OldStartPosition) {
        return false;
    }

    return true;
}

/// Initializes a single side of a pattern, loading all metadata, start
/// positions, and retrevies instruction mapping information.
void PatternSet::initializePatternSide(Pattern &Pat,
                                       MappingInfo &MapInfo,
                                       bool IsNewSide) {
    PatternMetadata Metadata;
    const Function *PatternSide;
    const Instruction **StartPosition;
    const Instruction *MappingInstruction = nullptr;
    if (IsNewSide) {
        PatternSide = Pat.NewPattern;
        StartPosition = &Pat.NewStartPosition;
    } else {
        PatternSide = Pat.OldPattern;
        StartPosition = &Pat.OldStartPosition;
    }

    // Initialize the selected pattern side.
    for (auto &&BB : PatternSide->getBasicBlockList()) {
        for (auto &Inst : BB) {
            // Load instruction metadata.
            Metadata = {};
            if (getPatternMetadata(Metadata, Inst)) {
                Pat.MetadataMap[&Inst] = Metadata;
                // If present, register start position metadata.
                if (!*StartPosition && Metadata.PatternStart) {
                    *StartPosition = &Inst;
                }
            }
            // Load instruction mapping information from the first mapping call
            // or pattern function return.
            auto Call = dyn_cast<CallInst>(&Inst);
            if (!MappingInstruction
                && (isa<ReturnInst>(Inst)
                    || (Call && Call->getCalledFunction()
                        && Call->getCalledFunction()->getName()
                                   == MappingFunctionName))) {
                MappingInstruction = &Inst;
            }
        }
    }

    // Get the number of possible instruction mapping operands.
    int MappedOperandsCount = 0;
    if (MappingInstruction) {
        MappedOperandsCount = MappingInstruction->getNumOperands();
        // Ignore the last operand of calls since it references the called
        // function.
        if (isa<CallInst>(MappingInstruction)) {
            --MappedOperandsCount;
        }
    }
    // Generate mapping information.
    MapInfo = {MappingInstruction, MappedOperandsCount};
}

/// Parses a single pattern metadata operand, including all dependent operands.
/// The total metadata operand offset is returned unless the parsing fails.
int PatternSet::parseMetadataOperand(PatternMetadata &PatternMetadata,
                                     const MDNode *InstMetadata,
                                     const int Index) const {
    // Metadata offsets
    const int BasicBlockLimitOffset = 2;
    const int BasicBlockLimitEndOffset = 1;
    const int PatternStartOffset = 1;
    const int PatternEndOffset = 1;

    if (auto Type = dyn_cast<MDString>(InstMetadata->getOperand(Index).get())) {
        auto TypeName = Type->getString();
        if (TypeName == "basic-block-limit") {
            // basic-block-limit metadata: string type, int limit.
            ConstantAsMetadata *LimitConst;
            if (InstMetadata->getNumOperands() > (Index + 1)
                && (LimitConst = dyn_cast<ConstantAsMetadata>(
                            InstMetadata->getOperand(Index + 1).get()))) {
                if (auto Limit =
                            dyn_cast<ConstantInt>(LimitConst->getValue())) {
                    PatternMetadata.BasicBlockLimit =
                            Limit->getValue().getZExtValue();
                    return BasicBlockLimitOffset;
                }
            }
        } else if (TypeName == "basic-block-limit-end") {
            // basic-block-limit-end metadata: string type.
            PatternMetadata.BasicBlockLimitEnd = true;
            return BasicBlockLimitEndOffset;
        } else if (TypeName == "pattern-start") {
            // pattern-start metadata: string type.
            PatternMetadata.PatternStart = true;
            return PatternStartOffset;
        } else if (TypeName == "pattern-end") {
            // pattern-end metadata: string type.
            PatternMetadata.PatternEnd = true;
            return PatternEndOffset;
        }
    }

    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << getDebugIndent()
                           << "Failed to parse pattern metadata "
                           << "from node " << *InstMetadata << ".\n");

    return -1;
}
