//===----------------- FFI.cpp - C interface for SimpLL -------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains definitions of C functions used for interacting with
/// DiffKemp.
///
//===----------------------------------------------------------------------===//

#include "FFI.h"
#include "Config.h"
#include "ModuleAnalysis.h"
#include "Output.h"
#include <cstring>

extern "C" {
void runSimpLL(const char *ModL,
               const char *ModR,
               const char *ModLOut,
               const char *ModROut,
               const char *FunL,
               const char *FunR,
               struct config Conf,
               char *Output) {
    Config config(FunL,
                  FunR,
                  ModL,
                  ModR,
                  ModLOut,
                  ModROut,
                  Conf.CacheDir,
                  Conf.Variable,
                  Conf.ControlFlowOnly,
                  Conf.PrintAsmDiffs,
                  Conf.PrintCallStacks,
                  Conf.Verbose,
                  Conf.VerboseMacros);

    OverallResult Result;
    processAndCompare(config, Result);

    std::string outputString = reportOutputToString(config, Result);
    strcpy(Output, outputString.c_str());

    llvm_shutdown();
}
}
