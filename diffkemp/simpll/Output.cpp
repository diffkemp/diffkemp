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
    int line;
    CallStack callstack;
    bool isMacro;

    // Default constructor is needed for YAML serialisation so that the struct
    // can be used as an optional YAML field.
    FunctionInfo() {}
    FunctionInfo(const std::string &name,
                 const std::string &file,
                 const CallStack &callstack,
                 bool isMacro = false,
                 int line = 0)
            : name(name), file(file), line(line), callstack(callstack),
              isMacro(isMacro) {}
};

// Macro body
struct MacroBody {
    std::string name, LBody, RBody;
};

// MacroBody to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<MacroBody> {
    static void mapping(IO &io, MacroBody &body) {
        io.mapRequired("name", body.name);
        io.mapRequired("left-value", body.LBody);
        io.mapRequired("right-value", body.RBody);
    }
};
}

// Vector of MacroBody to YAML
LLVM_YAML_IS_SEQUENCE_VECTOR(MacroBody);

// FunctionInfo to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<FunctionInfo> {
    static void mapping(IO &io, FunctionInfo &info) {
        io.mapRequired("function", info.name);
        io.mapOptional("file", info.file);
        if (info.line != 0)
            io.mapOptional("line", info.line);
        io.mapOptional("callstack", info.callstack);
        io.mapRequired("is-macro", info.isMacro);
    }
};
}

// Pair of different functions that will be reported
struct DiffFunPair {
    FunctionInfo first, second;
};

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
    std::vector<MacroBody> macroDefinitions;
};

// Report to YAML
namespace llvm::yaml {
template<>
struct MappingTraits<ResultReport> {
    static void mapping(IO &io, ResultReport &result) {
        io.mapOptional("diff-functions", result.diffFuns);
        io.mapOptional("missing-defs", result.missingDefs);
        io.mapOptional("macro-defs", result.macroDefinitions);
    }
};
}

void reportOutput(Config &config,
                  std::vector<FunPair> &nonequalFuns,
                  std::vector<ConstFunPair> &missingDefs,
                  std::vector<SyntaxDifference> &differingMacros) {
    ResultReport report;
    for (auto &funPair : nonequalFuns) {
        report.diffFuns.push_back({
                FunctionInfo(funPair.first->getName(),
                             getFileForFun(funPair.first),
                             getCallStack(*config.FirstFun, *funPair.first),
                             false, funPair.first->getSubprogram() ?
                             funPair.first->getSubprogram()->getLine() : 0),
                FunctionInfo(funPair.second->getName(),
                             getFileForFun(funPair.second),
                             getCallStack(*config.SecondFun, *funPair.second),
                             false, funPair.second->getSubprogram() ?
                             funPair.second->getSubprogram()->getLine() : 0)
        });
    }
    for (auto &macroDiff : differingMacros) {
        // Try to append call stack of function to the macro stack if possible
        CallStack toAppendLeft, toAppendRight;
        for (auto &diff : report.diffFuns) {
            if (diff.first.name == macroDiff.function &&
                diff.first.callstack.size() > 0)
                toAppendLeft = diff.first.callstack;
            if (diff.second.name == macroDiff.function &&
                diff.second.callstack.size() > 0)
                toAppendRight = diff.second.callstack;
        }
        if (toAppendLeft.size() > 0)
            macroDiff.StackL.insert(macroDiff.StackL.begin(),
                toAppendLeft.begin(), toAppendLeft.end());
        if (toAppendRight.size() > 0)
            macroDiff.StackR.insert(macroDiff.StackR.begin(),
                toAppendRight.begin(), toAppendRight.end());

        report.diffFuns.push_back({
               FunctionInfo(macroDiff.name,
                            macroDiff.StackL[0].file,
                            macroDiff.StackL,
                            true),
               FunctionInfo(macroDiff.name,
                            macroDiff.StackR[0].file,
                            macroDiff.StackR,
                            true)
        });

        report.macroDefinitions.push_back(MacroBody {
            macroDiff.name, macroDiff.BodyL, macroDiff.BodyR
        });
    }
    for (auto &funPair : missingDefs) {
        report.missingDefs.emplace_back(
                funPair.first ? funPair.first->getName() : "",
                funPair.second ? funPair.second->getName() : "");
    }

    llvm::yaml::Output output(outs());
    output << report;
}
