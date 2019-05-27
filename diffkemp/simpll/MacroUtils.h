//===---------- MacroUtils.h - Functions for working with macros -----------==//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Tomas Glozar, tglozar@gmail.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains declarations of functions working with macros and their
/// differences and structures used by them.
///
//===----------------------------------------------------------------------===//

#ifndef DIFFKEMP_SIMPLL_MACRO_UTILS_H
#define DIFFKEMP_SIMPLL_MACRO_UTILS_H

#include "Utils.h"

#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <string>
#include <unordered_map>

using namespace llvm;

/// Returned macros with value and location.
struct MacroElement {
    // The macro name is shortened, therefore it has to be stored as the whole
    // string, because otherwise the content would be dropped on leaving the
    // block in which the shortening is done.
    std::string name;
    // The body is in DebugInfo, parentMacro is a key in the map, therefore
    // both can be stored by reference.
    StringRef body, parentMacro;
    // This is the line in the C source code on which the macro is located.
    int line;
    std::string sourceFile;
};

/// Syntactic difference between objects that cannot be found in the original
/// source files.
/// Note: this can be either a macro difference or inline assembly difference.
struct SyntaxDifference {
	// Name of the object.
	std::string name;
	// The difference.
	std::string BodyL, BodyR;
	// Stacks containing the differing objects and all other objects affected
	// by the difference (again for both modules).
	CallStack StackL, StackR;
	// The function in which the difference was found
	std::string function;
};

/// Gets all macros used on the line in the form of a key to value map.
std::unordered_map<std::string, MacroElement> getAllMacrosOnLine(
    StringRef line, StringMap<StringRef> macroMap);

/// Gets all macros used on a certain DILocation in the form of a key to value
/// map.
std::unordered_map<std::string, MacroElement> getAllMacrosAtLocation(
    DILocation *LineLoc, const Module *Mod);

/// Finds macro differences at the locations of the instructions L and R and
/// return them as a vector.
/// This is used when a difference is suspected to be in a macro in order to
/// include that difference into ModuleComparator, and therefore avoid an
/// empty diff.
std::vector<SyntaxDifference> findMacroDifferences(
		const Instruction *L, const Instruction *R);

#endif // DIFFKEMP_SIMPLL_MACRO_UTILS_H
