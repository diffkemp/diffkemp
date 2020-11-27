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

/// Initializes a pattern, loading all metadata and start positions. Unless
/// the start position is chosen by metadata, it is set to the first differing
/// pair of pattern instructions. Patterns with conflicting differences in
/// concurrent branches are skipped, returning false.
bool PatternSet::initializePattern(Pattern &Pat) {
    PatternMetadata Metadata;

    // Initialize the new side of the pattern.
    for (auto &&BB : Pat.NewPattern->getBasicBlockList()) {
        for (auto &Inst : BB) {
            Metadata = {};
            if (getPatternMetadata(Metadata, Inst)) {
                Pat.MetadataMap[&Inst] = Metadata;
                if (!Pat.NewStartPosition && Metadata.FirstDifference) {
                    Pat.NewStartPosition = &Inst;
                }
            }
        }
    }

    // Initialize the old side of the pattern.
    for (auto &&BB : Pat.OldPattern->getBasicBlockList()) {
        for (auto &Inst : BB) {
            Metadata = {};
            if (getPatternMetadata(Metadata, Inst)) {
                Pat.MetadataMap[&Inst] = Metadata;
                if (!Pat.OldStartPosition && Metadata.FirstDifference) {
                    Pat.OldStartPosition = &Inst;
                }
            }
        }
    }

    // TODO: Search for the first difference if not given in metadata.
    if (!Pat.NewStartPosition || !Pat.OldStartPosition) {
        return false;
    }

    return true;
}

/// Parses a single pattern metadata operand, including all dependent operands.
/// The total metadata operand offset is returned unless the parsing fails.
int PatternSet::parseMetadataOperand(PatternMetadata &PatternMetadata,
                                     const MDNode *InstMetadata,
                                     const int Index) const {
    // Metadata offsets
    const int BasicBlockLimitOffset = 2;
    const int BasicBlockLimitEndOffset = 1;
    const int FirstDifferenceOffset = 1;

    if (auto Type = dyn_cast<MDString>(InstMetadata->getOperand(Index).get())) {
        if (Type->getString() == "basic-block-limit") {
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
        } else if (Type->getString() == "basic-block-limit-end") {
            // basic-block-limit-end metadata: string type.
            PatternMetadata.BasicBlockLimitEnd = true;
            return BasicBlockLimitEndOffset;
        } else if (Type->getString() == "first-difference") {
            // first-difference metadata: string type.
            PatternMetadata.FirstDifference = true;
            return FirstDifferenceOffset;
        }
    }

    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << getDebugIndent()
                           << "Failed to parse pattern metadata "
                           << "from node " << *InstMetadata << ".\n");

    return -1;
}
