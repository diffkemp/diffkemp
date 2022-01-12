//===----------------- Config.cpp - Parsing CLI options -------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definition of command line options, their parsing, and
/// setting the tool configuration.
///
//===----------------------------------------------------------------------===//

#include "Config.h"
#include <llvm/Support/Debug.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

/// Sets debug types specified in the vector.
void Config::setDebugTypes(std::vector<std::string> &debugTypes) {
    if (!debugTypes.empty()) {
        DebugFlag = true;
        // Transform vector of strings into char ** (array of char *)
        std::vector<const char *> types;
        std::transform(debugTypes.begin(),
                       debugTypes.end(),
                       std::back_inserter(types),
                       [](const std::string &s) { return s.c_str(); });
        setCurrentDebugTypes(&types[0], debugTypes.size());
    }
}

Config::Config(std::string FirstFunName,
               std::string SecondFunName,
               Module *FirstModule,
               Module *SecondModule,
               std::string FirstOutFile,
               std::string SecondOutFile,
               std::string CacheDir,
               std::string Variable,
               bool OutputLlvmIR,
               bool ControlFlowOnly,
               bool PrintAsmDiffs,
               bool PrintCallStacks,
               int Verbosity)
        : FirstFunName(FirstFunName), SecondFunName(SecondFunName),
          First(FirstModule), Second(SecondModule), FirstOutFile(FirstOutFile),
          SecondOutFile(SecondOutFile), CacheDir(CacheDir),
          OutputLlvmIR(OutputLlvmIR), ControlFlowOnly(ControlFlowOnly),
          PrintAsmDiffs(PrintAsmDiffs), PrintCallStacks(PrintCallStacks) {
    refreshFunctions();

    if (!Variable.empty()) {
        FirstVar = First->getGlobalVariable(Variable, true);
        SecondVar = Second->getGlobalVariable(Variable, true);
    }

    std::vector<std::string> debugTypes;
    if (Verbosity > 0) {
        // Enable debugging output in passes. Intended fallthrough
        switch (Verbosity) {
        default:
        case 3:
            debugTypes.emplace_back(DEBUG_SIMPLL_VERBOSE_EXTRA);
            [[fallthrough]];
        case 2:
            debugTypes.emplace_back(DEBUG_SIMPLL_VERBOSE);
            [[fallthrough]];
        case 1:
            debugTypes.emplace_back(DEBUG_SIMPLL);
            break;
        }
    }
    setDebugTypes(debugTypes);
}

void Config::refreshFunctions() {
    FirstFun = First->getFunction(FirstFunName);
    SecondFun = Second->getFunction(SecondFunName);
}
