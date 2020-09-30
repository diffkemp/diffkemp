//===------------- PatternComparator.h - Code pattern finder --------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the LLVM code pattern finder and
/// comparator.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
#define DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H

#include <llvm/ADT/StringMap.h>
#include <llvm/IR/Module.h>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace llvm;

/// Representation of the whole difference pattern configuration.
struct PatternConfiguration {
    std::string onParseFailure;
    std::vector<std::string> patternFiles;
};

/// Representation of difference pattern metadata configuration.
struct PatternMetadata {
    int basicBlockLimit = -1;
    bool basicBlockLimitEnd = false;
};

/// Compares difference patterns againts functions, possibly eliminating reports
/// of prior semantic differences.
class PatternComparator {
  public:
    /// Name for pattern metadata nodes.
    static const StringRef MetadataName;
    /// Prefix for the new side of difference patterns.
    static const StringRef NewPrefix;
    /// Prefix for the old side of difference patterns.
    static const StringRef OldPrefix;

    PatternComparator(std::string configPath);

    ~PatternComparator();

    /// Add a new difference pattern.
    void addPattern(std::string &path);

    /// Checks whether any difference patterns are loaded.
    bool hasPatterns();

    /// Retrives pattern metadata attached to the given instruction, returning
    /// true for valid pattern metadata nodes.
    bool getPatternMetadata(PatternMetadata &metadata, const Instruction &Inst);

  private:
    /// Settings applied to all pattern files.
    StringMap<std::string> GlobalSettings;
    /// Set of loaded difference patterns.
    std::set<std::tuple<StringRef, Function *, Function *>> Patterns;
    /// Map of loaded pattern modules.
    std::unordered_map<Module *, std::unique_ptr<Module>> PatternModules;
    /// Map of loaded pattern module contexts.
    std::unordered_map<Module *, std::unique_ptr<LLVMContext>> PatternContexts;

    /// Load the given configuration file.
    void loadConfig(std::string &configPath);

    /// Parses a single pattern metadata operand, including all dependent
    /// operands.
    int parseMetadataOperand(PatternMetadata &patternMetadata,
                             const MDNode *instMetadata,
                             const int index);
};

#endif // DIFFKEMP_SIMPLL_PATTERNCOMPARATOR_H
