//===------------ PatternSet.h - Unordered set of code patterns -----------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Petr Silling, psilling@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the unordered LLVM code pattern set.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_PATTERNSET_H
#define DIFFKEMP_SIMPLL_PATTERNSET_H

#include <llvm/ADT/StringMap.h>
#include <llvm/IR/Module.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;

// Forward declaration of the DifferentialFunctionComparator.
class DifferentialFunctionComparator;

/// Representation of difference pattern metadata configuration.
struct PatternMetadata {
    /// Limit for the number of following basic blocks.
    int BasicBlockLimit = -1;
    /// End of the previous basic block limit.
    bool BasicBlockLimitEnd = false;
    /// Marker for the first differing instruction pair.
    bool FirstDifference = false;
};

/// Representation of the whole difference pattern configuration.
struct PatternConfiguration {
    /// Logging option for parse failures.
    std::string OnParseFailure;
    /// Vector of paths to pattern files.
    std::vector<std::string> PatternFiles;
};

/// Representation of a difference pattern pair.
struct Pattern {
    /// Name of the pattern.
    const std::string Name;
    /// Function corresponding to the new part of the pattern.
    const Function *NewPattern;
    /// Function corresponding to the old part of the pattern.
    const Function *OldPattern;
    /// Map of all included pattern metadata.
    std::unordered_map<const Instruction *, PatternMetadata> MetadataMap;
    /// Comparison start position for the new part of the pattern.
    const Instruction *NewStartPosition = nullptr;
    /// Comparison start position for the old part of the pattern.
    const Instruction *OldStartPosition = nullptr;

    Pattern(const std::string &Name,
            const Function *NewPattern,
            const Function *OldPattern)
            : Name(Name), NewPattern(NewPattern), OldPattern(OldPattern) {}

    bool operator==(const Pattern &Rhs) const {
        return (Name == Rhs.Name && NewPattern == Rhs.NewPattern
                && OldPattern == Rhs.OldPattern);
    }
};

// Define a hash function for difference patterns.
namespace std {
template <> struct hash<Pattern> {
    std::size_t operator()(const Pattern &Pat) const noexcept {
        return std::hash<std::string>()(Pat.Name);
    }
};
} // namespace std

/// Compares difference patterns against functions, possibly eliminating reports
/// of prior semantic differences.
class PatternSet {
  public:
    PatternSet(std::string ConfigPath);

    ~PatternSet();

    /// Retrives pattern metadata attached to the given instruction, returning
    /// true for valid pattern metadata nodes.
    bool getPatternMetadata(PatternMetadata &Metadata,
                            const Instruction &Inst) const;

    /// Checks whether the difference pattern set is empty.
    bool empty() const noexcept;

    /// Returns a constant iterator pointing to the first difference pattern.
    std::unordered_set<Pattern>::iterator begin() noexcept;

    /// Returns a constant iterator pointing beyond the last difference pattern.
    std::unordered_set<Pattern>::iterator end() noexcept;

    /// Returns a constant iterator pointing to the first difference pattern.
    std::unordered_set<Pattern>::const_iterator begin() const noexcept;

    /// Returns a constant iterator pointing beyond the last difference pattern.
    std::unordered_set<Pattern>::const_iterator end() const noexcept;

    /// Returns a constant iterator pointing to the first difference pattern.
    std::unordered_set<Pattern>::const_iterator cbegin() const noexcept;

    /// Returns a constant iterator pointing beyond the last difference pattern.
    std::unordered_set<Pattern>::const_iterator cend() const noexcept;

  private:
    /// Name for pattern metadata nodes.
    static const std::string MetadataName;
    /// Prefix for the new side of difference patterns.
    static const std::string NewPrefix;
    /// Prefix for the old side of difference patterns.
    static const std::string OldPrefix;
    /// Settings applied to all pattern files.
    StringMap<std::string> GlobalSettings;
    /// Map of loaded pattern modules.
    std::unordered_map<Module *, std::unique_ptr<Module>> PatternModules;
    /// Map of loaded pattern module contexts.
    std::unordered_map<Module *, std::unique_ptr<LLVMContext>> PatternContexts;
    /// Set of loaded difference patterns.
    std::unordered_set<Pattern> Patterns;

    /// Load the given configuration file.
    void loadConfig(std::string &ConfigPath);

    /// Add a new difference pattern.
    void addPattern(std::string &Path);

    /// Initializes a pattern, loading all metadata and start positions.
    bool initializePattern(Pattern &Pat);

    /// Parses a single pattern metadata operand, including all dependent
    /// operands.
    int parseMetadataOperand(PatternMetadata &PatternMetadata,
                             const MDNode *InstMetadata,
                             const int Index) const;
};

#endif // DIFFKEMP_SIMPLL_PATTERNSET_H
