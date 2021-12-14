//===------------------ SimpLL.cpp - SimpLL entry point -------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the main function of the SimpLL tool.
///
//===----------------------------------------------------------------------===//

#include "Config.h"
#include "ModuleAnalysis.h"
#include "ModuleComparator.h"
#include "Output.h"
#include "Utils.h"

using namespace llvm;

int main(int argc, const char **argv) {
    // Parse CLI options
    cl::ParseCommandLineOptions(argc, argv);
    Config config;

    // Run transformations and the comparison.
    OverallResult Result;
    processAndCompare(config, Result);

    // Report the result to standard output.
    reportOutput(Result);

    llvm_shutdown();
    return 0;
}
