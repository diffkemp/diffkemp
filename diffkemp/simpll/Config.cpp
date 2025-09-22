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
#include "Logger.h"
#include <llvm/Support/Debug.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

Config::Config(std::string FirstFunName,
               std::string SecondFunName,
               Module *FirstModule,
               Module *SecondModule,
               std::string FirstOutFile,
               std::string SecondOutFile,
               std::string CacheDir,
               std::string CustomPatternConfigPath,
               BuiltinPatterns Patterns,
               unsigned SmtTimeout,
               std::string Variable,
               bool OutputLlvmIR,
               bool PrintAsmDiffs,
               bool PrintCallStacks,
               bool ExtendedStat,
               int Verbosity)
        : FirstFunName(FirstFunName), SecondFunName(SecondFunName),
          First(FirstModule), Second(SecondModule), FirstOutFile(FirstOutFile),
          SecondOutFile(SecondOutFile), CacheDir(CacheDir),
          CustomPatternConfigPath(CustomPatternConfigPath),
          SmtTimeout(SmtTimeout), Patterns(Patterns),
          OutputLlvmIR(OutputLlvmIR), PrintAsmDiffs(PrintAsmDiffs),
          PrintCallStacks(PrintCallStacks), ExtendedStat(ExtendedStat) {
    refreshFunctions();

    if (!Variable.empty()) {
        FirstVar = First->getGlobalVariable(Variable, true);
        SecondVar = Second->getGlobalVariable(Variable, true);
    }
    logger.setVerbosity(Verbosity);
}

void Config::refreshFunctions() {
    FirstFun = First->getFunction(FirstFunName);
    SecondFun = Second->getFunction(SecondFunName);
}
