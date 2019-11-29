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
#include "ModuleComparator.h"
#include "Transforms.h"
#include "Utils.h"

/// Report the overall result in YAML format to stdout.
/// \param config Configuration.
/// \param Result The overall result of the comparison
void reportOutput(Config &config, OverallResult &Result);

#endif // DIFFKEMP_SIMPLL_OUTPUT_H
