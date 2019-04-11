//===------------------- Output.h - Reporting results ---------------------===//
//
//       SimpLL - Program simplifier for analysis of semantic difference      //
//
// This file is published under Apache 2.0 license. See LICENSE for details.
// Author: Viktor Malik, vmalik@redhat.com
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains classes for reporting results of the simplification.
/// Result is printed as a YAML to stdout.
/// Uses LLVM's YAML library.
///
//===----------------------------------------------------------------------===//

#include "Output.h"
#include <llvm/Support/YAMLTraits.h>

using namespace llvm::yaml;

// CallInfo to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<CallInfo> {
    static void mapping(IO &io, CallInfo &callinfo) {
        io.mapRequired("function", callinfo.fun);
        io.mapRequired("file", callinfo.file);
        io.mapRequired("line", callinfo.line);
    }
};
}

// CallStack (vector of CallInfo) to YAML
LLVM_YAML_IS_SEQUENCE_VECTOR(CallInfo);

// Info about a single function in a non-equal function pair
struct FunctionInfo {
    std::string name;
    std::string file;
    CallStack callstack;

    // Default constructor is needed for YAML serialisation so that the struct
    // can be used as an optional YAML field.
    FunctionInfo() {}
    FunctionInfo(const std::string &name,
                 const std::string &file,
                 const CallStack &callstack)
            : name(name), file(file), callstack(callstack) {}
};

// FunctionInfo to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<FunctionInfo> {
    static void mapping(IO &io, FunctionInfo &info) {
        io.mapRequired("function", info.name);
        io.mapOptional("file", info.file);
        io.mapOptional("callstack", info.callstack);
    }
};
}

// Pair of different functions that will be reported
typedef std::pair<FunctionInfo, FunctionInfo> DiffFunPair;

// DiffFunPair to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<DiffFunPair> {
    static void mapping(IO &io, DiffFunPair &funs) {
        io.mapOptional("first", funs.first);
        io.mapOptional("second", funs.second);
    }
};
}

// Vector of DiffFunPair to YAML
LLVM_YAML_IS_SEQUENCE_VECTOR(DiffFunPair);

// Pair of function names
typedef std::pair<std::string, std::string> MissingDefPair;

// MissingDefPair to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<MissingDefPair> {
    static void mapping(IO &io, MissingDefPair &funs) {
        io.mapOptional("first", funs.first);
        io.mapOptional("second", funs.second);
    }
};
}

// Overall report: contains pairs of different (non-equal) functions
struct ResultReport {
    std::vector<DiffFunPair> diffFuns;
    std::vector<MissingDefPair> missingDefs;
    std::vector<MacroDifference> differingMacros;
};

// Report to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<ResultReport> {
    static void mapping(IO &io, ResultReport &result) {
        io.mapOptional("diff-functions", result.diffFuns);
        io.mapOptional("missing-defs", result.missingDefs);
        io.mapOptional("differing-macros", result.differingMacros);
    }
};
}

// MacroElement to YAML (for stack in MacroDifference)
namespace llvm::yaml {
template<>
struct MappingTraits<MacroElement> {
    static void mapping(IO &io, MacroElement &result) {
        auto filename = result.source->getFile()->getFilename().str();
        io.mapRequired("name", result.name);
        io.mapRequired("body", result.body);
        io.mapRequired("file", filename);
        io.mapRequired("line", result.line);
    }
};
}

// Vector of MacroElement to YAML
LLVM_YAML_IS_SEQUENCE_VECTOR(MacroElement);

// MacroDifference to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<MacroDifference> {
    static void mapping(IO &io, MacroDifference &result) {
        io.mapRequired("name", result.macroName);
        io.mapRequired("function", result.functionName);
        io.mapRequired("left-stack", result.StackL);
        io.mapRequired("right-stack", result.StackR);
        io.mapRequired("left-line", result.lineLeft);
        io.mapRequired("right-line", result.lineRight);
    }
};
}

// Vector of MacroDifference to YAML
LLVM_YAML_IS_SEQUENCE_VECTOR(MacroDifference);

void reportOutput(Config &config,
                  std::vector<FunPair> &nonequalFuns,
                  std::vector<ConstFunPair> &missingDefs,
                  std::vector<MacroDifference> &differingMacros) {
    ResultReport report;
    for (auto &funPair : nonequalFuns) {
        report.diffFuns.emplace_back(
                FunctionInfo(funPair.first->getName(),
                             getFileForFun(funPair.first),
                             getCallStack(*config.FirstFun, *funPair.first)),
                FunctionInfo(funPair.second->getName(),
                             getFileForFun(funPair.second),
                             getCallStack(*config.SecondFun, *funPair.second))

        );
    }
    for (auto &funPair : missingDefs) {
        report.missingDefs.emplace_back(
                funPair.first ? funPair.first->getName() : "",
                funPair.second ? funPair.second->getName() : "");
    }
    report.differingMacros = differingMacros;

    llvm::yaml::Output output(outs());
    output << report;
}
