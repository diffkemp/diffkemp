//===------ CPatternPass.h - preprocessing pass for custom C patterns -----===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Kucma, tomaskucma2@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the CPatternPass class, which provides
/// preprocessing for custom C patterns.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_CPATTERNPASS_H
#define DIFFKEMP_SIMPLL_CPATTERNPASS_H

#include "Utils.h"
#include <llvm/IR/Module.h>
#include <tuple>
#include <unordered_map>

using namespace llvm;

/// Class responsible for preprocessing custom C patterns, which are already
/// compiled to LLVM IR. Primary function consists of renaming functions to
/// proper names and tagging pattern starts and ends.
class CPatternPass {
    /// A pair of old and new pattern represented as LLVM functions.
    using PatternPair = std::tuple<Function *, Function *>;
    /// Map of pattern names and LLVM pattern pairs.
    std::unordered_map<std::string, CPatternPass::PatternPair> patterns{};

  public:
    /// Run the preprocessing pass.
    void run(Module *Mod);

  private:
    /// Initialize the pattern map.
    void initialize(Module *Mod);

    /// Tag patterns with "diffkemp-pattern" metadata.
    void tagPatterns(Module *Mod);

    /// Rename function to a proper LLVM pattern name.
    static void renameFunction(Function &Fun);

    /// Check whether the given instruction is part of the pattern body.
    static bool isPatternBody(Instruction *Inst);

    /// Tag the pattern end in a given function.
    void tagPatternEnd(Function &Fun);
};

#endif // DIFFKEMP_SIMPLL_CPATTERNPASS_H
