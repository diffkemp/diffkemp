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

/// Checks whether any valid difference patterns are loaded.
bool PatternComparator::hasPatterns() { return !Patterns.empty(); }

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

/// Retrives pattern metadata attached to the given instruction.
void PatternComparator::getPatternMetadata(const Instruction &Inst) {
    auto instMetadata = Inst.getMetadata(MetadataName);
    if (!instMetadata) {
        return;
    }

    // TODO: Parse pattern metadata operands.
}
