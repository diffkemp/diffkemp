//===------------- PatternComparator.h - Code pattern finder --------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the implementation of the LLVM code pattern finder and
/// comparator, which enables eliminations of reports of known differences.
///
//===----------------------------------------------------------------------===//

#include "PatternComparator.h"
#include "Config.h"
#include "Utils.h"
#include "library/ModuleLoader.h"
#include <algorithm>
#include <llvm/Support/Debug.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/YAMLTraits.h>

using namespace llvm::yaml;
using namespace llvm::sys::path;

// YAML to PatternConfiguration mapping
namespace llvm::yaml {
template <> struct MappingTraits<PatternConfiguration> {
    static void mapping(IO &io, PatternConfiguration &config) {
        io.mapOptional("on_parse_failure", config.onParseFailure);
        io.mapOptional("patterns", config.patternFiles);
    }
};
} // namespace llvm::yaml

/// Name for pattern metadata nodes.
const StringRef PatternComparator::MetadataName = "diffkemp.pattern";
/// Prefix for the new side of difference patterns.
const StringRef PatternComparator::NewPrefix = "new_";
/// Prefix for the old side of difference patterns.
const StringRef PatternComparator::OldPrefix = "old_";

/// Create a new comparator based on the given configuration.
PatternComparator::PatternComparator(std::string configPath) {
    if (configPath.empty()) {
        return;
    }

    // If a pattern is used as a configuration file, only load the pattern.
    if (extension(configPath) == ".ll") {
        addPattern(configPath);
    } else {
        loadConfig(configPath);
    }
}

/// Destroy the pattern comparator, clearing all modules and contexts.
PatternComparator::~PatternComparator() {
    PatternModules.clear();
    PatternContexts.clear();
}

/// Checks whether any valid difference patterns are loaded.
bool PatternComparator::hasPatterns() { return !Patterns.empty(); }

/// Add a new LLVM IR difference pattern file.
void PatternComparator::addPattern(std::string &path) {
    Module *PatternModule = loadModule(path, PatternModules, PatternContexts);
    if (!PatternModule) {
        DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                        dbgs() << getDebugIndent()
                               << "Failed to parse difference pattern "
                               << "module " << path << ".\n");
        return;
    }

    for (auto &Function : PatternModule->getFunctionList()) {
        // Select only functions starting with the new prefix.
        if (Function.getName().startswith_lower(NewPrefix)) {
            StringRef name = Function.getName().substr(NewPrefix.size());
            std::string oldName = OldPrefix;
            oldName += name;

            // Find the corresponding pattern function with the old prefix.
            auto oldFunction = PatternModule->getFunction(oldName);
            if (oldFunction) {
                Patterns.emplace(name, &Function, oldFunction);

                DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                                dbgs() << getDebugIndent()
                                       << "Loaded difference pattern "
                                       << name.str() << " from module " << path
                                       << ".\n");
            }
        }
    }
}

/// Load the given LLVM IR based difference pattern YAML configuration.
void PatternComparator::loadConfig(std::string &configPath) {
    auto configFile = MemoryBuffer::getFile(configPath);
    if (std::error_code ec = configFile.getError()) {
        DEBUG_WITH_TYPE(
                DEBUG_SIMPLL,
                dbgs() << getDebugIndent() << "Failed to open difference "
                       << "pattern configuration " << configPath << ".\n");
        return;
    }

    // Parse the configuration file.
    Input yamlFile(configFile.get()->getBuffer());
    PatternConfiguration config;
    yamlFile >> config;

    if (yamlFile.error()) {
        DEBUG_WITH_TYPE(
                DEBUG_SIMPLL,
                dbgs() << getDebugIndent() << "Failed to parse difference "
                       << "pattern configuration " << configPath << ".\n");
        return;
    }

    // Load all pattern files included in the configuration.
    for (auto &patternFile : config.patternFiles) {
        addPattern(patternFile);
    }
}

/// Retrives pattern metadata attached to the given instruction, returning
/// true for valid pattern metadata nodes.
bool PatternComparator::getPatternMetadata(PatternMetadata &metadata,
                                           const Instruction &Inst) {
    auto instMetadata = Inst.getMetadata(MetadataName);
    if (!instMetadata) {
        return false;
    }

    // Erase previous metadata information.
    metadata = {};

    int operandIndex = 0;
    while (operandIndex < instMetadata->getNumOperands()) {
        int indexOffset =
                parseMetadataOperand(metadata, instMetadata, operandIndex);

        // Continue only when the metadata operand gets parsed successfully.
        if (indexOffset < 0) {
            return false;
        }

        operandIndex += indexOffset;
    }

    return true;
}

/// Parses a single pattern metadata operand, including all dependent operands.
/// The total metadata operand offset is returned unless the parsing fails.
int PatternComparator::parseMetadataOperand(PatternMetadata &patternMetadata,
                                            const MDNode *instMetadata,
                                            const int index) {
    if (auto type = dyn_cast<MDString>(instMetadata->getOperand(index).get())) {
        // basic-block-limit metadata: string type, int limit.
        if (type->getString() == "basic-block-limit") {
            ConstantAsMetadata *limitConst;
            if (instMetadata->getNumOperands() > (index + 1)
                && (limitConst = dyn_cast<ConstantAsMetadata>(
                            instMetadata->getOperand(index + 1).get()))) {
                if (auto limit =
                            dyn_cast<ConstantInt>(limitConst->getValue())) {
                    patternMetadata.basicBlockLimit =
                            limit->getValue().getZExtValue();
                    return 2;
                }
            }
            // basic-block-limit-end metadata: string type.
        } else if (type->getString() == "basic-block-limit-end") {
            patternMetadata.basicBlockLimitEnd = true;
            return 1;
        }
    }

    DEBUG_WITH_TYPE(DEBUG_SIMPLL,
                    dbgs() << getDebugIndent()
                           << "Failed to parse pattern metadata "
                           << "from node " << *instMetadata << ".\n");

    return -1;
}