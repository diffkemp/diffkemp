//===------------------ Config.h - Parsing CLI options --------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the Config class that stores parsed
/// command line options.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_CONFIG_H
#define DIFFKEMP_SIMPLL_CONFIG_H

#include "llvm/Support/CommandLine.h"
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>

#define DEBUG_SIMPLL "debug-simpll"

using namespace llvm;

// Declaration of command line options
extern cl::opt<std::string> FirstFileOpt;
extern cl::opt<std::string> SecondFileOpt;
extern cl::opt<std::string> FunctionOpt;
extern cl::opt<std::string> VariableOpt;
extern cl::opt<std::string> SuffixOpt;
extern cl::opt<bool> ControlFlowOpt;
extern cl::opt<bool> PrintCallstacksOpt;
extern cl::opt<bool> VerboseOpt;

/// Tool configuration parsed from CLI options.
class Config {
  private:
    SMDiagnostic err;
    LLVMContext context_first;
    LLVMContext context_second;

    std::string FirstFunName;
    std::string SecondFunName;

  public:
    // Parsed LLVM modules
    std::unique_ptr<Module> First;
    std::unique_ptr<Module> Second;
    // Compared functions
    Function *FirstFun = nullptr;
    Function *SecondFun = nullptr;
    // Compared global variables
    GlobalVariable *FirstVar = nullptr;
    GlobalVariable *SecondVar = nullptr;
    // Output files
    std::string FirstOutFile;
    std::string SecondOutFile;
    // Cache file directory.
    std::string CacheDir;

    // Keep only control-flow related instructions
    bool ControlFlowOnly;
    // Print raw differences in inline assembly.
    bool PrintAsmDiffs;
    // Show call stacks for non-equal functions
    bool PrintCallStacks;

    Config();

    void refreshFunctions();
};

#endif // DIFFKEMP_SIMPLL_CONFIG_H
