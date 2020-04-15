//===------ DiffKempUtils.h - utility functions for use in DiffKemp -------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declarations of functions for looking up locations of
/// functions and global variables that affect module and sysctl parameters for
/// use in the generate phase of DiffKemp.
///
//===----------------------------------------------------------------------===//

#include "Utils.h"
#include <llvm/IR/Module.h>
#include <set>
#include <vector>

using namespace llvm;

/// Find global variable in the module that corresponds to the given param.
/// In case the param is defined by module_param_named, this can be different
/// from the param name.
/// Information about the variable is stored inside the last element of the
/// structure assigned to the '__param_#name' variable (#name is the name of
/// param).
/// \param[in] Param Parameter name
/// \return Name of the global variable corresponding to the parameter
StringRef findParamVar(std::string Param, const Module *Mod);

/// Find names of all functions using the given parameter (global variable).
std::set<StringRef> getFunctionsUsingParam(std::string Param,
                                           std::vector<int> indices,
                                           const Module *Mod);
