//===------------------- Output.h - Reporting results ---------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declaration of a function for reporting results of the
/// simplification.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_OUTPUT_H
#define DIFFKEMP_SIMPLL_OUTPUT_H

#include "Config.h"
#include "Utils.h"

/// Report results in YAML format to stdout.
/// \param config Configuration.
/// \param nonequalFuns List of non-equal functions.
void reportOutput(Config &config, std::vector<FunPair> &nonequalFuns);

#endif // DIFFKEMP_SIMPLL_OUTPUT_H
