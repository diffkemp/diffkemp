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

#include <llvm/ADT/DenseMap.h>
#include <llvm/ADT/SmallPtrSet.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/IR/Module.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;

/// Input instructions and arguments.
typedef SmallPtrSet<const Value *, 16> InputSet;

/// Mapping between input values from different pattern sides.
typedef DenseMap<const Value *, const Value *> InputMap;

/// Instructions pointer set.
typedef SmallPtrSet<const Instruction *, 32> InstructionSet;

/// Instruction to instruction mapping.
typedef DenseMap<const Instruction *, const Instruction *> InstructionMap;

// Forward declaration of the DifferentialFunctionComparator.
class DifferentialFunctionComparator;

/// Representation of difference pattern metadata configuration.
struct PatternMetadata {
    /// Marker for the first differing instruction pair.
    bool PatternStart = false;
    /// Marker for the last differing instruction pair.
    bool PatternEnd = false;
    /// Prevents skipping of module instructions when no match is found.
    bool GroupStart = false;
    /// End of the previous instruction group.
    bool GroupEnd = false;
    /// Disables the default name-based comparison of globals and structures.
    bool DisableNameComparison = false;
    /// Does not register the instruction as an input.
    bool NotAnInput = false;
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
    /// Function corresponding to the left part of the pattern.
    const Function *PatternL;
    /// Function corresponding to the right part of the pattern.
    const Function *PatternR;
    /// Map of all included pattern metadata.
    mutable std::unordered_map<const Value *, PatternMetadata> MetadataMap;
    /// Input instructions and arguments for the left part of the pattern.
    mutable InputSet InputL;
    /// Input instructions and arguments for the right part of the pattern.
    mutable InputSet InputR;
    /// Mapping of input arguments from new to old part of the pattern.
    mutable InputMap ArgumentMapping;
    /// Final instruction mapping associated with the pattern.
    mutable InstructionMap FinalMapping;
    /// Comparison start position for the left part of the pattern.
    const Instruction *StartPositionL = nullptr;
    /// Comparison start position for the right part of the pattern.
    const Instruction *StartPositionR = nullptr;

    Pattern(const std::string &Name,
            const Function *PatternL,
            const Function *PatternR)
            : Name(Name), PatternL(PatternL), PatternR(PatternR) {}

    bool operator==(const Pattern &Rhs) const {
        return (Name == Rhs.Name && PatternL == Rhs.PatternL
                && PatternR == Rhs.PatternR);
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
    /// Default DiffKemp prefix for all pattern information.
    static const std::string DefaultPrefix;
    /// Prefix for the left (old) side of difference patterns.
    static const std::string PrefixL;
    /// Prefix for the right (new) side of difference patterns.
    static const std::string PrefixR;
    /// Complete prefix for the left side of difference patterns.
    static const std::string FullPrefixL;
    /// Complete prefix for the right side of difference patterns.
    static const std::string FullPrefixR;
    /// Name for the function defining final instuction mapping.
    static const std::string MappingFunctionName;
    /// Name for pattern metadata nodes.
    static const std::string MetadataName;

    PatternSet(std::string ConfigPath);

    ~PatternSet();

    /// Retrives pattern metadata attached to the given instruction, returning
    /// true for valid pattern metadata nodes.
    bool getPatternMetadata(PatternMetadata &Metadata,
                            const Instruction &Inst) const;

    /// Checks whether the difference pattern set is empty.
    bool empty() const noexcept { return Patterns.empty(); }

    /// Returns a constant iterator pointing to the first difference pattern.
    std::unordered_set<Pattern>::iterator begin() noexcept {
        return Patterns.begin();
    }

    /// Returns a constant iterator pointing beyond the last difference pattern.
    std::unordered_set<Pattern>::iterator end() noexcept {
        return Patterns.end();
    }

    /// Returns a constant iterator pointing to the first difference pattern.
    std::unordered_set<Pattern>::const_iterator begin() const noexcept {
        return Patterns.begin();
    }

    /// Returns a constant iterator pointing beyond the last difference pattern.
    std::unordered_set<Pattern>::const_iterator end() const noexcept {
        return Patterns.end();
    }

    /// Returns a constant iterator pointing to the first difference pattern.
    std::unordered_set<Pattern>::const_iterator cbegin() const noexcept {
        return Patterns.cbegin();
    }

    /// Returns a constant iterator pointing beyond the last difference pattern.
    std::unordered_set<Pattern>::const_iterator cend() const noexcept {
        return Patterns.cend();
    }

  private:
    /// Basic information about the final instruction mapping present on one
    /// side of a pattern.
    using MappingInfo = std::pair<const Instruction *, int>;

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

    /// Initializes a pattern, loading all metadata, start positions, and the
    /// final instruction mapping.
    bool initializePattern(Pattern &Pat);

    /// Initializes a single side of a pattern, loading all metadata, start
    /// positions, and retrevies instruction mapping information.
    void initializePatternSide(Pattern &Pat,
                               MappingInfo &MapInfo,
                               bool IsLeftSide);

    /// Parses a single pattern metadata operand, including all dependent
    /// operands.
    int parseMetadataOperand(PatternMetadata &PatternMetadata,
                             const MDNode *InstMetadata,
                             const int Index) const;
};

#endif // DIFFKEMP_SIMPLL_PATTERNSET_H
